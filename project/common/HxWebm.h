#ifndef HX_WEBP_HEADER
#define HX_WEBP_HEADER

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

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

#endif