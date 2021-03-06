# VulkanRayTracing
Examples for using the real-time raytracing extensions in Vulkan.

Inspiration from:
- [Alexander Overvoorde's Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Peter Shirley's Ray Tracing in One Weekend](http://in1weekend.blogspot.com/2016/01/ray-tracing-in-one-weekend.html)

## Building

### Requirements
The following requirements must be met for these examples to compile.
- CMake 3.12 or higher
- Vulkan SDK 1.1.101 or higher
- Python 3.4 or higher
  - Wheezy Template: `pip install --user wheezy.template`

### Dependencies
The following packages are fetched and managed as part of the build.
- [g-truc/glm](https://github.com/g-truc/glm)
- [glfw/glfw](https://github.com/glfw/glfw)
- [GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [martinmoene/gsl-lite](https://github.com/martinmoene/gsl-lite)
- [mosra/flextgl](https://github.com/mosra/flextgl)
- [TartanLlama/expected](https://github.com/TartanLlama/expected)

## Other

### Developers
- Wesley Griffin wesley.griffin@nist.gov

### License
This software was developed by employees of the National Institute of
Standards and Technology (NIST), an agency of the Federal Government and is
being made available as a public service. Pursuant to title 17 United States
Code Section 105, works of NIST employees are not subject to copyright
protection in the United States.  This software may be subject to foreign
copyright.  Permission in the United States and in foreign countries, to the
extent that NIST may hold copyright, to use, copy, modify, create derivative
works, and distribute this software and its documentation without fee is
hereby granted on a non-exclusive basis, provided that this notice and
disclaimer of warranty appears in all copies. 

THE SOFTWARE IS PROVIDED 'AS IS' WITHOUT ANY WARRANTY OF ANY KIND, EITHER
EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, ANY WARRANTY
THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS, ANY IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND FREEDOM FROM
INFRINGEMENT, AND ANY WARRANTY THAT THE DOCUMENTATION WILL CONFORM TO THE
SOFTWARE, OR ANY WARRANTY THAT THE SOFTWARE WILL BE ERROR FREE.  IN NO EVENT
SHALL NIST BE LIABLE FOR ANY DAMAGES, INCLUDING, BUT NOT LIMITED TO, DIRECT,
INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR
IN ANY WAY CONNECTED WITH THIS SOFTWARE, WHETHER OR NOT BASED UPON WARRANTY,
CONTRACT, TORT, OR OTHERWISE, WHETHER OR NOT INJURY WAS SUSTAINED BY PERSONS
OR PROPERTY OR OTHERWISE, AND WHETHER OR NOT LOSS WAS SUSTAINED FROM, OR AROSE
OUT OF THE RESULTS OF, OR USE OF, THE SOFTWARE OR SERVICES PROVIDED HEREUNDER.
