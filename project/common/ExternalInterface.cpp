#ifndef IPHONE
#define IMPLEMENT_API
#endif

//#include <hx/CFFI.h>

//#include <Object.h>

#include <hxcpp.h>
//#include <hx/Macros.h>
#include <hx/CFFI.h>
//#include <hx/CFFIAPI.h>
#include <hxcpp.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "HxWebm.h"

#define DEFINE_FUNC(COUNT, NAME, ...) value NAME(__VA_ARGS__); DEFINE_PRIM(NAME, COUNT); value NAME(__VA_ARGS__)
#define DEFINE_FUNC_0(NAME) DEFINE_FUNC(0, NAME)
#define DEFINE_FUNC_1(NAME, PARAM1) DEFINE_FUNC(1, NAME, value PARAM1)
#define DEFINE_FUNC_2(NAME, PARAM1, PARAM2) DEFINE_FUNC(2, NAME, value PARAM1, value PARAM2)
#define DEFINE_FUNC_3(NAME, PARAM1, PARAM2, PARAM3) DEFINE_FUNC(3, NAME, value PARAM1, value PARAM2, value PARAM3)
#define DEFINE_FUNC_4(NAME, PARAM1, PARAM2, PARAM3, PARAM4) DEFINE_FUNC(4, NAME, value PARAM1, value PARAM2, value PARAM3, value PARAM4)
#define DEFINE_FUNC_5(NAME, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) DEFINE_FUNC(5, NAME, value PARAM1, value PARAM2, value PARAM3, value PARAM4, value PARAM5)

#if defined(HX_WINDOWS) || defined(HX_MACOS) || defined(HX_LINUX)
// Include neko glue....
#define NEKO_COMPATIBLE
#endif

#ifdef HX_WINDOWS
#define snprintf _snprintf
#endif

