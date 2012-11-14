@echo off
cls
mkdir ndll\Android 2> NUL
pushd %~dp0\project
haxelib run hxcpp Build.xml -Dandroid
popd