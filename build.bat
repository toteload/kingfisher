@echo off

where cl >nul 2>&1
if %errorlevel% equ 1 (
  call initcl.bat
)

set LIBS=ext\SDL\debug\SDL3.lib ext\embree-4.4.0\lib\embree4.lib ext\embree-4.4.0\lib\tbb12.lib
set OPTIONS=/nologo /W4 /Zi /wd4201 /wd4100 /wd4189 /Iext /Iext\SDL /Iext\embree-4.4.0\include /Oi /O2 /arch:AVX512

cl %OPTIONS% /c src\main.c /Fo:main.obj
cl %OPTIONS% /c src\cie_xyz_lut.c /Fo:cie_xyz_lut.obj
cl %OPTIONS% /c src\bvh.c /Fo:bvh.obj
cl %OPTIONS% /c src\aabb.c /Fo:aabb.obj
cl %OPTIONS% /c src\camera.c /Fo:camera.obj

cl /nologo /Fe:kingfisher /Zi main.obj camera.obj aabb.obj cie_xyz_lut.obj bvh.obj %LIBS% /link /SUBSYSTEM:CONSOLE