extern "C" {
	vkind gCodecContextKind;
	static int sgIDsInit = false;
	
	HxWebmContext* _get_codec_context_from_value(value codec_context_value) {
		if (!val_is_kind(codec_context_value, gCodecContextKind)) val_throw(alloc_string("Not a codec context"));
		return (HxWebmContext*)val_get_handle(codec_context_value, gCodecContextKind);
	}
	
	buffer get_buffer_from_value(value data_buffer_value) {
		if (!val_is_buffer(data_buffer_value)) {
			val_throw(alloc_string("Expected to be a buffer"));
			//return alloc_null();
		}
		return val_to_buffer(data_buffer_value);
	}
	
	vpx_codec_err_t check_result(vpx_codec_err_t result) {
		if (result != VPX_CODEC_OK) val_throw(alloc_string(vpx_codec_err_to_string(result)));
		/*
		switch (result) {
			case VPX_CODEC_ERROR: val_throw(alloc_string("vpx_codec: Unspecified error")); break;
			case VPX_CODEC_MEM_ERROR: val_throw(alloc_string("vpx_codec: Memory operation failed")); break;
			case VPX_CODEC_ABI_MISMATCH: val_throw(alloc_string("vpx_codec: ABI version mismatch")); break;
			case VPX_CODEC_INCAPABLE: val_throw(alloc_string("vpx_codec: Algorithm does not have required capability")); break;
			case VPX_CODEC_UNSUP_BITSTREAM: val_throw(alloc_string("vpx_codec: The given bitstream is not supported")); break;
			case VPX_CODEC_UNSUP_FEATURE: val_throw(alloc_string("vpx_codec: Encoded bitstream uses an unsupported feature")); break;
			case VPX_CODEC_CORRUPT_FRAME: val_throw(alloc_string("vpx_codec: The coded data for this stream is corrupt or incomplete")); break;
			case VPX_CODEC_INVALID_PARAM: val_throw(alloc_string("vpx_codec: An application-supplied parameter is not valid")); break;
		}
		*/
		return result;
	}

	DEFINE_FUNC_0(hx_vpx_codec_iface_name) {
		return alloc_string(_vpx_codec_iface_name());
	}
	
	static void release_HxWebmContext(value inValue) {
		HxWebmContext* codecPointer = _get_codec_context_from_value(inValue);
		//printf("release_HxWebmContext: %08X\n", codecPointer);
		_vpx_codec_destroy(codecPointer);
		free(codecPointer);
	}
	
	DEFINE_FUNC_0(hx_vpx_codec_dec_init) {
		HxWebmContext* codecPointer = (HxWebmContext*)malloc(sizeof(HxWebmContext));
		//printf("hx_vpx_codec_dec_init: %08X\n", codecPointer);
		check_result(_vpx_codec_dec_init(codecPointer));
		value result = alloc_abstract(gCodecContextKind, codecPointer);
		val_gc(result, release_HxWebmContext);
		return result;
	}
	
	/*
	DEFINE_FUNC_1(hx_vpx_codec_destroy, codec_context_value) {
		_vpx_codec_destroy(_get_codec_context_from_value(codec_context_value));
		return alloc_null();
	}
	*/
	
	DEFINE_FUNC_2(hx_vpx_codec_decode, codec_context_value, data_buffer_value) {
		HxWebmContext* codecPointer = _get_codec_context_from_value(codec_context_value);
		buffer data_buffer = get_buffer_from_value(data_buffer_value);

		//printf("hx_vpx_codec_decode: %08X\n", codecPointer);

		//codecPointer->frameSize = buffer_size(data_buffer);
		//memcpy(&codecPointer->frameData, buffer_data(data_buffer), codecPointer->frameSize);
		
		//vpx_codec_err_t result = check_result(_vpx_codec_decode(codecPointer, (char *)&codecPointer->frameData[0], codecPointer->frameSize));
		vpx_codec_err_t result = check_result(_vpx_codec_decode(codecPointer, buffer_data(data_buffer), buffer_size(data_buffer)));

		value array = alloc_array(3);
		val_array_set_i(array, 0, alloc_int(codecPointer->stream_info.w));
		val_array_set_i(array, 1, alloc_int(codecPointer->stream_info.h));
		val_array_set_i(array, 2, alloc_int(codecPointer->stream_info.is_kf));
		return array;
	}
	
	DEFINE_FUNC_2(hx_vpx_test_decode_main, in_filename_value, out_filename_value) {
		test_decode_main((const char *)val_string(in_filename_value), (const char *)val_string(out_filename_value));
		return alloc_null();
	}
	
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
	
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
void YUV420toRGBA(uint8* Y,
                  uint8* U,
                  uint8* V,
                  int words_per_line,
                  int width,
                  int height,
                  uint32* pixdata, int y_stride, int uv_stride) {
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

	DEFINE_FUNC_1(hx_vpx_codec_get_frame, codec_context_value) {
		vpx_image_t *img = _vpx_codec_get_frame(_get_codec_context_from_value(codec_context_value));
		//printf("img: %p\n", img);
		if (img == NULL) return alloc_null();
		
		//printf("  %d, %d\n", img->d_w, img->d_h);
		
		int display_width = img->d_w;
		int display_height = img->d_h;
		
		int stored_width = img->w;
		int stored_height = img->h;
		
		//int min_height = min(img->d_h, img->h);
		//int min_width = min(img->d_w, img->w);

		int output_len = display_width * display_height * 4;
		buffer output_buffer = alloc_buffer_len(output_len);
		unsigned char *output_data_start = (unsigned char*)buffer_data(output_buffer);
		unsigned char *output_data = output_data_start;
		//unsigned char *output_data = (unsigned char*)malloc(output_len);
		
		if (output_data == NULL) {
			val_throw(alloc_string("Can't alloc memory"));
			return alloc_null();
		}
		
		//printf("  %p\n", output_data);
		//printf("  %p, %p, %p, %p\n", p0, p1, p2, p3);
		//printf("  %d, %d, %d, %d\n", s0, s1, s2, s3);
		
		//printf("img:: fmt:%08X\n", img->fmt);
		
		/*
		if (img->fmt == VPX_IMG_FMT_I420) {
		}
		*/
		
		//dump_img(img);
		
		//return alloc_null();
		
		/*
		unsigned char *p[4];

		for (int n = 0; n < 4; n++) {
			p[n] = img->planes[n] ? (img->planes[n] + img->stride[n] * 0) : NULL;
			printf("  %d: %p\n", n, p[n]);
		}

		output_data = output_data_start;
		
		for (int y = 0; y < stored_height; y++) {
			for (int x = 0; x < stored_width; x++) {
				*output_data++ = 0xFF;
				*output_data++ = 0xFF;
				*output_data++ = 0xFF;
				*output_data++ = 0xFF;
			}
		}
		*/
		
		output_data = output_data_start;
		
		//printf("STRIDE: %d, %d\n", display_width, img->stride[0]);
		YUV420toRGBA(img->planes[0], img->planes[1], img->planes[2], display_width, display_width, display_height, (uint32*)output_data, img->stride[0], img->stride[1]);
		
		/*
		for (int y = 0; y < display_height; y++) {
			for (int x = 0; x < display_width; x++) {
				int Y = yuv_getY(img, x, y);
				//int U = yuv_getU(img, x, y);
				//int V = yuv_getV(img, x, y);
				int A = yuv_getA(img, x, y);
				
				//unsigned char B = clamp_00_ff(1.164 * (Y - 16) + 2.018 * (U - 128));
				//unsigned char G = clamp_00_ff(1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128));
				//unsigned char R = clamp_00_ff(1.164 * (Y - 16) + 1.596 * (V - 128));
				
				unsigned char R = Y;
				unsigned char G = 0x00;
				unsigned char B = 0x00;
				
				//unsigned char R = clamp_00_ff(Y+U-V);
				//unsigned char G = clamp_00_ff(Y+V);
				//unsigned char B = clamp_00_ff(Y-U-V);
			
				*output_data++ = A;
				*output_data++ = B;
				*output_data++ = G;
				*output_data++ = R;
			}
			
			//for (int x = 0; x < (display_width - stored_width); x++) *output_data++ = *output_data++ = *output_data++ = *output_data++ = 0;
		}
		*/
		
		//buffer output_buffer = alloc_buffer_len(0);
		//buffer_append_sub(output_buffer, (const char *)output_data, output_len);

		value array = alloc_array(3);
		val_array_set_i(array, 0, alloc_int(display_width));
		val_array_set_i(array, 1, alloc_int(display_height));
		val_array_set_i(array, 2, buffer_val(output_buffer));
		return array;

	}
	
	void InitIDs()
	{
		if (sgIDsInit) return;
		sgIDsInit = true;
		
		gCodecContextKind = alloc_kind();
	}
	
	DEFINE_ENTRY_POINT(InitIDs);
}
