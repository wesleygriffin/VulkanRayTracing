cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

project(VulkanRayTracing VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory(third_party)

add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/flextVk.h
    ${CMAKE_CURRENT_BINARY_DIR}/flextVk.cpp
  COMMAND
    ${Python3_EXECUTABLE} ${FLEXTGL_SOURCE_DIR}/flextGLgen.py
      -D ${CMAKE_CURRENT_BINARY_DIR}
      -t ${CMAKE_CURRENT_SOURCE_DIR}/flext/templates/vulkan
      $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/flext/vk_win32_khr.txt>
      $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_SOURCE_DIR}/flext/vk_xcb_khr.txt>
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/flext/templates/vulkan/flextVk.h.template
    ${CMAKE_CURRENT_SOURCE_DIR}/flext/templates/vulkan/flextVk.cpp.template
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/flext/vk_win32_khr.txt>
    $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_SOURCE_DIR}/flext/vk_xcb_khr.txt>
)

set(COMMON_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/flextVk.cpp
)

add_executable(01_sphere 01_sphere.cpp ${COMMON_SOURCES})
target_compile_features(01_sphere PRIVATE cxx_std_17)
target_compile_definitions(01_sphere
  PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_EXPLICIT_CTOR GLM_FORCE_INLINE
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
    $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
)
target_compile_options(01_sphere
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive- /Zc:__cplusplus>
)
target_include_directories(01_sphere PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(01_sphere PRIVATE glfw glm vma gsl-lite expected)
