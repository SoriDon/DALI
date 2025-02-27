// Copyright (c) 2019-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <tuple>

#include "dali/core/error_handling.h"
#include "dali/operators/decoder/image/image_factory.h"
#include "dali/operators/decoder/host/fused/host_decoder_crop.h"
#include "dali/pipeline/operator/common.h"

namespace dali {

HostDecoderCrop::HostDecoderCrop(const OpSpec &spec)
  : HostDecoder(spec)
  , CropAttr(spec) {
}

DALI_REGISTER_OPERATOR(legacy__decoders__ImageCrop, HostDecoderCrop, CPU);

}  // namespace dali
