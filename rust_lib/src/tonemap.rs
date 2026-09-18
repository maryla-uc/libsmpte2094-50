/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Tone mapping functions according to SMPTE ST 2094-50 Section 6.

use crate::pchip::GainCurve;
use crate::utils::{ComponentMix, DynamicMetadata};

/// RGB pixel in gain application color space.
pub type Rgb = [f32; 3];

/// Evaluates component mixing according to SMPTE ST 2094-50 Section 6.4.
#[inline]
pub fn evaluate_component_mixing(mix: &ComponentMix, c: Rgb) -> Rgb {
    let max_c = c[0].max(c[1]).max(c[2]);
    let min_c = c[0].min(c[1]).min(c[2]);
    let luma = mix.rgb[0] * c[0] + mix.rgb[1] * c[1] + mix.rgb[2] * c[2];
    let base = mix.max * max_c + mix.min * min_c + luma;
    let ksum = mix.rgb[0] + mix.rgb[1] + mix.rgb[2] + mix.max + mix.min + mix.component;
    if ksum <= 0.0 {
        return [0.0, 0.0, 0.0];
    }
    let inv_ksum = 1.0 / ksum;
    [
        (base + mix.component * c[0]) * inv_ksum,
        (base + mix.component * c[1]) * inv_ksum,
        (base + mix.component * c[2]) * inv_ksum,
    ]
}

/// A contributing tone mapping rule representation.
#[derive(Clone, Debug)]
pub enum ActiveRule {
    /// Zero color gain function (applied at baseline HDR headroom).
    ZeroGain,
    /// Curve rule with associated component mixing and gain curve.
    Curve {
        mix: ComponentMix,
        curve: GainCurve,
        channels_share_gain: bool,
    },
}

impl Default for ActiveRule {
    fn default() -> Self {
        ActiveRule::ZeroGain
    }
}

impl ActiveRule {
    #[inline]
    pub fn evaluate(&self, c: Rgb) -> Rgb {
        match self {
            ActiveRule::ZeroGain => [0.0, 0.0, 0.0],
            ActiveRule::Curve { mix, curve, channels_share_gain } => {
                let m = evaluate_component_mixing(mix, c);
                if *channels_share_gain {
                    let g = curve.interpolate(m[0]);
                    [g, g, g]
                } else {
                    [
                        curve.interpolate(m[0]),
                        curve.interpolate(m[1]),
                        curve.interpolate(m[2]),
                    ]
                }
            }
        }
    }
}

/// Result structure for FFI calls creating a `ToneMapper`.
pub struct ToneMapperResult {
    pub mapper: ToneMapper,
    pub success: bool,
    pub error_message: String,
}

impl ToneMapperResult {
    pub fn get_error_message(&self) -> &str {
        &self.error_message
    }
}

/// Headroom-adaptive tone mapper context.
#[derive(Clone, Debug)]
pub struct ToneMapper {
    rule_i: ActiveRule,
    rule_j: ActiveRule,
    weight_i: f32,
    weight_j: f32,
    is_identity: bool,
}

impl Default for ToneMapper {
    fn default() -> Self {
        Self::identity()
    }
}

impl ToneMapper {
    /// Creates a no-op identity tone mapper.
    pub fn identity() -> Self {
        Self {
            rule_i: ActiveRule::ZeroGain,
            rule_j: ActiveRule::ZeroGain,
            weight_i: 1.0,
            weight_j: 0.0,
            is_identity: true,
        }
    }

