#ifndef encodeH
#define encodeH

typedef struct venc
{
	AVCodec *codec;
	AVCodecContext *c;
	char filename[512];
	FILE *f;
	AVFrame *frame;
	AVPacket *pkt;
	uint8_t endcode[4];
}videoencoder;

#endif
