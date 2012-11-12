@echo off
cls
mkdir ndll\Windows 2> NUL
pushd %~dp0\project
haxelib run hxcpp Build.xml -Dwindows
popd