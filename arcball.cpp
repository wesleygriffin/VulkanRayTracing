/*
 * Copyright (c) 2018 NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Modifications by Wesley Griffin <wesley.griffin@nist.gov>
 */

#include "arcball.hpp"
#include "glm/gtc/quaternion.hpp"

//------------------------------------------------------------------------------
//
// Arcball implementation
//
//------------------------------------------------------------------------------

glm::vec3 Arcball::toSphere(glm::vec2 const& v) const {
  float x = (v.x - m_center.x) / m_radius;
  float y = (1.0f - v.y - m_center.y) / m_radius;

  float z = 0.0f;
  float len2 = x * x + y * y;
  if (len2 > 1.0f) {
    // Project to closest point on edge of sphere.
    float len = sqrtf(len2);
    x /= len;
    y /= len;
  } else {
    z = sqrtf(1.0f - len2);
  }
  return glm::vec3(x, y, z);
}

glm::mat4 Arcball::rotate(glm::vec2 const& from, glm::vec2 const& to) const {
  glm::vec3 const a = toSphere(from);
  glm::vec3 const b = toSphere(to);

  glm::quat q(a, b);
  return glm::mat4_cast(glm::normalize(q));
}

