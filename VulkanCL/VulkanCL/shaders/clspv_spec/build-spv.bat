:: Configure your own clspv.exe path here --
set PATH=C:\Open-Source-Projects\clspv\build\bin\Release;%PATH%
clspv  clspv_spec.cl -o clspv_spec.spv --cl-std=CL1.2 --spv-version=1.3 --arch=spir64

