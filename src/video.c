/*
 * Copyright (c) 2003 Fabrice Bellard, 2007 Sean D'Epagnier
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* this code is based on ffmpeg/output_example.c */

#include <stdlib.h>
#include <stdio.h>

#include "config.h"

#ifdef HAVE_FFMPEG

#include <string.h>
#include <math.h>

#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>

#include <stdarg.h>
#include "util.h"

/* 5 seconds stream duration */
#define STREAM_FRAME_RATE 24 /* 24 images/s */
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */

static int sws_flags = SWS_BICUBIC;

AVFrame *picture, *tmp_picture;
uint8_t *video_outbuf;
int frame_count, video_outbuf_size;

/* add a video output stream */
static AVStream *add_video_stream(AVFormatContext *oc, int codec_id,
				  int width, int height)
{
    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 0);
    if (!st)
        die("video: Could not alloc stream\n");

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = width;
    c->height = height;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = STREAM_FRAME_RATE;
    c->time_base.num = 1;
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt = STREAM_PIX_FMT;
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == CODEC_ID_MPEG1VIDEO){
        /* needed to avoid using macroblocks in which some coeffs overflow
           this doesnt happen with normal video, it just happens here as the
           motion of the chroma plane doesnt match the luma plane */
        c->mb_decision=2;
    }
    // some formats want stream headers to be seperate
    if(!strcmp(oc->oformat->name, "mp4") || !strcmp(oc->oformat->name, "mov")
       || !strcmp(oc->oformat->name, "3gp"))
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

static AVFrame *alloc_picture(int pix_fmt, int width, int height)
{
    AVFrame *picture;
    uint8_t *picture_buf;
    int size;

    picture = avcodec_alloc_frame();
    if (!picture)
        return NULL;
    size = avpicture_get_size(pix_fmt, width, height);
    picture_buf = av_malloc(size);
    if (!picture_buf) {
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf,
                   pix_fmt, width, height);
    return picture;
}

static void open_video(AVFormatContext *oc, AVStream *st, int width, int height)
{
    AVCodec *codec;
    AVCodecContext *c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
        die("video: codec not found\n");

    /* open the codec */
    if (avcodec_open(c, codec) < 0)
        die("video: could not open codec\n");

    video_outbuf = NULL;
    if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
        /* allocate output buffer */
        /* XXX: API change will be done */
        /* buffers passed into lav* can be allocated any way you prefer,
           as long as they're aligned enough for the architecture, and
           they're freed appropriately (such as using av_free for buffers
           allocated with av_malloc) */
        video_outbuf_size = 200000;
        video_outbuf = av_malloc(video_outbuf_size);
    }

    /* allocate the encoded raw picture */
    picture = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!picture)
        die("video: Could not allocate picture\n");

    /* if the output format is not YUV420P, then a temporary YUV420P
       picture is needed too. It is then converted to the required
       output format */
    tmp_picture = NULL;
    if (c->pix_fmt != PIX_FMT_YUV420P) {
        tmp_picture = alloc_picture(PIX_FMT_YUV420P, c->width, c->height);
        if (!tmp_picture)
            die("video: Could not allocate temporary picture\n");
    }
}

static inline unsigned char clamp(int v)
{
   return v < 0 ? 0 : v > 255 ? 255 : v;
}

static void rgb_to_yuv_image(AVFrame *pict, unsigned char *rgbdata, int width, int height)
{
   int x, y;

   for(y=0;y<height;y+=2) {
      for(x=0;x<width;x+=2) {
	 int invy = height - y - 1;
	 unsigned char *p1 = rgbdata + (invy*width + x)*3;
	 unsigned char *p2 = rgbdata + (invy*width + x+1)*3;
	 unsigned char *p3 = rgbdata + ((invy-1)*width + x)*3;
	 unsigned char *p4 = rgbdata + ((invy-1)*width + x+1)*3;

	 unsigned char Y1 = 0.299 * p1[0] + 0.587 * p1[1] + 0.114 * p1[2];
	 unsigned char Y2 = 0.299 * p2[0] + 0.587 * p2[1] + 0.114 * p2[2];
	 unsigned char Y3 = 0.299 * p3[0] + 0.587 * p3[1] + 0.114 * p3[2];
	 unsigned char Y4 = 0.299 * p4[0] + 0.587 * p4[1] + 0.114 * p4[2];

	 pict->data[0][y * pict->linesize[0] + x] = Y1;
	 pict->data[0][y * pict->linesize[0] + x+1] = Y2;
	 pict->data[0][(y+1) * pict->linesize[0] + x] = Y3;
	 pict->data[0][(y+1) * pict->linesize[0] + x+1] = Y4;

	 if(!(x&1) && !(y&1)) {
	    int Y = (Y1 + Y2 + Y3 + Y4) >> 2;
	    int R = (p1[2] + p2[2] + p3[2] + p4[2]) >> 2;
	    int B = (p1[0] + p2[0] + p3[0] + p4[0]) >> 2;

	    unsigned char U = clamp(0.713 * (R - Y) + 127);
	    unsigned char V = clamp(0.564 * (B - Y) + 127);

	    pict->data[1][y/2 * pict->linesize[1] + x/2] = U;
	    pict->data[2][y/2 * pict->linesize[2] + x/2] = V;
	 }
      }
   }
}

