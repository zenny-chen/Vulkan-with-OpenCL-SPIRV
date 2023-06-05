# Vulkan-with-OpenCL-SPIRV
Vulkan Compute Shader with OpenCL-C to SPIR-V

<br />

This project is built with Visual Studio 2022 community edition. To run it, a [Vulkan SDK](https://vulkan.lunarg.com/) is required.

If you want to build the OpenCL-C source into SPV destinations, please refer to this repository: [clspv](https://github.com/google/clspv)

The SPV codes for **SimpleComputeTest** and **AdvancedComputeTest** have an equivalent to GLSL syntax. So they can be disassembled to GLSL code.

While the SPV code for **CLSPVSpecComputeTest** and **BufferAddressComputeTest** are specific for SPIR-V and OpenCL features which can not be found on GLSL. So it cannot be disassembled to GLSL code.

