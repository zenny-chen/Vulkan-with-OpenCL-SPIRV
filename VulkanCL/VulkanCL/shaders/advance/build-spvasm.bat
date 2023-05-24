%VK_SDK_PATH%\Bin\spirv-dis advance.spv  -o advance.spvasm
%VK_SDK_PATH%\Bin\spirv-cross  --vulkan-semantics  --output advance.comp.glsl  advance.spv

