// ======================================================================== //
// Copyright 2019-2020 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

// owl stuff that we use throughout this library
#include <owl/common/math/AffineSpace.h>
#include "owl/common/parallel/parallel_for.h"
// the pbrt parser scene graph we're operating on
#include "pbrtParser/Scene.h"
#include "owl/common/math/box.h"

namespace cup {

  using namespace owl;
  using namespace owl::common;

#ifndef __CUDACC__
  typedef owl::vec2f float2;
  typedef owl::vec3f float3;
  typedef owl::vec4f float4;
#endif
}
