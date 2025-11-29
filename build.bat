@echo off

cl -nologo -W4 -Zi -wd4100 -wd4189 -Iext\SDL -Oi -O2 src\main.c src\cie_xyz_lut.c ext\SDL\debug\SDL3.lib -link /SUBSYSTEM:CONSOLE
