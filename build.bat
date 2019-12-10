@echo off


IF NOT EXIST build mkdir build
pushd build
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW_BUILD=1 -DHANDMADE_WIN32=1 -Zi ../win32_handmade.cpp User32.lib Gdi32.lib Kernel32.lib
popd

