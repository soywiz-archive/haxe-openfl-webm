mkdir Mac

pushd ../libs.src/libvpx

export CC="gcc -m32"
export AS="yasm -a x86" 
export MACOSX_DEPLOYMENT_TARGET=10.8

## LIBOGG
#cd ../libogg && \
#./configure && \
#make clean && \
#make && \
##sudo make install && \
#cp ./src/.libs/libogg.a ../../libs/Mac/libogg.a && \
#
## LIBVORBIS
#cd ../libvorbis
#./configure --with-ogg=/local --disable-oggtest --disable-examples --disable-docs && \
#make clean && \
#make && \
##sudo make install && \
#cp ./lib/.libs/libvorbis.a ../../libs/Mac/libvorbis.a && \
#
## LIBWEBM
#cd ../libwebm && \
#./configure && \
#make clean && \
#make && \
#cp ./libwebm.a ../../libs/Mac/libwebm.a && \

# LIBVPX
cd ../libvpx && \
yasm --version && \
#./configure --target=x86-darwin9-gcc --as=yasm --disable-examples --disable-docs --disable-unit-tests --disable-vp8-encoder && \
./configure --disable-examples --disable-docs --disable-unit-tests --disable-vp8-encoder && \
make clean && \
make && \
#sudo make install && \
cp ./libvpx.a ../../libs/Mac/libvpx.a	 && \

popd
