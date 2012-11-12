#ifndef HX_WEBP_HEADER
#define HX_WEBP_HEADER

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include <hxcpp.h>
//#include <hx/Macros.h>
#include <hx/CFFI.h>
//#include <hx/CFFIAPI.h>

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

typedef uint64 uint64_t;

typedef struct {
	value func_read;
	value func_seek;
	value func_tell;
} io_struct;

typedef struct HxWebmContext {
	vpx_codec_ctx_t context;
	vpx_codec_err_t res;
	vpx_codec_iter_t iter;
	vpx_codec_stream_info_t stream_info;
	//int frameSize;
	//unsigned char    frameData[256*1024];
} HxWebmContext;


extern const char *_vpx_codec_iface_name();
extern vpx_codec_err_t _vpx_codec_dec_init(HxWebmContext *codecPointer);
extern void _vpx_codec_destroy(HxWebmContext *codecPointer);
extern vpx_codec_err_t _vpx_codec_decode(HxWebmContext *codecPointer, char *framePointer, int frameSize);
extern vpx_image_t *_vpx_codec_get_frame(HxWebmContext *codecPointer);
extern int test_decode_main(const char *infileName, const char* outfileName);
extern void YUV420toRGBA(uint8* Y, uint8* U, uint8* V, int words_per_line, int width, int height, uint32* pixdata, int y_stride, int uv_stride);

#endif