    /// Creates a new `ToneMapper` from metadata and target HDR headroom (in log2).
    /// The target HDR headroom is the ratio between the display's maximum light
    /// level over SDR white light level.
    pub fn new(metadata: &DynamicMetadata, target_headroom_log2: f32) -> Result<Self, String> {
        if !metadata.is_valid() {
            return Err("Invalid metadata".to_string());
        }

        if !metadata.has_adaptive_tone_map_flag {
            // The specification doesn't say what to do in this case, any implementation
            // is valid.
            return Ok(Self::identity());
        }

        let mut meta = metadata.clone();
        meta.populate_implicit_parameters()?;

        #[derive(Clone, Copy)]
        struct RuleCandidate {
            headroom: f32,
            rule_index: Option<usize>, // None indicates implicit zero-gain baseline rule
        }

        let mut candidates = Vec::with_capacity(meta.rules.len() + 1);
        for (i, rule) in meta.rules.iter().enumerate() {
            candidates.push(RuleCandidate {
                headroom: rule.alternate_hdr_headroom_log2,
                rule_index: Some(i),
            });
        }
        candidates.push(RuleCandidate {
            headroom: meta.baseline_hdr_headroom_log2,
            rule_index: None,
        });

        candidates.sort_by(|a, b| a.headroom.partial_cmp(&b.headroom).unwrap_or(std::cmp::Ordering::Equal));

        if candidates.len() == 1 {
            return Ok(Self::identity());
        }

        // Binary search for bracketing rules.
        let mut altr_min = 0;
        let mut altr_max = candidates.len() - 1;
        while altr_max - altr_min > 1 {
            let mid = (altr_min + altr_max) / 2;
            if target_headroom_log2 <= candidates[mid].headroom {
                altr_max = mid;
            } else {
                altr_min = mid;
            }
        }

        let h_min = candidates[altr_min].headroom;
        let h_max = candidates[altr_max].headroom;

        let (weight_i, weight_j) = if h_max > h_min {
            let w_j = ((target_headroom_log2 - h_min) / (h_max - h_min)).clamp(0.0, 1.0);
            (1.0 - w_j, w_j)
        } else {
            (1.0, 0.0)
        };

        let create_active_rule = |cand: RuleCandidate| -> Result<ActiveRule, String> {
            match cand.rule_index {
                None => Ok(ActiveRule::ZeroGain),
                Some(idx) => {
                    let rule = &meta.rules[idx];
                    let x: Vec<f32> = rule.curve.iter().map(|p| p.x).collect();
                    let y: Vec<f32> = rule.curve.iter().map(|p| p.y).collect();
                    let slopes: Vec<f32> = rule.curve.iter().map(|p| p.m).collect();
                    let curve = GainCurve::create_with_slopes(x, y, slopes);
                    let channels_share_gain = rule.mix.component == 0.0;
                    Ok(ActiveRule::Curve {
                        mix: rule.mix,
                        curve,
                        channels_share_gain,
                    })
                }
            }
        };

        let rule_i = create_active_rule(candidates[altr_min])?;
        let rule_j = create_active_rule(candidates[altr_max])?;

        let is_identity = match (&rule_i, &rule_j) {
            (ActiveRule::ZeroGain, ActiveRule::ZeroGain) => true,
            (ActiveRule::ZeroGain, _) if weight_j == 0.0 => true,
            (_, ActiveRule::ZeroGain) if weight_i == 0.0 => true,
            _ => false,
        };

        Ok(Self {
            rule_i,
            rule_j,
            weight_i,
            weight_j,
            is_identity,
        })
    }

    /// Returns whether this tone mapper is a no-op identity transform.
    pub fn is_identity(&self) -> bool {
        self.is_identity
    }

    /// Tone maps a single RGB pixel. The RGB values must be in gain application
    /// color space, i.e. linear SDR-relative in the gain application
    /// chromaticities.
    #[inline]
    pub fn tone_map_pixel(&self, c: Rgb) -> Rgb {
        if self.is_identity {
            return c;
        }

        let mut log_gains = [0.0f32; 3];
        if self.weight_i > 0.0 {
            let g_i = self.rule_i.evaluate(c);
            log_gains[0] += self.weight_i * g_i[0];
            log_gains[1] += self.weight_i * g_i[1];
            log_gains[2] += self.weight_i * g_i[2];
        }
        if self.weight_j > 0.0 {
            let g_j = self.rule_j.evaluate(c);
            log_gains[0] += self.weight_j * g_j[0];
            log_gains[1] += self.weight_j * g_j[1];
            log_gains[2] += self.weight_j * g_j[2];
        }

        [
            c[0] * log_gains[0].exp2(),
            c[1] * log_gains[1].exp2(),
            c[2] * log_gains[2].exp2(),
        ]
    }

    /// In-place tone maps an interleaved float RGB buffer.
    pub fn tone_map_buffer(&self, buffer: &mut [f32]) -> Result<(), String> {
        if buffer.len() % 3 != 0 {
            return Err(format!(
                "Buffer length must be a multiple of 3 (got {})",
                buffer.len()
            ));
        }

        if self.is_identity {
            return Ok(());
        }

        for chunk in buffer.chunks_exact_mut(3) {
            let rgb = [chunk[0], chunk[1], chunk[2]];
            let mapped = self.tone_map_pixel(rgb);
            chunk[0] = mapped[0];
            chunk[1] = mapped[1];
            chunk[2] = mapped[2];
        }

        Ok(())
    }
}

