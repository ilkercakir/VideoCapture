#include "VideoQueue.h"

void vqs_init(vqstatus *vqs)
{
	vqs->playerstatus = IDLE;
	vqs->vq = NULL;
	vqs->vqLength = 0;
	vqs->vqMaxLength = 10;
	
	int ret;

	if((ret=pthread_mutex_init(&(vqs->vqmutex), NULL))!=0 )
		printf("mutex init failed, %d\n", ret);

	if((ret=pthread_cond_init(&(vqs->vqlowcond), NULL))!=0 )
		printf("cond init failed, %d\n", ret);

	if((ret=pthread_cond_init(&(vqs->vqhighcond), NULL))!=0 )
		printf("cond init failed, %d\n", ret);
}

void vq_add(vqstatus *vqs, unsigned char *yuyv)
{
	struct videoqueue **q = &(vqs->vq);
	struct videoqueue *p;

	pthread_mutex_lock(&(vqs->vqmutex));
	while (vqs->vqLength>=vqs->vqMaxLength)
	{
		//printf("Video queue sleeping, overrun\n");
		pthread_cond_wait(&(vqs->vqhighcond), &(vqs->vqmutex));
	}

	p = malloc(sizeof(struct videoqueue));
//printf("malloc vq %d\n", sizeof(struct videoqueue));
	if (*q == NULL)
	{
		p->next = p;
		p->prev = p;
		*q = p;
	}
	else
	{
		p->next = *q;
		p->prev = (*q)->prev;
		(*q)->prev = p;
		p->prev->next = p;
	}
	p->yuyv = yuyv;

	vqs->vqLength++;

	//condition = true;
	pthread_cond_signal(&(vqs->vqlowcond)); // Should wake up *one* thread
	pthread_mutex_unlock(&(vqs->vqmutex));
}

struct videoqueue* vq_remove_element(vqstatus *vqs)
{
	struct videoqueue **q = &(vqs->vq);
	struct videoqueue *p;

	if ((*q)->next == (*q))
	{
		p=*q;
		*q = NULL;
	}
	else
	{
		p = (*q);
		(*q) = (*q)->next;
		(*q)->prev = p->prev;
		(*q)->prev->next = (*q);
	}
	return p;
}

struct videoqueue* vq_remove(vqstatus *vqs)
{
	struct videoqueue **q = &(vqs->vq);
	struct videoqueue *p;

	pthread_mutex_lock(&(vqs->vqmutex));
	while((*q)==NULL) // queue empty
	{
		if ((vqs->playerstatus==PLAYING) || (vqs->playerstatus==PAUSED))
		{
			//printf("Video queue sleeping, underrun\n");
			pthread_cond_wait(&(vqs->vqlowcond), &(vqs->vqmutex));
		}
		else
			break;
	}
	switch (vqs->playerstatus)
	{
		case PLAYING:
		case PAUSED:
			p = vq_remove_element(vqs);
			vqs->vqLength--;
			break;
		case DRAINING:
			if (vqs->vqLength>0)
			{
				p = vq_remove_element(vqs);
				vqs->vqLength--;
			}
			else
				p=NULL;
			break;
		default:
			p = NULL;
			break;
	}

	//condition = true;
	pthread_cond_signal(&(vqs->vqhighcond)); // Should wake up *one* thread
	pthread_mutex_unlock(&(vqs->vqmutex));

	return p;
}

void vq_drain(vqstatus *vqs)
{
	//struct videoqueue **q = &(vqs->vq);

	pthread_mutex_lock(&(vqs->vqmutex));
	vqs->playerstatus = DRAINING;
	while (vqs->vqLength)
	{
		pthread_mutex_unlock(&(vqs->vqmutex));
		usleep(100000); // 0.1s
//printf("vqLength=%d\n", vqLength);
		pthread_mutex_lock(&(vqs->vqmutex));
	}
	vqs->playerstatus = IDLE;
	pthread_cond_signal(&(vqs->vqlowcond)); // Should wake up *one* thread
	pthread_mutex_unlock(&(vqs->vqmutex));
//printf("vq_drain exit\n");
}

void vq_destroy(vqstatus *vqs)
{
	pthread_mutex_destroy(&(vqs->vqmutex));
	pthread_cond_destroy(&(vqs->vqlowcond));
	pthread_cond_destroy(&(vqs->vqhighcond));
}
