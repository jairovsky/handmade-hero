@echo off

rmdir /s /q build
mkdir build
pushd build
cl -DHANDMADE_INTERNAL=1 ^
   -DHANDMADE_SLOW_BUILD=1 ^
   -DHANDMADE_WIN32=1 ^
   -nologo -Oi -GR- -EHa- ^
   -W4 -WX -wd4201 -wd4100 -wd4189 ^
   -MT ^
   -Z7 ../win32_handmade.cpp ^
   /link -subsystem:windows,5.1 ^
   User32.lib Gdi32.lib Kernel32.lib
popd

