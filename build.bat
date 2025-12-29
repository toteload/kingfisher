@echo off

where cl >nul 2>&1
if %errorlevel% equ 1 (
  call initcl.bat
)

python build.py
