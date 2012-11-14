#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "HxWebm.h"

#ifndef NULL
#define NULL 0
#endif

#define interface vpx_codec_vp8_dx()

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

static unsigned int mem_get_le32(const unsigned char *mem) {
    return (mem[3] << 24)|(mem[2] << 16)|(mem[1] << 8)|(mem[0]);
}

static void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    if(fmt[strlen(fmt)-1] != '\n')
        printf("\n");
    exit(EXIT_FAILURE);
}

static void die_codec(vpx_codec_ctx_t *ctx, const char *s) {                  //
    const char *detail = vpx_codec_error_detail(ctx);                         //
                                                                              //
    printf("%s: %s\n", s, vpx_codec_error(ctx));                              //
    if(detail)                                                                //
        printf("    %s\n",detail);                                            //
    exit(EXIT_FAILURE);                                                       //
}                                                                             //

enum {
    COLOR_RED = 0,
    COLOR_GREEN = 1,
    COLOR_BLUE = 2,
    ALPHA_CHANNEL = 3
};

/* endian neutral extractions of RGBA from a 32 bit pixel */
static const uint32  RED_SHIFT =
       8 * (sizeof(uint32) - 1 - COLOR_RED);           /* 24 */
static const uint32  GREEN_SHIFT =
       8 * (sizeof(uint32) - 1 - COLOR_GREEN);         /* 16 */
static const uint32  BLUE_SHIFT =
       8 * (sizeof(uint32) - 1 - COLOR_BLUE);          /*  8 */
static const uint32  ALPHA_SHIFT =
       8 * (sizeof(uint32) - 1 - ALPHA_CHANNEL);       /*  0 */

/* Converts YUV to RGB and writes into a 32 bit pixel in endian
 * neutral fashion
 */
static const int RGB_FRAC = 16;
static const int RGB_HALF = (1 << RGB_FRAC) / 2;
static const int RGB_RANGE_MIN = -227;
static const int RGB_RANGE_MAX = 256 + 226;

static int init_done = 0;
static int16_t kVToR[256], kUToB[256];
static int32_t kVToG[256], kUToG[256];
static uint8_t kClip[RGB_RANGE_MAX - RGB_RANGE_MIN];

void WebpInitTables() {
  int i;
  for (i = 0; i < 256; ++i) {
    kVToR[i] = (89858 * (i - 128) + RGB_HALF) >> RGB_FRAC;
    kUToG[i] = -22014 * (i - 128) + RGB_HALF;
    kVToG[i] = -45773 * (i - 128);
    kUToB[i] = (113618 * (i - 128) + RGB_HALF) >> RGB_FRAC;
  }
  for (i = RGB_RANGE_MIN; i < RGB_RANGE_MAX; ++i) {
    const int j = ((i - 16) * 76283 + RGB_HALF) >> RGB_FRAC;
    kClip[i - RGB_RANGE_MIN] = (j < 0) ? 0 : (j > 255) ? 255 : j;
  }

  init_done = 1;
}

void WebpToRGB(int y, int u, int v, uint8* const dst) {
  const int r_off = kVToR[v];
  const int g_off = (kVToG[v] + kUToG[u]) >> RGB_FRAC;
  const int b_off = kUToB[u];
  const int a = 0xFF;
  const int r = kClip[y + r_off - RGB_RANGE_MIN];
  const int g = kClip[y + g_off - RGB_RANGE_MIN];
  const int b = kClip[y + b_off - RGB_RANGE_MIN];
  dst[0] = a;
  dst[1] = r;
  dst[2] = g;
  dst[3] = b;
}


/* Generate RGBA row from an YUV row (with width upsampling of chrome data)
 * Input:
 *    1, 2, 3. y_src, u_src, v_src - Pointers to input Y, U, V row data
 *    respectively. We reuse these variables, they iterate over all pixels in
 *    the row.
 *    4. y_width: width of the Y image plane (aka image width)
 * Output:
 *    5. rgb_sat: pointer to the output rgb row. We reuse this variable, it
 *    iterates over all pixels in the row.
 */
static void YUV420toRGBLine(uint8* y_src,
                            uint8* u_src,
                            uint8* v_src,
                            int y_width,
                            uint32* rgb_dst) {
  int x;
  for (x = 0; x < (y_width >> 1); ++x) {
    const int U = u_src[0];
    const int V = v_src[0];
    WebpToRGB(y_src[0], U, V, (uint8* const)(rgb_dst));
    WebpToRGB(y_src[1], U, V, (uint8* const)(rgb_dst + 1));
    ++u_src;
    ++v_src;
    y_src += 2;
    rgb_dst += 2;
  }
  if (y_width & 1) {      /* Rightmost pixel */
    WebpToRGB(y_src[0], (*u_src), (*v_src), (uint8* const)(rgb_dst));
  }
}
	
/* Converts from YUV (with color subsampling) such as produced by the WebPDecode
 * routine into 32 bits per pixel RGBA data array. This data array can be
 * directly used by the Leptonica Pix in-memory image format.
 * Input:
 *      1, 2, 3. Y, U, V: the input data buffers
 *      4. pixwpl: the desired words per line corresponding to the supplied
 *                 output pixdata.
 *      5. width, height: the dimensions of the image whose data resides in Y,
 *                        U, V.
 * Output:
 *     6. pixdata: the output data buffer. Caller should allocate
 *                 height * pixwpl bytes of memory before calling this routine.
 */
void YUV420toRGBA(uint8* Y, uint8* U, uint8* V, int words_per_line, int width, int height, uint32* pixdata, int y_stride, int uv_stride) {
  int y_width = width;
  //int y_stride = y_width;
  int uv_width = ((y_width + 1) >> 1);
  //int uv_stride = uv_width;
  int y;

  if (!init_done)
    WebpInitTables();

  /* note that the U, V upsampling in height is happening here as the U, V
   * buffers sent to successive odd-even pair of lines is same.
   */
  for (y = 0; y < height; ++y) {
    YUV420toRGBLine(Y + y * y_stride,
                    U + (y >> 1) * uv_stride,
                    V + (y >> 1) * uv_stride,
                    width,
                    pixdata + y * words_per_line);
  }
}

const char *_vpx_codec_iface_name() {
	return vpx_codec_iface_name(interface);
}

static vpx_codec_ctx_t  codec;

vpx_codec_err_t _vpx_codec_dec_init(HxWebmContext *codecPointer) {
	int flags = 0;
	memset(codecPointer, 0, sizeof(HxWebmContext));
	return vpx_codec_dec_init(&codecPointer->context, interface, NULL, flags);
}

void _vpx_codec_destroy(HxWebmContext *codecPointer) {
	vpx_codec_destroy(&codecPointer->context);
}

vpx_codec_err_t _vpx_codec_decode(HxWebmContext *codecPointer, char *framePointer, int frameSize) {
	codecPointer->iter = NULL;
	//printf("\n_vpx_codec_decode:\n");
	//for (int n = 0; n < frameSize; n++) printf("%02X ", (unsigned char)framePointer[n]);
	//printf("\n");
	vpx_codec_err_t decode_result = vpx_codec_decode(&codecPointer->context, (unsigned char *)framePointer, frameSize, NULL, 0);
	vpx_codec_err_t info_result = vpx_codec_get_stream_info(&codecPointer->context, &codecPointer->stream_info);
			
	return decode_result;
}

vpx_image_t *_vpx_codec_get_frame(HxWebmContext *codecPointer) {
	//printf("_vpx_codec_get_frame: %p\n", codecPointer->iter);
	return vpx_codec_get_frame(&codecPointer->context, &codecPointer->iter);
}
