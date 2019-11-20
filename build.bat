@echo off


mkdir build
pushd build
cl -Zi ../winmain.cpp User32.lib Gdi32.lib Kernel32.lib
popd

