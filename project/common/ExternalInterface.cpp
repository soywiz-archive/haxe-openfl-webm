#ifndef IPHONE
#define IMPLEMENT_API
#endif

#ifndef STATIC_LINK
#define IMPLEMENT_API
#endif

#if defined(HX_WINDOWS) || defined(HX_MACOS) || defined(HX_LINUX)
// Include neko glue....
#define NEKO_COMPATIBLE
#endif

#include <hx/CFFI.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>
#include <assert.h>
#include <math.h>
#include "../libwebm/mkvparser.hpp"
#include <vorbis/codec.h>

#ifndef NULL
#define NULL 0
#endif

#undef interface
#define interface vpx_codec_vp8_dx()

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include <hx/CFFI.h>

#include <stdlib.h>

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


#define DEFINE_FUNC(COUNT, NAME, ...) value NAME(__VA_ARGS__); DEFINE_PRIM(NAME, COUNT); value NAME(__VA_ARGS__)
#define DEFINE_FUNC_0(NAME) DEFINE_FUNC(0, NAME)
#define DEFINE_FUNC_1(NAME, PARAM1) DEFINE_FUNC(1, NAME, value PARAM1)
#define DEFINE_FUNC_2(NAME, PARAM1, PARAM2) DEFINE_FUNC(2, NAME, value PARAM1, value PARAM2)
#define DEFINE_FUNC_3(NAME, PARAM1, PARAM2, PARAM3) DEFINE_FUNC(3, NAME, value PARAM1, value PARAM2, value PARAM3)
#define DEFINE_FUNC_4(NAME, PARAM1, PARAM2, PARAM3, PARAM4) DEFINE_FUNC(4, NAME, value PARAM1, value PARAM2, value PARAM3, value PARAM4)
#define DEFINE_FUNC_5(NAME, PARAM1, PARAM2, PARAM3, PARAM4, PARAM5) DEFINE_FUNC(5, NAME, value PARAM1, value PARAM2, value PARAM3, value PARAM4, value PARAM5)

#ifdef HX_WINDOWS
#define snprintf _snprintf
#endif

