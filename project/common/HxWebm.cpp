#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "HxWebm.h"

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

int test_decode_main(const char *infileName, const char* outfileName) {
    FILE            *infile, *outfile;
    vpx_codec_ctx_t  codec;
    int              flags = 0, frame_cnt = 0;
    unsigned char    file_hdr[IVF_FILE_HDR_SZ];
    unsigned char    frame_hdr[IVF_FRAME_HDR_SZ];
    unsigned char    frame[256*1024];
    vpx_codec_err_t  res;

    (void)res;

    if(!(infile = fopen(infileName, "rb")))
        die("Failed to open %s for reading", infileName);
    if(!(outfile = fopen(outfileName, "wb")))
        die("Failed to open %s for writing", outfileName);

    // Read file header
    if(!(fread(file_hdr, 1, IVF_FILE_HDR_SZ, infile) == IVF_FILE_HDR_SZ
         && file_hdr[0]=='D' && file_hdr[1]=='K' && file_hdr[2]=='I'
         && file_hdr[3]=='F'))
        die("%s is not an IVF file.", infileName);

    printf("Using %s\n",vpx_codec_iface_name(interface));
    // Initialize codec                                                       //
    if(vpx_codec_dec_init(&codec, interface, NULL, flags))                    //
        die_codec(&codec, "Failed to initialize decoder");                    //

    // Read each frame
    while(fread(frame_hdr, 1, IVF_FRAME_HDR_SZ, infile) == IVF_FRAME_HDR_SZ) {
        int               frame_sz = mem_get_le32(frame_hdr);
        vpx_codec_iter_t  iter = NULL;
        vpx_image_t      *img;
		printf("%08X: %d\n", ftell(infile), frame_sz);

        frame_cnt++;
        if(frame_sz > sizeof(frame))
            die("Frame %d data too big for example code buffer", frame_sz);
        if(fread(frame, 1, frame_sz, infile) != frame_sz)
            die("Frame %d failed to read complete frame", frame_cnt);

        // Decode the frame                                                   //
        if(vpx_codec_decode(&codec, frame, frame_sz, NULL, 0))                //
            die_codec(&codec, "Failed to decode frame");                      //

        // Write decoded data to disk   
        while((img = vpx_codec_get_frame(&codec, &iter))) {                   //
            unsigned int plane, y;

            for(plane=0; plane < 3; plane++) {                                //
                unsigned char *buf =img->planes[plane];                       //
                                                                              //
                for(y=0; y < (plane ? (img->d_h + 1) >> 1 : img->d_h); y++) { //
                    (void) fwrite(buf, 1, (plane ? (img->d_w + 1) >> 1 : img->d_w),//
                                  outfile);                                   //
                    buf += img->stride[plane];                                //
                }                                                             //
            }                                                                 //
        }
    }
    //printf("Processed %d frames.\n",frame_cnt);
    if(vpx_codec_destroy(&codec))                                             //
        die_codec(&codec, "Failed to destroy codec");                         //

    fclose(outfile);
    fclose(infile);
    return EXIT_SUCCESS;
}
