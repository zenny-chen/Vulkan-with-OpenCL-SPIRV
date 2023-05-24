#! /bin/sh
# Configure your own clspv executable path here --
export PATH=/Users/zenny-chen/programs/Open-Source-Projects/clspv/build/bin/Release:$PATH
clspv  simple.cl -o simple.spv --cl-std=CL1.2 --spv-version=1.3 --arch=spir64

