C:\VulkanSDK\1.2.135.0\Bin\glslangValidator.exe -V shader.frag
C:\VulkanSDK\1.2.135.0\Bin\glslangValidator.exe -V shader.vert
xcopy .\*.spv ..\..\x64\Release\Shaders\ /S /Y
xcopy .\*.spv ..\..\x64\Debug\Shaders\ /S /Y

pause