/// Convenience function to create a ToneMapperResult for FFI.
pub fn tone_mapper_create_ffi(
    metadata: &DynamicMetadata,
    target_headroom_log2: f32,
) -> ToneMapperResult {
    match ToneMapper::new(metadata, target_headroom_log2) {
        Ok(mapper) => ToneMapperResult {
            mapper,
            success: true,
            error_message: String::new(),
        },
        Err(e) => ToneMapperResult {
            mapper: ToneMapper::identity(),
            success: false,
            error_message: e,
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::utils::{ControlPoint, ToneMappingRule};
    use googletest::prelude::*;

    const EPSILON: f32 = 1e-4;

    fn make_base_metadata() -> DynamicMetadata {
        DynamicMetadata {
            hdr_reference_white: 203.0,
            has_adaptive_tone_map_flag: true,
            baseline_hdr_headroom_log2: 0.0,
            gain_application_space_chromaticities: [
                0.708, 0.292, 0.17, 0.797, 0.131, 0.046, 0.3127, 0.329,
            ],
            ..Default::default()
        }
    }

    #[gtest]
    fn test_component_mixing_preserves_neutral_gray() {
        let mix = ComponentMix {
            rgb: [0.2, 0.7, 0.1],
            max: 0.5,
            min: 0.1,
            component: 0.1,
        };
        // By definition in Section 6.4.1, component mixing on neutral gray values is identity.
        let gray_sub = [0.42, 0.42, 0.42];
        let mixed_sub = evaluate_component_mixing(&mix, gray_sub);
        assert!((mixed_sub[0] - 0.42).abs() < EPSILON);
        assert!((mixed_sub[1] - 0.42).abs() < EPSILON);
        assert!((mixed_sub[2] - 0.42).abs() < EPSILON);

        let gray_hdr = [2.5, 2.5, 2.5];
        let mixed_hdr = evaluate_component_mixing(&mix, gray_hdr);
        assert!((mixed_hdr[0] - 2.5).abs() < EPSILON);
        assert!((mixed_hdr[1] - 2.5).abs() < EPSILON);
        assert!((mixed_hdr[2] - 2.5).abs() < EPSILON);
    }

    #[gtest]
    fn test_component_mixing_with_component_weight() {
        let mix = ComponentMix {
            rgb: [0.3, 0.6, 0.1],
            max: 0.0,
            min: 0.0,
            component: 0.5,
        };
        // For c = [1.0, 0.0, 0.0]:
        // luma = 0.3 * 1.0 = 0.3
        // base = 0.3
        // ksum = 0.3 + 0.6 + 0.1 + 0 + 0 + 0.5 = 1.5
        // m[0] = (0.3 + 0.5 * 1.0) / 1.5 = 0.8 / 1.5 ≈ 0.53333336
        // m[1] = (0.3 + 0.5 * 0.0) / 1.5 = 0.3 / 1.5 = 0.2
        // m[2] = (0.3 + 0.5 * 0.0) / 1.5 = 0.3 / 1.5 = 0.2
        let c = [1.0, 0.0, 0.0];
        let m = evaluate_component_mixing(&mix, c);
        assert!((m[0] - 0.8 / 1.5).abs() < EPSILON);
        assert!((m[1] - 0.2).abs() < EPSILON);
        assert!((m[2] - 0.2).abs() < EPSILON);
    }

    #[gtest]
    fn test_tonemapper_smoke() {
        let mut agtm = make_base_metadata();
        agtm.baseline_hdr_headroom_log2 = 0.0;

        let mut rule = ToneMappingRule::default();
        rule.alternate_hdr_headroom_log2 = 2.0;
        rule.use_pchip_slope = true;
        rule.mix = ComponentMix {
            rgb: [0.0, 0.0, 0.0],
            max: 0.0,
            min: 0.0,
            component: 1.0,
        };
        rule.curve = vec![
            ControlPoint { x: 0.0, y: 1.0, m: 0.0 },
            ControlPoint { x: 64.0, y: 1.0, m: 0.0 },
        ];
        agtm.rules.push(rule);

        // Target headroom 1.0: halfway between baseline 0.0 and alternate 2.0 (gain = 0.5 in log2).
        // Expected output = 1.0 * 2^0.5 ≈ 1.41421356.
        let mapper = ToneMapper::new(&agtm, 1.0).unwrap();
        let out = mapper.tone_map_pixel([1.0, 1.0, 1.0]);
        assert!((out[0] - 1.41421356).abs() < EPSILON);

        let mut buffer = vec![1.0, 1.0, 1.0];
        mapper.tone_map_buffer(&mut buffer).unwrap();
        assert!((buffer[0] - 1.41421356).abs() < EPSILON);
    }
}
