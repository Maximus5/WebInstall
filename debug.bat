@echo off

cd /d "%~dp0"

if defined ConEmuPID (
  call ConEmuC -GuiMacro Progress 3
)

call "%VS140COMNTOOLS%vsvars32.bat"

cd

msbuild WebInstall.vcxproj /nologo /t:Build /p:Configuration=Debug;Platform=x86 /m:2

if defined ConEmuPID (
  call ConEmuC -GuiMacro Progress 0
)
