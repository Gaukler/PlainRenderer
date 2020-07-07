@echo off
for /r %%i in (*) do (
 if %%~xi == .comp (
 C:\VulkanSDK\1.2.135.0\Bin\glslc.exe %%i -o %%~ni_comp.spirv
)
 if %%~xi == .vert (
 C:\VulkanSDK\1.2.135.0\Bin\glslc.exe %%i -o %%~ni_vert.spirv
)
 if %%~xi == .frag (
 C:\VulkanSDK\1.2.135.0\Bin\glslc.exe %%i -o %%~ni_frag.spirv
)
)
pause