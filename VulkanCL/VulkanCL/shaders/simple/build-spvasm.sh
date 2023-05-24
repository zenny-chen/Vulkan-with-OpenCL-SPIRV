#! /bin/sh
# Configure your own VulkanSDK path here --
export PATH=/Users/zenny-chen/VulkanSDK/1.3.243.0/macOS/bin:$PATH
spirv-dis simple.spv  -o simple.spvasm
spirv-cross  --vulkan-semantics  --output simple.comp.glsl  simple.spv

