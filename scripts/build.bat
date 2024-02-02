@echo off
setlocal

cd ..
if not exist build\win mkdir build\win
cd build\win
cmake ../..
if %errorlevel% neq 0 exit /b !errorlevel!

msbuild /property:Configuration="%1" firefly_synth.sln
if %errorlevel% neq 0 exit /b !errorlevel!

cd ..\..\dist\"%1"\win
plugin_base.ref_gen.exe firefly_synth_1.vst3\Contents\x86_64-win\firefly_synth_1.vst3 ..\..\..\PARAMREF.html
if %errorlevel% neq 0 exit /b !errorlevel!

cd ..\..\..\..\scripts