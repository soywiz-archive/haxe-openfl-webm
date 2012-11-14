mkdir ndll/iPhone 2> /dev/null
pushd project
IPHONE_VER=6.0 haxelib run hxcpp Build.xml -Diphonesim
IPHONE_VER=6.0 haxelib run hxcpp Build.xml -Diphoneos
popd
