@echo off


mkdir build
pushd build
cl -DHANDMADE_WIN32=1 -Zi ../win32_handmade.cpp User32.lib Gdi32.lib Kernel32.lib
popd

