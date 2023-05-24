%VK_SDK_PATH%\Bin\spirv-dis simple.spv  -o simple.spvasm
%VK_SDK_PATH%\Bin\spirv-cross  --vulkan-semantics  --output simple.comp.glsl  simple.spv

