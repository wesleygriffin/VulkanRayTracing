include(FetchContent)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set_target_properties(Python3::Interpreter PROPERTIES IMPORTED_GLOBAL TRUE)

find_package(Vulkan 1.1.101.0 REQUIRED)
set_target_properties(Vulkan::Vulkan PROPERTIES IMPORTED_GLOBAL TRUE)

get_filename_component(Vulkan_LIBRARY_DIR ${Vulkan_LIBRARY} DIRECTORY CACHE)
get_filename_component(Vulkan_SDK_DIR ${Vulkan_LIBRARY_DIR}/.. ABSOLUTE CACHE)

find_program(GlslangValidator_EXECUTABLE glslangValidator
  PATHS ${Vulkan_SDK_DIR}/bin
)
if(NOT GlslangValidator_EXECUTABLE)
  message(FATAL_ERROR "Cannot locate glslangValidator")
endif()

FetchContent_Declare(gsllite
  GIT_REPOSITORY https://github.com/martinmoene/gsl-lite
  GIT_TAG v0.33.0
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(gsllite)
if(NOT gsllite_POPULATED)
  FetchContent_Populate(gsllite)
  add_subdirectory(${gsllite_SOURCE_DIR} ${gsllite_BINARY_DIR})
endif()

set(EXPECTED_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
set(EXPECTED_ENABLE_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(expected
  GIT_REPOSITORY https://github.com/TartanLlama/expected
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(expected)
if(NOT expected_POPULATED)
  FetchContent_Populate(expected)
  add_subdirectory(${expected_SOURCE_DIR} ${expected_BINARY_DIR})
endif()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.2.1
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(glfw)
if(NOT glfw_POPULATED)
  FetchContent_Populate(glfw)
  add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})
endif()

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm
  GIT_TAG 0.9.9.3
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
  FetchContent_Populate(glm)
  add_subdirectory(${glm_SOURCE_DIR} ${glm_BINARY_DIR})
endif()

FetchContent_Declare(flextGL
  GIT_REPOSITORY https://github.com/mosra/flextgl
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(flextgl)
if(NOT flextgl_POPULATED)
  FetchContent_Populate(flextgl)
endif()

set(FLEXTGL_SOURCE_DIR ${flextgl_SOURCE_DIR} CACHE STRING "" FORCE)

FetchContent_Declare(vulkanmemoryallocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v2.2.0
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(vulkanmemoryallocator)
if(NOT vulkanmemoryallocator_POPULATED)
  FetchContent_Populate(vulkanmemoryallocator)
endif()

if(NOT EXISTS ${vulkanmemoryallocator_BINARY_DIR}/vk_mem_alloc.cc)
  file(WRITE ${vulkanmemoryallocator_BINARY_DIR}/vk_mem_alloc.cc
    "#define VMA_IMPLEMENTATION\n#include \"vk_mem_alloc.h\""
  )
endif()

add_library(vma ${vulkanmemoryallocator_BINARY_DIR}/vk_mem_alloc.cc)
target_include_directories(vma
  PUBLIC
    ${vulkanmemoryallocator_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIR}
)
target_link_libraries(vma PUBLIC Vulkan::Vulkan)
target_compile_definitions(vma
  PUBLIC
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)
