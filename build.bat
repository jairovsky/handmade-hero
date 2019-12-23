@echo off

set commonFlags=-DHANDMADE_INTERNAL=1 ^
   -DHANDMADE_SLOW_BUILD=1 ^
   -DHANDMADE_WIN32=1 ^
   -nologo -Oi -GR- -EHa- ^
   -W4 -WX -MT -Z7 ^
   -wd4201 -wd4100 -wd4189
set linkFlags=-opt:ref User32.lib Gdi32.lib Kernel32.lib Winmm.lib

rmdir /s /q build
mkdir build
pushd build

cl -DHANDMADE_EXPORTS=1 /LD %commonFlags% ../handmade.cpp
cl %commonFlags% ../win32_handmade.cpp /link %linkFlags%
popd

