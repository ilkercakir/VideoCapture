#ifndef encodeH
#define encodeH

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>

typedef struct venc
{
	AVCodec *codec;
	AVCodecContext *c;
	char filename[512];
	FILE *f;
	AVFrame *frame;
	AVPacket *pkt;
	uint8_t endcode[4];
	int uoffset, voffset;
}videoencoder;

int init_encoder(videoencoder *v, char *filename, int bitrate, int width, int height);
int encode(videoencoder *v, unsigned char *yuv420, long long int pts);
void close_encoder(videoencoder *v);
#endif