static AVFormatContext *oc;
static AVStream *video_st;
static AVOutputFormat *fmt;

void videoWriteFrame(unsigned char *rgbdata, int width, int height)
{
    int out_size, ret;
    AVCodecContext *c;
    static struct SwsContext *img_convert_ctx;

    c = video_st->codec;

    if(width != c->width || height != c->height) {
       warning("video capture after a resize not supported!\n");
       return;
    }

    if (c->pix_fmt != PIX_FMT_YUV420P) {
       /* as we only generate a YUV420P picture, we must convert it
	  to the codec pixel format if needed */
       if (img_convert_ctx == NULL) {
	  img_convert_ctx = sws_getContext(c->width, c->height,
					   PIX_FMT_YUV420P,
					   c->width, c->height,
					   c->pix_fmt,
					   sws_flags, NULL, NULL, NULL);
                if (img_convert_ctx == NULL)
		   die("video: Cannot initialize the conversion context\n");
       }

       rgb_to_yuv_image(tmp_picture, rgbdata, c->width, c->height);
       sws_scale(img_convert_ctx, tmp_picture->data, tmp_picture->linesize,
                      0, c->height, picture->data, picture->linesize);
    } else {
       rgb_to_yuv_image(picture, rgbdata, c->width, c->height);
    }

    if (oc->oformat->flags & AVFMT_RAWPICTURE) {
        /* raw video case. The API will change slightly in the near
           futur for that */
        AVPacket pkt;
        av_init_packet(&pkt);

        pkt.flags |= PKT_FLAG_KEY;
        pkt.stream_index= video_st->index;
        pkt.data= (uint8_t *)picture;
        pkt.size= sizeof(AVPicture);

        ret = av_write_frame(oc, &pkt);
    } else {
        /* encode the image */
        out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);
        /* if zero size, it means the image was buffered */
        if (out_size > 0) {
            AVPacket pkt;
            av_init_packet(&pkt);

            pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, video_st->time_base);
            if(c->coded_frame->key_frame)
                pkt.flags |= PKT_FLAG_KEY;
            pkt.stream_index= video_st->index;
            pkt.data= video_outbuf;
            pkt.size= out_size;

            /* write the compressed frame in the media file */
            ret = av_write_frame(oc, &pkt);
        } else {
            ret = 0;
        }
    }
    if (ret != 0)
        die("video: Error while writing video frame\n");
    frame_count++;
}

int videoStart(const char *filename, int width, int height)
{
    /* initialize libavcodec, and register all codecs and formats */
    av_register_all();

    /* auto detect the output format from the name. default is
       mpeg. */
    fmt = guess_format(NULL, filename, NULL);

    if (!fmt)
        fmt = guess_format("avi", NULL, NULL);

    if (!fmt) {
        warning("Could not find suitable video output format\n");
        return -1;
    }

    if (!strcmp(filename, "-"))
        filename = "pipe:";

    /* allocate the output media context */
    oc = av_alloc_format_context();
    if (!oc)
        die("video: Memory error\n");

    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

    /* add the audio and video streams using the default format codecs
       and initialize the codecs */
    video_st = NULL;

    if (fmt->video_codec != CODEC_ID_NONE)
       if(!(video_st = add_video_stream(oc, fmt->video_codec, width, height)))
	  die("video: Failed to add video stream");

    /* set the output parameters (must be done even if no
       parameters). */
    if (av_set_parameters(oc, NULL) < 0)
        die("video: Invalid output format parameters\n");

    dump_format(oc, 0, filename, 1);

    /* now that all the parameters are set, we can open the video codec
       and allocate the necessary encode buffers */
    open_video(oc, video_st, width, height);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0)
            die("video: Could not open '%s'\n", filename);
    }

    /* write the stream header, if any */
    av_write_header(oc);
    return 0;
}

void videoStop(void)
{
    avcodec_close(video_st->codec);
    av_free(picture->data[0]);
    av_free(picture);
    if (tmp_picture) {
        av_free(tmp_picture->data[0]);
        av_free(tmp_picture);
    }
    av_free(video_outbuf);

    /* write the trailer, if any */
    av_write_trailer(oc);

    /* free the streams */
    int i;
    for(i = 0; i < oc->nb_streams; i++) {
        av_freep(&oc->streams[i]->codec);
        av_freep(&oc->streams[i]);
    }

    if (!(fmt->flags & AVFMT_NOFILE)) {
        /* close the output file */
        url_fclose(&oc->pb);
    }

    /* free the stream */
    av_free(oc);
}

#else

void videoWriteFrame(unsigned char *rgbdata, int width, int height)
{
}

int videoStart(const char *filename, int width, int height)
{
   warning("Would begin recording to %s at %dx%d, but was"
           " not compiled with video support\n", filename, width, height);
   return -1;
}

void videoStop(void)
{
}

#endif
