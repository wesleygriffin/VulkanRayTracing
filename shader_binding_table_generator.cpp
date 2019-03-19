#include "shader_binding_table_generator.hpp"
#include <algorithm>
#include <cstring>

#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment) (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
#endif

VkDeviceSize ShaderBindingTableGenerator::ComputeSize(
  VkDeviceSize shaderGroupHandleSize) noexcept {
  Expects(shaderGroupHandleSize > 0);

  shaderGroupHandleSize_ = shaderGroupHandleSize;
  rayGenEntrySize_ = GetEntrySize(rayGenEntries_);
  missEntrySize_ = GetEntrySize(missEntries_);
  hitGroupEntrySize_ = GetEntrySize(hitGroupEntries_);

  sbtSize_ = rayGenEntrySize_ * rayGenEntries_.size() +
             missEntrySize_ * missEntries_.size() +
             hitGroupEntrySize_ * hitGroupEntries_.size();

  Ensures(shaderGroupHandleSize_ > 0);
  Ensures(rayGenEntrySize_ >= 0);
  Ensures(missEntrySize_ >= 0);
  Ensures(hitGroupEntrySize_ >= 0);
  Ensures(sbtSize_ > 0);

  return sbtSize_;
} // ShaderBindingTableGenerator::ComputeSize

VkResult ShaderBindingTableGenerator::Generate(VkDevice device,
                                               VkPipeline pipeline,
                                               VkDeviceMemory memory) {
  Expects(device != VK_NULL_HANDLE);
  Expects(pipeline != VK_NULL_HANDLE);
  Expects(memory != VK_NULL_HANDLE);
  Expects(shaderGroupHandleSize_ > 0);
  Expects(rayGenEntrySize_ >= 0);
  Expects(missEntrySize_ >= 0);
  Expects(hitGroupEntrySize_ >= 0);
  Expects(sbtSize_ > 0);

  std::fprintf(stderr, "RayGenStride: %zu\n", RayGenStride());
  std::fprintf(stderr, "RayGenSize: %zu\n", RayGenSize());
  std::fprintf(stderr, "MissOffset: %zu\n", MissOffset());
  std::fprintf(stderr, "MissStride: %zu\n", MissStride());
  std::fprintf(stderr, "MissSize: %zu\n", MissSize());
  std::fprintf(stderr, "HitGroupOffset: %zu\n", HitGroupOffset());
  std::fprintf(stderr, "HitGroupStride: %zu\n", HitGroupStride());
  std::fprintf(stderr, "HitGroupSize: %zu\n", HitGroupSize());

  std::uint32_t const groupCount = gsl::narrow_cast<std::uint32_t>(
    rayGenEntries_.size() + missEntries_.size() + hitGroupEntries_.size());
  std::fprintf(stderr, "groupCount: %d shaderGroupHandleSize_: %zu\n",
               groupCount, shaderGroupHandleSize_);

  std::vector<std::byte> shaderHandleStorage(groupCount *
                                             shaderGroupHandleSize_);
  if (auto result = vkGetRayTracingShaderGroupHandlesNV(
        device, pipeline, 0, groupCount, shaderHandleStorage.size(),
        shaderHandleStorage.data());
      result != VK_SUCCESS) {
    return result;
  }

  void* pBuffer;
  if (auto result = vkMapMemory(device, memory, 0, sbtSize_, 0, &pBuffer);
      result != VK_SUCCESS) {
    return result;
  }

  VkDeviceSize offset = 0;
  std::fprintf(stderr, "offset: %zu\n", offset);

  offset += CopyShaderData(
    device, pipeline, reinterpret_cast<std::byte*>(pBuffer) + offset,
    rayGenEntries_, rayGenEntrySize_, shaderHandleStorage.data());
  std::fprintf(stderr, "offset: %zu\n", offset);

  offset += CopyShaderData(
    device, pipeline, reinterpret_cast<std::byte*>(pBuffer) + offset,
    missEntries_, missEntrySize_, shaderHandleStorage.data());
  std::fprintf(stderr, "offset: %zu\n", offset);

  offset += CopyShaderData(
    device, pipeline, reinterpret_cast<std::byte*>(pBuffer) + offset,
    hitGroupEntries_, hitGroupEntrySize_, shaderHandleStorage.data());
  std::fprintf(stderr, "offset: %zu\n", offset);

  vkUnmapMemory(device, memory);
  return VK_SUCCESS;
} // ShaderBindingTableGenerator::Generate

VkDeviceSize ShaderBindingTableGenerator::GetEntrySize(
  gsl::span<SBTEntry> const& entries) noexcept {
  Expects(shaderGroupHandleSize_ > 0);

  auto maxArgIter =
    std::max_element(std::begin(entries), std::end(entries),
                     [](SBTEntry const& a, SBTEntry const& b) {
                       return a.inlineData.size() < b.inlineData.size();
                     });

  return ROUND_UP(shaderGroupHandleSize_ + maxArgIter->inlineData.size(), 16);
} // ShaderBindingTableGenerator::GetEntrySize

VkDeviceSize ShaderBindingTableGenerator::CopyShaderData(
  VkDevice device, VkPipeline pipeline, gsl::not_null<std::byte*> pOutput,
  gsl::span<SBTEntry> shaders, VkDeviceSize entrySize,
  gsl::not_null<std::byte const*> pShaderHandleStorage) noexcept {

  std::byte* pData = pOutput;
  for (auto&& shader : shaders) {
    std::memcpy(pData,
                pShaderHandleStorage.get() +
                  shader.groupIndex * shaderGroupHandleSize_,
                shaderGroupHandleSize_);

    std::fprintf(stderr, "groupIndex: %d handle: 0x", shader.groupIndex);
    for (int i = 0; i < shaderGroupHandleSize_; ++i) {
      std::fprintf(stderr, "%0x", pData[i]);
    }
    std::fprintf(stderr, "\n");

    if (!shader.inlineData.empty()) {
      std::memcpy(pData + shaderGroupHandleSize_, shader.inlineData.data(),
                  shader.inlineData.size());
    }

    pData += entrySize;
  }

  return shaders.size() * entrySize;
} // ShaderBindingTableGenerator::CopyShaderData

