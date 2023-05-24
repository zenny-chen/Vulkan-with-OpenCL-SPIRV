#! /bin/sh
# Configure your own VulkanSDK path here --
export PATH=/Users/zenny-chen/VulkanSDK/1.3.243.0/macOS/bin:$PATH
spirv-dis advance.spv  -o advance.spvasm
spirv-cross  --vulkan-semantics  --output advance.comp.glsl  advance.spv

