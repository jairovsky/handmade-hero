@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

DOSKEY ga=git add $*
DOSKEY gd=git diff $*
DOSKEY gdca=git diff --cached $*
DOSKEY grh=git reset
DOSKEY glg=git log $*
DOSKEY gsb=git status
DOSKEY gc=git commit $*
DOSKEY gc!=git commit --amend $*