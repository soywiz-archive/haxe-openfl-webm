mkdir Linux

pushd ../libs.src/libvpx

## LIBOGG
#cd ../libogg && \
#./configure && \
#make clean && \
#make && \
##sudo make install && \
#cp ./src/.libs/libogg.a ../../libs/Linux/libogg.a && \
#
## LIBVORBIS
#cd ../libvorbis
#./configure --with-ogg=/local --disable-oggtest --disable-examples --disable-docs && \
#make clean && \
#make && \
##sudo make install && \
#cp ./lib/.libs/libvorbis.a ../../libs/Linux/libvorbis.a && \
#
## LIBWEBM
#cd ../libwebm && \
#./configure && \
#make clean && \
#make && \
#cp ./libwebm.a ../../libs/Linux/libwebm.a && \

# LIBVPX
cd ../libvpx && \
./configure --disable-examples --disable-docs --disable-unit-tests --disable-vp8-encoder && \
make clean && \
make && \
#sudo make install && \
cp ./libvpx.a ../../libs/Linux/libvpx.a	 && \

popd
