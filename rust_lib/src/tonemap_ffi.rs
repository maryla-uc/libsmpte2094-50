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
// CXX bridge for tonemap_rs.

#[cxx::bridge(namespace = "tonemap_ffi")]
pub mod ffi {
    #[derive(Clone, Debug)]
    struct FfiControlPoint {
        x: f32,
        y: f32,
        m: f32,
    }

    #[derive(Clone, Debug)]
    struct FfiComponentMix {
        rgb: [f32; 3],
        max: f32,
        min: f32,
        component: f32,
    }

    #[derive(Clone, Debug)]
    struct FfiToneMappingRule {
        alternate_hdr_headroom_log2: f32,
        use_pchip_slope: bool,
        mix: FfiComponentMix,
        curve: Vec<FfiControlPoint>,
    }

    #[derive(Clone, Debug)]
    struct FfiDynamicMetadata {
        has_adaptive_tone_map_flag: bool,
        use_reference_white_tone_mapping_flag: bool,
        hdr_reference_white: f32,
        baseline_hdr_headroom_log2: f32,
        gain_application_space_chromaticities: [f32; 8],
        rules: Vec<FfiToneMappingRule>,
    }

    struct ToneMapperCreationResult {
        success: bool,
        error_message: String,
        mapper: Box<ToneMapperWrapper>,
    }

    struct ToneMapperBufferResult {
        success: bool,
        error_message: String,
    }

    extern "Rust" {
        type ToneMapperWrapper;
        #[cxx_name = "is_identity"]
        fn is_identity(self: &ToneMapperWrapper) -> bool;
        #[cxx_name = "tone_map_pixel"]
        fn tone_map_pixel(self: &ToneMapperWrapper, r: f32, g: f32, b: f32) -> [f32; 3];
        #[cxx_name = "tone_map_buffer"]
        fn tone_map_buffer(self: &ToneMapperWrapper, buffer: &mut [f32]) -> ToneMapperBufferResult;
        #[cxx_name = "clone"]
        fn clone_box(self: &ToneMapperWrapper) -> Box<ToneMapperWrapper>;

        fn tone_mapper_create_ffi(
            metadata: &FfiDynamicMetadata,
            target_headroom_log2: f32,
        ) -> ToneMapperCreationResult;
    }
}

pub struct ToneMapperWrapper {
    inner: crate::tonemap::ToneMapper,
}

impl ToneMapperWrapper {
    pub fn is_identity(&self) -> bool {
        self.inner.is_identity()
    }

    pub fn tone_map_pixel(&self, r: f32, g: f32, b: f32) -> [f32; 3] {
        self.inner.tone_map_pixel([r, g, b])
    }

    pub fn tone_map_buffer(&self, buffer: &mut [f32]) -> ffi::ToneMapperBufferResult {
        match self.inner.tone_map_buffer(buffer) {
            Ok(()) => ffi::ToneMapperBufferResult {
                success: true,
                error_message: String::new(),
            },
            Err(e) => ffi::ToneMapperBufferResult {
                success: false,
                error_message: e,
            },
        }
    }

    pub fn clone_box(&self) -> Box<ToneMapperWrapper> {
        Box::new(ToneMapperWrapper {
            inner: self.inner.clone(),
        })
    }
}

impl From<&ffi::FfiControlPoint> for crate::utils::ControlPoint {
    fn from(p: &ffi::FfiControlPoint) -> Self {
        Self {
            x: p.x,
            y: p.y,
            m: p.m,
        }
    }
}

impl From<&ffi::FfiComponentMix> for crate::utils::ComponentMix {
    fn from(m: &ffi::FfiComponentMix) -> Self {
        Self {
            rgb: m.rgb,
            max: m.max,
            min: m.min,
            component: m.component,
        }
    }
}

impl From<&ffi::FfiToneMappingRule> for crate::utils::ToneMappingRule {
    fn from(r: &ffi::FfiToneMappingRule) -> Self {
        Self {
            alternate_hdr_headroom_log2: r.alternate_hdr_headroom_log2,
            use_pchip_slope: r.use_pchip_slope,
            mix: (&r.mix).into(),
            curve: r.curve.iter().map(|p| p.into()).collect(),
        }
    }
}

impl From<&ffi::FfiDynamicMetadata> for crate::utils::DynamicMetadata {
    fn from(m: &ffi::FfiDynamicMetadata) -> Self {
        Self {
            has_adaptive_tone_map_flag: m.has_adaptive_tone_map_flag,
            use_reference_white_tone_mapping_flag: m.use_reference_white_tone_mapping_flag,
            hdr_reference_white: m.hdr_reference_white,
            baseline_hdr_headroom_log2: m.baseline_hdr_headroom_log2,
            gain_application_space_chromaticities: m.gain_application_space_chromaticities,
            rules: m.rules.iter().map(|r| r.into()).collect(),
        }
    }
}

pub fn tone_mapper_create_ffi(
    metadata: &ffi::FfiDynamicMetadata,
    target_headroom_log2: f32,
) -> ffi::ToneMapperCreationResult {
    let rs_meta: crate::utils::DynamicMetadata = metadata.into();
    match crate::tonemap::ToneMapper::new(&rs_meta, target_headroom_log2) {
        Ok(mapper) => ffi::ToneMapperCreationResult {
            success: true,
            error_message: String::new(),
            mapper: Box::new(ToneMapperWrapper { inner: mapper }),
        },
        Err(e) => ffi::ToneMapperCreationResult {
            success: false,
            error_message: e,
            mapper: Box::new(ToneMapperWrapper {
                inner: crate::tonemap::ToneMapper::identity(),
            }),
        },
    }
}
