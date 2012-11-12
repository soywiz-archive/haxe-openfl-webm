@echo off
pushd %~dp0
del deploy.zip 2> NUL
"%~dp0\tools\7z" a -tzip -mx=9 deploy.zip -r -x!.git -x!obj -x!project/obj -xr!*.pdb -xr!all_objs -x!tools -x!deploy.bat -x!.DS_Store
haxelib submit deploy.zip
del deploy.zip 2> NUL
popd