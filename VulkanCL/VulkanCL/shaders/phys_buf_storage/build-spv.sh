#! /bin/sh
export PATH=/Users/zenny-chen/programs/Open-Source-Projects/clspv/build/bin/Release:$PATH
clspv  buff_addr.cl -o buff_addr.spv --cl-std=CL1.2 --spv-version=1.3 --arch=spir64 --physical-storage-buffers

