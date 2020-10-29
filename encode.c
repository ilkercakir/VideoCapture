/*
* Copyright (c) 2001 Fabrice Bellard
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
 
/**
* @file
* video encoding with libavcodec API example
*
* @example encode_video.c
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>

#include "encode.h"

int init_encoder(videoencoder *v, char *filename, int bitrate, int width, int height)
{
	uint8_t code[] = { 0, 0, 1, 0xb7 };
	int ret, i;

	v->c = NULL;
	for(i=0;i<4;i++)
		v->endcode[i] = code[i];
	strcpy(v->filename, filename);

	av_register_all();
	avcodec_register_all();
 
	/* find the video encoder */
	if (!(v->codec = avcodec_find_encoder(AV_CODEC_ID_H264)))
	//if (!(v->codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO)))
	{
		printf("Codec not found\n");
		return -1;
	}
 
	if (!(v->c = avcodec_alloc_context3(v->codec)))
	{
		printf("Could not allocate video codec context\n");
		return -2;
	}

	if (!(v->pkt = av_packet_alloc()))
	{
		printf("Could not allocate packet\n");
		return -3;
	}
 
	/* put sample parameters */
	v->c->bit_rate = bitrate;
	/* resolution must be a multiple of two */
	v->c->width = width;
	v->c->height = height;
	/* frames per second */
	v->c->time_base = (AVRational){1, 25};
	v->c->framerate = (AVRational){25, 1};

	/* emit one intra frame every ten frames
	* check frame pict_type before passing frame
	* to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	* then gop_size is ignored and the output of encoder
	* will always be I frame irrespective to gop_size
	*/
	v->c->gop_size = 10;
	v->c->max_b_frames = 1;
	v->c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (v->codec->id == AV_CODEC_ID_H264)
	{
		// preset: ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo
		av_opt_set(v->c->priv_data, "preset", "veryfast", 0);
		// tune: film, animation, grain, stillimage, psnr, ssim, fastdecode, zerolatency
		av_opt_set(v->c->priv_data, "tune", "zerolatency", 0);
		// profile: baseline, main, high, high10, high422, high444
		av_opt_set(v->c->priv_data, "profile", "main", 0);
	}


	v->c->thread_count = 4;
	/* open it */
	if ((ret = avcodec_open2(v->c, v->codec, NULL)) < 0)
	{
		printf("Could not open codec, %s\n", av_err2str(ret));
		return -4;
	}

	if (!(v->f = fopen(v->filename, "wb")))
	{
		printf("Could not open %s\n", v->filename);
		return -5;
	}

	if (!(v->frame = av_frame_alloc()))
	{
		printf("Could not allocate video frame\n");
		return -6;
	}
	v->frame->pict_type = 0;

	v->frame->format = v->c->pix_fmt;
	v->frame->width = v->c->width;
	v->frame->height = v->c->height;

	if ((ret = av_frame_get_buffer(v->frame, 1)) < 0)
	{
		printf("Could not allocate the video frame data\n");
		return -7;
	}
//printf("linesizes %d %d %d\n", v->frame->linesize[0], v->frame->linesize[1], v->frame->linesize[2]);

	int picsize = v->frame->width * v->frame->height;
	v->uoffset = picsize;
	v->voffset = v->uoffset + picsize / 4;

	return ret;
}

int encode(videoencoder *v, unsigned char *yuv420, long long int pts)
{
	int ret;

	/* send the frame to the encoder */
	if (v->frame)
	{
//printf("Send frame %3"PRId64"\n", pts);
		if ((ret = av_frame_make_writable(v->frame))<0)
		{
			printf("av_frame_make_writable error\n");
			return -1;
		}
		v->frame->data[0] = yuv420;
		v->frame->data[1] = yuv420 + v->uoffset;
		v->frame->data[2] = yuv420 + v->voffset;

		v->frame->pts = pts;
	}

	if ((ret = avcodec_send_frame(v->c, v->frame)) < 0)
	{
		printf("Error sending a frame for encoding\n");
		return ret;
	}

	while (ret >= 0)
	{
		ret = avcodec_receive_packet(v->c, v->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return ret;
		else if (ret < 0)
		{
			printf("Error during encoding\n");
			return ret;
		}

//printf("Write packet %3"PRId64" (size=%5d)\n", v->pkt->pts, v->pkt->size);
		fwrite(v->pkt->data, 1, v->pkt->size, v->f);
		av_packet_unref(v->pkt);
	}

	return ret;
}

void close_encoder(videoencoder *v)
{
	/* flush the encoder */
	av_frame_free(&(v->frame));
	encode(v, NULL, 0);

	/* add sequence end code to have a real MPEG file */
	fwrite(v->endcode, 1, sizeof(v->endcode), v->f);
	fclose(v->f);

	avcodec_free_context(&(v->c));
	av_packet_free(&(v->pkt));
}
