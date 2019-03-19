#ifndef SHADER_BINDING_TABLE_GENERATOR_HPP_
#define SHADER_BINDING_TABLE_GENERATOR_HPP_

/*-----------------------------------------------------------------------
Copyright (c) 2014-2018, NVIDIA. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Neither the name of its contributors may be used to endorse
or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/

/*
 * Modifications by Wesley Griffin <wesley.griffin@nist.gov>
 */

#include "flextVk.h"
#include "gsl/gsl-lite.hpp"
#include <vector>

class ShaderBindingTableGenerator {
public:
  void AddRayGen(std::uint32_t groupIndex,
                 gsl::span<std::byte> inlineData = {}) {
    rayGenEntries_.emplace_back(
      groupIndex,
      std::vector<std::byte>(std::begin(inlineData), std::end(inlineData)));
  }

  void AddMiss(std::uint32_t groupIndex, gsl::span<std::byte> inlineData = {}) {
    missEntries_.emplace_back(
      groupIndex,
      std::vector<std::byte>(std::begin(inlineData), std::end(inlineData)));
  }

  void AddHitGroup(std::uint32_t groupIndex,
                   gsl::span<std::byte> inlineData = {}) {
    hitGroupEntries_.emplace_back(
      groupIndex,
      std::vector<std::byte>(std::begin(inlineData), std::end(inlineData)));
  }

  VkDeviceSize ComputeSize(VkDeviceSize shaderGroupHandleSize) noexcept;

  VkDeviceSize RayGenStride() const noexcept { return rayGenEntrySize_; }
  VkDeviceSize RayGenSize() const noexcept {
    return RayGenStride() * rayGenEntries_.size();
  }

  VkDeviceSize MissOffset() const noexcept { return RayGenSize(); }
  VkDeviceSize MissStride() const noexcept { return missEntrySize_; }
  VkDeviceSize MissSize() const noexcept {
    return MissStride() * missEntries_.size();
  }

  VkDeviceSize HitGroupOffset() const noexcept {
    return RayGenSize() + MissSize();
  }
  VkDeviceSize HitGroupStride() const noexcept { return hitGroupEntrySize_; }
  VkDeviceSize HitGroupSize() const noexcept {
    return HitGroupStride() * hitGroupEntries_.size();
  }

  VkResult Generate(VkDevice device, VkPipeline pipeline, VkBuffer buffer,
                    VkDeviceMemory memory);

  struct SBTEntry {
    std::uint32_t groupIndex;
    std::vector<std::byte> inlineData;

    SBTEntry(std::uint32_t index, std::vector<std::byte> data)
      : groupIndex(index)
      , inlineData(std::move(data)) {}
  }; // struct SBTEntry

private:
  std::vector<SBTEntry> rayGenEntries_{};
  std::vector<SBTEntry> missEntries_{};
  std::vector<SBTEntry> hitGroupEntries_{};

  VkDeviceSize shaderGroupHandleSize_{0};
  VkDeviceSize rayGenEntrySize_{0};
  VkDeviceSize missEntrySize_{0};
  VkDeviceSize hitGroupEntrySize_{0};
  VkDeviceSize sbtSize_{0};

  VkDeviceSize GetEntrySize(gsl::span<SBTEntry> const& entries) noexcept;

  VkDeviceSize
  CopyShaderData(VkDevice device, VkPipeline pipeline,
                 gsl::not_null<std::byte*> pOutput, gsl::span<SBTEntry> shaders,
                 VkDeviceSize entrySize,
                 gsl::not_null<std::byte const*> pShaderHandleStorage) noexcept;
}; // class ShaderBindingTableGenerator

#endif // SHADER_BINDING_TABLE_GENERATOR_HPP_