extern "C" {
	vkind gCodecContextKind;
	vkind gIoStructKind;
	vkind kind_MkvProcessor;
	static int init_ids_done = false;
	
	void init_ids() {
		if (init_ids_done) return; else init_ids_done = true;
		
		gCodecContextKind = alloc_kind();
		gIoStructKind = alloc_kind();
		kind_MkvProcessor = alloc_kind();
	}
	
	DEFINE_ENTRY_POINT(init_ids);
	
	HxWebmContext* _get_codec_context_from_value(value _value) {
		if (!val_is_kind(_value, gCodecContextKind)) val_throw(alloc_string("Not a codec context"));
		return (HxWebmContext*)val_get_handle(_value, gCodecContextKind);
	}

	io_struct* _get_io_struct_from_value(value _value) {
		if (!val_is_kind(_value, gIoStructKind)) val_throw(alloc_string("Not a io_struct"));
		return (io_struct*)val_get_handle(_value, gIoStructKind);
	}

	buffer get_buffer_from_value(value data_buffer_value) {
		if (!val_is_buffer(data_buffer_value)) val_throw(alloc_string("Expected to be a buffer"));
		return val_to_buffer(data_buffer_value);
	}
	
	vpx_codec_err_t check_result(vpx_codec_err_t result) {
		if (result != VPX_CODEC_OK) val_throw(alloc_string(vpx_codec_err_to_string(result)));
		return result;
	}

	DEFINE_FUNC_0(hx_vpx_codec_iface_name) {
		return alloc_string(_vpx_codec_iface_name());
	}
	
	static void release_HxWebmContext(value inValue) {
		HxWebmContext* codecPointer = _get_codec_context_from_value(inValue);
		_vpx_codec_destroy(codecPointer);
		free(codecPointer);
	}
	
	DEFINE_FUNC_0(hx_vpx_codec_dec_init) {
		HxWebmContext* codecPointer = (HxWebmContext*)malloc(sizeof(HxWebmContext));
		check_result(_vpx_codec_dec_init(codecPointer));
		value result = alloc_abstract(gCodecContextKind, codecPointer);
		val_gc(result, release_HxWebmContext);
		return result;
	}
	
	DEFINE_FUNC_2(hx_vpx_codec_decode, codec_context_value, data_buffer_value) {
		HxWebmContext* codecPointer = _get_codec_context_from_value(codec_context_value);
		buffer data_buffer = get_buffer_from_value(data_buffer_value);

		vpx_codec_err_t result = check_result(_vpx_codec_decode(codecPointer, buffer_data(data_buffer), buffer_size(data_buffer)));

		value array = alloc_array(3);
		val_array_set_i(array, 0, alloc_int(codecPointer->stream_info.w));
		val_array_set_i(array, 1, alloc_int(codecPointer->stream_info.h));
		val_array_set_i(array, 2, alloc_int(codecPointer->stream_info.is_kf));
		return array;
	}
	
	DEFINE_FUNC_1(hx_vpx_codec_get_frame, codec_context_value) {
		vpx_image_t *img = _vpx_codec_get_frame(_get_codec_context_from_value(codec_context_value));
		//printf("img: %p\n", img);
		if (img == NULL) return alloc_null();
		
		//printf("  %d, %d\n", img->d_w, img->d_h);
		
		int display_width = img->d_w;
		int display_height = img->d_h;
		
		int output_len = display_width * display_height * 4;
		buffer output_buffer = alloc_buffer_len(output_len);
		unsigned char *output_data = (unsigned char*)buffer_data(output_buffer);
		
		if (output_data == NULL) {
			val_throw(alloc_string("Can't alloc memory"));
			return alloc_null();
		}
		
		YUV420toRGBA(
			img->planes[0],
			img->planes[1],
			img->planes[2],
			display_width,
			display_width,
			display_height,
			(uint32*)output_data,
			img->stride[0],
			img->stride[1]
		);
		
		value array = alloc_array(3);
		val_array_set_i(array, 0, alloc_int(display_width));
		val_array_set_i(array, 1, alloc_int(display_height));
		val_array_set_i(array, 2, buffer_val(output_buffer));
		return array;
	}

	static void release_io_struct(value inValue) {
		//printf("release_io_struct\n"); fflush(stdout);
		io_struct* io = _get_io_struct_from_value(inValue);
		free(io);
	}
	
	int io_proxy_read(void * out_buffer, size_t length, void * userdata) {
		io_struct *io = (io_struct *)userdata;
		value data_value = val_call1(io->func_read, alloc_int(length));
		buffer data = val_to_buffer(data_value);
		memcpy(out_buffer, buffer_data(data), buffer_size(data));
		return buffer_size(data);
	}
	
	int io_proxy_seek(int64_t offset, int whence, void * userdata) {
		io_struct *io = (io_struct *)userdata;
		value out = val_call2(io->func_seek, alloc_float(offset), alloc_int(whence));
		//printf("seek: %d, %d\n", (int)offset, (int)whence);
		return val_int(out);
	}

	int64_t io_proxy_tell(void * userdata) {
		io_struct *io = (io_struct *)userdata;
		value out = val_call0(io->func_tell);
		//printf("tell: %d\n", (int)val_number(out));
		return (int64_t)val_number(out);
	}
	
	DEFINE_FUNC_3(hx_create_io, read, seek, tell) {
		//printf("hx_create_io\n"); fflush(stdout);
		io_struct* io = (io_struct *)malloc(sizeof(io_struct));
		io->func_read = read;
		io->func_seek = seek;
		io->func_tell = tell;
		val_gc_add_root(&io->func_read);
		val_gc_add_root(&io->func_seek);
		val_gc_add_root(&io->func_tell);
		value result = alloc_abstract(gIoStructKind, io);
		val_gc(result, release_io_struct);
		return result;
	}
	
	namespace mkvparser {
	
		class IoMkvReader : public IMkvReader
		{
			io_struct *io;
			
		public:
			IoMkvReader(io_struct *io) {
				//printf("IoMkvReader\n"); fflush(stdout);
				this->io = io;
			}

			virtual int Read(long long pos, long len, unsigned char* buf) {
				//printf("IoMkvReader::Read(%d, %d, %p)\n", (int)pos, (int)len, buf);
				io_proxy_seek(pos, SEEK_SET, this->io);
				io_proxy_read(buf, len, this->io);
				return 0;
			}

			virtual int Length(long long* total, long long* available) {
				int64_t cur = io_proxy_tell(this->io);
				io_proxy_seek(0, SEEK_END, this->io);
				int64_t end = io_proxy_tell(this->io);
				io_proxy_seek(cur, SEEK_SET, this->io);
				
				*total = end;
				//*available = end - cur;
				*available = end;

				//printf("IoMkvReader::Length(total=%d, available=%d, cur=%d)\n", (int)*total, (int)*available, (int)cur);

				return 0;
			}

			virtual ~IoMkvReader() {
				//printf("~IoMkvReader\n"); fflush(stdout);
			}
		};
		
		static const wchar_t* utf8towcs(const char* str)
		{
		#if ANDROID
			int len = strlen(str);
			wchar_t* const val = new wchar_t[len + 1];
			for (int n = 0; n < len; n++) val[n] = str[n];
			val[len] = 0;
		#else
			if (str == NULL)
				return NULL;

			//TODO: this probably requires that the locale be
			//configured somehow:

			const size_t size = mbstowcs(NULL, str, 0);

			if (size == 0)
				return NULL;

			wchar_t* const val = new wchar_t[size+1];

			mbstowcs(val, str, size);
			val[size] = L'\0';
		#endif
			return val;
		}
		
		ogg_packet InitOggPacket(int &mPacketCount, unsigned char* aData, size_t aLength, bool aBOS, bool aEOS, int64_t aGranulepos)
		{
			ogg_packet packet;
			packet.packet = aData;
			packet.bytes = aLength;
			packet.b_o_s = aBOS;
			packet.e_o_s = aEOS;
			packet.granulepos = aGranulepos;
			packet.packetno = mPacketCount++;
			return packet;
		}
		
		class VorbisDecoder {
		public:
			// Vorbis decoder state
			vorbis_info mVorbisInfo;
			vorbis_comment mVorbisComment;
			vorbis_dsp_state mVorbisDsp;
			vorbis_block mVorbisBlock;
			uint32_t mPacketCount;
			uint32_t mChannels;
			ogg_packet oggPacket;
			
			VorbisDecoder() {
				resetPacketNumber();
			}
			
			void resetPacketNumber() {
				oggPacket.packetno = -1;
			}

			void preparePacket(unsigned char * data, const long size) {
				oggPacket.packet = data;
				oggPacket.bytes = size;
				oggPacket.b_o_s = (oggPacket.packetno == -1);
				oggPacket.e_o_s = false;
				oggPacket.packetno++; 
				oggPacket.granulepos = -1; 
			}

			int parseData(unsigned char * data, const long size, long long int time_ns, value decode_audio) {
				int r;
				preparePacket(data, size);
				
				if (oggPacket.bytes > 0) {
					r = vorbis_synthesis(&mVorbisBlock, &oggPacket);
					if (r < 0) {
						fprintf(stderr, "vorbis_synthesis failed, error: %d\n", r);
						return -1;
					}
					
					r = vorbis_synthesis_blockin(&mVorbisDsp, &mVorbisBlock); 
					if (r < 0) {
						fprintf(stderr, "vorbis_synthesis failed, error: %d\n", r);
						return -1;
					}
					
					float **pcm = NULL;
					int nsamples = 0;
					
					if ((nsamples = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm)) > 0) {
						int nchannels = 2;
						buffer pcm_data = alloc_buffer_len(nsamples * nchannels * sizeof(float));
						float *samples = (float *)buffer_data(pcm_data);
						
						for (int sample = 0; sample < nsamples; sample++) {
							for (int channel = 0; channel < nchannels; channel++) {
								float fval = pcm[channel][sample];
								//short sval = (short)(fval * 32767);
								*samples++ = fval;
							}
						}
						
						val_call2(decode_audio, alloc_float((double)(time_ns / 1000) / (double)(1000 * 1000)), buffer_val(pcm_data));
						
						//printf("%p: %d\n", pcm, samples);
						vorbis_synthesis_read(&mVorbisDsp, nsamples);
					}
				}
				
				return 0;
			}
			
			static uint64_t ne_xiph_lace_value(unsigned char ** np)
			{
			  uint64_t lace;
			  uint64_t value;
			  unsigned char * p = *np;

			  lace = *p++;
			  value = lace;
			  while (lace == 255) {
				lace = *p++;
				value += lace;
			  }

			  *np = p;

			  return value;
			}

			// https://groups.google.com/a/webmproject.org/group/webm-discuss/tree/browse_frm/month/2010-09/be9ab6c87fb2bca2?rnum=41&_done=%2Fa%2Fwebmproject.org%2Fgroup%2Fwebm-discuss%2Fbrowse_frm%2Fmonth%2F2010-09%3F
			int parseHeader(unsigned char * data, const long size) {
				unsigned char * data_start = data;
				int status; 

				vorbis_info_init(&mVorbisInfo); 
				vorbis_comment_init(&mVorbisComment); 
				memset(&mVorbisDsp, 0, sizeof(mVorbisDsp)); 
				memset(&mVorbisBlock, 0, sizeof(mVorbisBlock)); 
				
				int packetCount = *data++ + 1;
				if (packetCount > 3) {
					fprintf(stderr, "packetCount > 3: %d\n", packetCount);
					return -1;
				}
				
				//printf("vorbis.parseHeader.packetCount: %d\n", packetCount);
				
				int lengths[3] = {0};
				int total = 0;

				for (int header_num = 0; header_num < packetCount - 1; header_num++) {
					int length = ne_xiph_lace_value(&data);
					total += length;
					lengths[header_num] = length;
				}
				
				lengths[2] = size - total;
				
				for (int header_num = 0; header_num < packetCount; header_num++) {
					//printf("  packer header[%d]: %d\n", header_num, lengths[header_num]);
				}
				

				for (int header_num = 0; header_num < packetCount; header_num++) {
					uint64_t psize = lengths[header_num];
					unsigned char * pdata = data;
					preparePacket(pdata, psize);
					data += psize;
					
					//printf("  packet: %p, %d\n", pdata, psize);
					//for (int m = 0; m < psize; m++) printf("%02X ", (unsigned char)pdata[m]);
					//printf("\n");

					assert(oggPacket.packetno == header_num);
					status = vorbis_synthesis_headerin(&mVorbisInfo, &mVorbisComment, &oggPacket); 
					if (status < 0) {
						fprintf(stderr, "vorbis_synthesis_headerin failed(%d), error: %d\n", header_num, status);
						return -1;
					}
				}
			
				printf("Vorbis: version:%d, channels:%d, rate:%d\n", mVorbisInfo.version, mVorbisInfo.channels, mVorbisInfo.rate);
				
				status = vorbis_synthesis_init(&mVorbisDsp, &mVorbisInfo); 
				if (status < 0) {
					fprintf(stderr, "vorbis_synthesis_init failed, error: %d\n", status);
					return -1;
				}
				
				status = vorbis_block_init(&mVorbisDsp, &mVorbisBlock); 
				if (status < 0) {
					fprintf(stderr, "vorbis_block_init failed, error: %d\n", status);
					return -1;
				}
				
				return 0;
			}
		};
		
		// http://dxr.mozilla.org/mozilla-central/content/media/webm/nsWebMReader.h.html
		class MkvProcessor {
		public:
			IoMkvReader *reader;
			unsigned long clusterCount;
			const mkvparser::Cluster *pCluster;
			const mkvparser::Tracks *pTracks;
			mkvparser::Segment *pSegment;
			const BlockEntry* pBlockEntry;
			long long timeCode;
			bool startCluster;
			bool hasMore;
			int width, height;
			double frameRate;
			double duration_sec;
			VorbisDecoder *vorbisDecoder;
			
			MkvProcessor(IoMkvReader *reader) {
				//printf("MkvProcessor\n"); fflush(stdout);
				this->pCluster = NULL;
				this->pTracks = NULL;
				this->pSegment = NULL;
				this->pBlockEntry = NULL;
				this->startCluster = true;
				this->timeCode = 0;
				this->reader = reader;
				this->hasMore = true;
				this->width = 0;
				this->height = 0;
				this->frameRate = 30;
				this->duration_sec = 0;
				this->vorbisDecoder = new VorbisDecoder();
			}
			
			~MkvProcessor() {
				//printf("~MkvProcessor\n"); fflush(stdout);
			}
			
			int parseHeader() {
				long long pos = 0;
				
				EBMLHeader ebmlHeader;

				ebmlHeader.Parse(reader, pos);

				printf("\t\t\t    EBML Header\n");
				printf("\t\tEBML Version\t\t: %lld\n", ebmlHeader.m_version);
				printf("\t\tEBML MaxIDLength\t: %lld\n", ebmlHeader.m_maxIdLength);
				printf("\t\tEBML MaxSizeLength\t: %lld\n", ebmlHeader.m_maxSizeLength);
				printf("\t\tDoc Type\t\t: %s\n", ebmlHeader.m_docType);
				printf("\t\tPos\t\t\t: %lld\n", pos);
				
				mkvparser::Segment* pSegment_;

				long long ret = mkvparser::Segment::CreateInstance(reader, pos, pSegment_);
				if (ret)
				{
					printf("\n Segment::CreateInstance() failed.");
					return -1;
				}

				//pSegment = new mkvparser::Segment(pSegment_);
				pSegment = pSegment_;

				ret  = (long long)pSegment->Load();
				if (ret < 0)
				{
					printf("\n Segment::Load() failed.");
					return -1;
				}

				const SegmentInfo* const pSegmentInfo = pSegment->GetInfo();

				const long long timeCodeScale = pSegmentInfo->GetTimeCodeScale();
				const long long duration_ns = pSegmentInfo->GetDuration();

				const char* const pTitle_ = pSegmentInfo->GetTitleAsUTF8();
				const wchar_t* const pTitle = utf8towcs(pTitle_);

				const char* const pMuxingApp_ = pSegmentInfo->GetMuxingAppAsUTF8();
				const wchar_t* const pMuxingApp = utf8towcs(pMuxingApp_);

				const char* const pWritingApp_ = pSegmentInfo->GetWritingAppAsUTF8();
				const wchar_t* const pWritingApp = utf8towcs(pWritingApp_);

				printf("\n");
				printf("\t\t\t   Segment Info\n");
				printf("\t\tTimeCodeScale\t\t: %lld \n", timeCodeScale);
				printf("\t\tDuration\t\t: %lld\n", duration_ns);

				this->duration_sec = double(duration_ns) / 1000000000;
				printf("\t\tDuration(secs)\t\t: %7.3lf\n", this->duration_sec);

				if (pTitle == NULL) {
					printf("\t\tTrack Name\t\t: NULL\n");
				} else {
					printf("\t\tTrack Name\t\t: %ls\n", pTitle);
					delete [] pTitle;
				}


				if (pMuxingApp == NULL)
					printf("\t\tMuxing App\t\t: NULL\n");
				else
				{
					printf("\t\tMuxing App\t\t: %ls\n", pMuxingApp);
					delete [] pMuxingApp;
				}

				if (pWritingApp == NULL)
					printf("\t\tWriting App\t\t: NULL\n");
				else
				{
					printf("\t\tWriting App\t\t: %ls\n", pWritingApp);
					delete [] pWritingApp;
				}

				// pos of segment payload
				printf("\t\tPosition(Segment)\t: %lld\n", pSegment->m_start);

				// size of segment payload
				printf("\t\tSize(Segment)\t\t: %lld\n", pSegment->m_size);

				pTracks = pSegment->GetTracks();

				unsigned long currentTrack = 0;
				const unsigned long totalTracks = pTracks->GetTracksCount();

				printf("\n\t\t\t   Track Info\n");

				for (int currentTrack = 0; currentTrack < totalTracks; currentTrack++)
				{
				printf("\t\t------------------------\n");
					const Track* const pTrack = pTracks->GetTrackByIndex(currentTrack);

					if (pTrack == NULL) continue;

					const long trackType = pTrack->GetType();
					const long trackNumber = pTrack->GetNumber();
					const unsigned long long trackUid = pTrack->GetUid();
					const wchar_t* const pTrackName = utf8towcs(pTrack->GetNameAsUTF8());

					printf("\t\tTrack Type\t\t: %ld\n", trackType);
					printf("\t\tTrack Number\t\t: %ld\n", trackNumber);
					printf("\t\tTrack Uid\t\t: %lld\n", trackUid);

					if (pTrackName == NULL) {
						printf("\t\tTrack Name\t\t: NULL\n");
					} else {
						printf("\t\tTrack Name\t\t: %ls \n", pTrackName);
						delete [] pTrackName;
					}

					const char* const pCodecId = pTrack->GetCodecId();

					if (pCodecId == NULL) {
						printf("\t\tCodec Id\t\t: NULL\n");
					} else {
						printf("\t\tCodec Id\t\t: %s\n", pCodecId);
					}

					const char* const pCodecName_ = pTrack->GetCodecNameAsUTF8();
					const wchar_t* const pCodecName = utf8towcs(pCodecName_);

					if (pCodecName == NULL) {
						printf("\t\tCodec Name\t\t: NULL\n");
					} else {
						printf("\t\tCodec Name\t\t: %ls\n", pCodecName);
						delete [] pCodecName;
					}

					if (trackType == mkvparser::Track::kVideo) {
						const VideoTrack* const pVideoTrack = static_cast<const VideoTrack*>(pTrack);
						this->width = pVideoTrack->GetWidth();
						this->height = pVideoTrack->GetHeight();
						this->frameRate = pVideoTrack->GetFrameRate();

						printf("\t\tVideo Width\t\t: %d\n", this->width);
						printf("\t\tVideo Height\t\t: %d\n", this->height);
						printf("\t\tVideo Rate\t\t: %f\n", (float)this->frameRate);
					}

					if (trackType == mkvparser::Track::kAudio) {
						const AudioTrack* const pAudioTrack = static_cast<const AudioTrack*>(pTrack);
						size_t privateDataSize = 0;
						unsigned char *privateDataPointer = (unsigned char *)pAudioTrack->GetCodecPrivate(privateDataSize);

						printf("\t\tAudio Channels\t\t: %lld\n", pAudioTrack->GetChannels());
						printf("\t\tAudio BitDepth\t\t: %lld\n", pAudioTrack->GetBitDepth());
						printf("\t\tAddio Sample Rate\t: %.3f\n", pAudioTrack->GetSamplingRate());
						printf("\t\tAddio Private Data\t: %p, %d\n", privateDataPointer, privateDataSize);
						
						this->vorbisDecoder->parseHeader(privateDataPointer, privateDataSize);
					}
				}

				printf("\n\n\t\t\t   Cluster Info\n");
				clusterCount = pSegment->GetCount();

				printf("\t\tCluster Count\t: %ld\n\n", clusterCount);

				if (clusterCount == 0)
				{
					printf("\t\tSegment has no clusters.\n");
					return -1;
				}

				fflush(stdout);

				return 0;
			}
			
			int parseStep(value decode_video, value decode_audio) {
				if (!this->hasMore) return 0;
			
				if (startCluster) {
					//printf("startCluster\n"); fflush(stdout);
					if (pCluster == NULL) {
						pCluster = pSegment->GetFirst();
					} else {
						pCluster = pSegment->GetNext(pCluster);
					}
					this->hasMore = ((pCluster != NULL) && !pCluster->EOS());
					if (!this->hasMore) return 0;
					
					pBlockEntry = NULL;
					startCluster = false;
				}

				long status = (pBlockEntry == NULL) ? pCluster->GetFirst(pBlockEntry) : pCluster->GetNext(pBlockEntry, pBlockEntry);

				if (status < 0)  //error
				{
					printf("\t\tError parsing first block of cluster\n");
					fflush(stdout);
					return -1;
				}

				//printf("\t\tCluster Time Code\t: %lld\n", pCluster->GetTimeCode());
				//printf("\t\tCluster Time (ns)\t: %lld\n", pCluster->GetTime());
				
				//return 0;

				if ((pBlockEntry != NULL) && !pBlockEntry->EOS())
				{
					const Block* const pBlock  = pBlockEntry->GetBlock();
					//printf("pBlock: %p, pBlockEntry: %p\n", pBlock, pBlockEntry); fflush(stdout);
				
					const long long trackNum = pBlock->GetTrackNumber();
					const unsigned long tn = static_cast<unsigned long>(trackNum);
					//printf("trackNumber: %d\n", tn);
					const Track* pTrack = pTracks->GetTrackByNumber(tn);
					//printf("pTrack: %p\n", pTrack);

					if (pTrack == NULL) {
						printf("\t\t\tBlock\t\t:UNKNOWN TRACK TYPE\n");
					} else {
						const long long trackType = pTrack->GetType();
						const int frameCount = pBlock->GetFrameCount();
						const long long time_ns = pBlock->GetTime(pCluster);

						//printf("\t\t\tBlock\t\t:%s,%s,%15lld\n", (trackType == mkvparser::Track::kVideo) ? "V" : "A", pBlock->IsKey() ? "I" : "P", time_ns); fflush(stdout);

						for (int i = 0; i < frameCount; ++i) {
							const Block::Frame& theFrame = pBlock->GetFrame(i);
							const long size = theFrame.len;
							const long long offset = theFrame.pos;
							
#if 1
							//printf("%d, %d\n", (int)size, (int)offset); fflush(stdout);
							
							buffer frame_data = alloc_buffer_len(size);
							theFrame.Read(reader, (unsigned char *)buffer_data(frame_data));
							
							if (trackType == mkvparser::Track::kVideo) {
								//printf("%lld\n", time_ns);
								val_call2(decode_video, alloc_float((double)(time_ns / 1000) / (double)(1000 * 1000)), buffer_val(frame_data));
							} else if (trackType == mkvparser::Track::kAudio) {
								vorbisDecoder->parseData((unsigned char *)buffer_data(frame_data), buffer_size(frame_data), time_ns, decode_audio);
							}
#endif
							
							//printf("\t\t\t %15ld,%15llx\n", size, offset);
						}
					}
				} else {
					startCluster = true;
				}
				
				//printf("endParseStep\n"); fflush(stdout);
				
				return 0;
			}
		};
		
		MkvProcessor* _get_MkvProcessor_from_value(value _value) {
			if (!val_is_kind(_value, kind_MkvProcessor)) val_throw(alloc_string("Not a MkvProcessor"));
			return (MkvProcessor*)val_get_handle(_value, kind_MkvProcessor);
		}
		
		static void release_MkvProcessor(value _value) {
			//printf("release_MkvProcessor\n"); fflush(stdout);
			MkvProcessor* processor = _get_MkvProcessor_from_value(_value);
			delete processor;
		}

		DEFINE_FUNC_1(hx_webm_decoder_create, io_value) {
			io_struct* io = _get_io_struct_from_value(io_value);
			
			MkvProcessor *processor = new MkvProcessor(new IoMkvReader(io));

			processor->parseHeader();
			
			/*
			while (processor->hasMore()) {
				processor->parseStep();
			}
			*/
			
			value result = alloc_abstract(kind_MkvProcessor, processor);
			val_gc(result, release_MkvProcessor);
			return result;
		}

		DEFINE_FUNC_1(hx_webm_decoder_has_more, processor_value) {
			MkvProcessor* processor = _get_MkvProcessor_from_value(processor_value);
			return alloc_bool(processor->hasMore);
		}
		
		DEFINE_FUNC_1(hx_webm_decoder_get_info, processor_value) {
			MkvProcessor* processor = _get_MkvProcessor_from_value(processor_value);
			value array = alloc_array(4);
			val_array_set_i(array, 0, alloc_int(processor->width));
			val_array_set_i(array, 1, alloc_int(processor->height));
			val_array_set_i(array, 3, alloc_float(processor->frameRate));
			val_array_set_i(array, 4, alloc_float(processor->duration_sec));
			return array;
		}

		DEFINE_FUNC_3(hx_webm_decoder_step, processor_value, decode_video_value, decode_audio_value) {
			MkvProcessor* processor = _get_MkvProcessor_from_value(processor_value);
			processor->parseStep(decode_video_value, decode_audio_value);
			//return alloc_bool(processor->hasMore());
			return alloc_null();
		}
		
		DEFINE_FUNC_0(hx_vorbis_codec_dec_init) {
			return alloc_null();
		}
	}
}
