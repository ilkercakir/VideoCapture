#ifndef VideoQueueH
#define VideoQueueH

#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>

enum playerstate
{
	IDLE = 0,
	PLAYING,
	PAUSED,
	DRAINING
};

struct videoqueue
{
	struct videoqueue *prev;
	struct videoqueue *next;
	unsigned char *yuyv; // YUV422 byte array
};

typedef struct 
{
	enum playerstate playerstatus;
	struct videoqueue *vq;
	int vqLength;
	int vqMaxLength;
	pthread_mutex_t vqmutex;
	pthread_cond_t vqlowcond;
	pthread_cond_t vqhighcond;
}vqstatus;

void vqs_init(vqstatus *vqs);
void vq_add(vqstatus *vqs, unsigned char *yuyv);
struct videoqueue* vq_remove_element(vqstatus *vqs);
struct videoqueue* vq_remove(vqstatus *vqs);
void vq_drain(vqstatus *vqs);
void vq_destroy(vqstatus *vqs);
#endif
