pushd ../project/libogg

## LIBOGG
#cd ../libogg && \
#./configure && \
#make clean && \
#make && \
#make install && \
#cp ./src/.libs/libogg.a ../../libs/Windows/ogg.lib && \
#
## LIBVORBIS
#cd ../libvorbis
#./configure --with-ogg=/local --disable-oggtest --disable-examples --disable-docs && \
#make clean && \
#make && \
#make install && \
#cp ./lib/.libs/libvorbis.a ../../libs/Windows/vorbis.lib && \
#
## LIBWEBM
#cd ../libwebm && \
#./configure && \
#make clean && \
#make && \
#cp ./libwebm.a ../../libs/Windows/webm.lib && \

# LIBVPX
cd ../libvpx && \
./configure --disable-examples --disable-docs --disable-unit-tests --disable-vp8-encoder && \
make clean && \
make && \
make install && \
cp ./libvpx.a ../../libs/Windows/vpx.lib && \

popd