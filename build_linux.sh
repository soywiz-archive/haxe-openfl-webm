mkdir ndll/Mac 2> /dev/null
pushd project
haxelib run hxcpp Build.xml -Dlinux
popd