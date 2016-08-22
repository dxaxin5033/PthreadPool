/*************************************************************************
    > File Name: pthreadpool.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: Thu 11 Aug 2016 11:23:20 PM EDT
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<pthread.h>
#include<errno.h>
#include<unistd.h>
#include<signal.h>
#define DEFAULT_VARY_NUM 10
#define DEFAULT_VARY_USECONDS 1000
#define MIN_WAIT_TASK_NUM 2

void* pthread_function(void *arg);
typedef struct{
	void *(*pthread_fun)(void *);
	void *arg;
}pthread_info_t;
typedef struct{
	pthread_t *pthreadset;
	int min_pthreadnum;
	int max_pthreadnum;
	int busy_pthreadnum;
	int live_pthreadnum;
	int size_queue;
	int livenum_queue;
	pthread_mutex_t busy_mutex;
	pthread_mutex_t live_mutex;
	pthread_cond_t queue_not_full;
	pthread_cond_t queue_not_empty;
	pthread_info_t *queue;
	int queue_head;
	int queue_tail;
	int destroynum;
	int shutdown;
}pthreadpool_t;

/** define global variable ***/
pthreadpool_t *pthreadpool;
pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
/*****/
void sys_err(char *str)
{
	perror(str);
	exit(1);
}
pthreadpool_t *pool_create(int minpthread, int maxpthread, int queuesize)
{
	
	pthreadpool_t *pool;
	
	if((pool =(pthreadpool_t *)malloc(sizeof(pthreadpool_t)))==NULL)
		sys_err("malloc pool");
	pool->queue=(pthread_info_t *)malloc(sizeof(pthread_info_t)*queuesize);
	if(pool->queue==NULL)
		sys_err("malloc queue");
	printf("init finsh\n");
	if((pool->pthreadset=(pthread_t *)malloc(maxpthread*sizeof(pthread_t)))==NULL)
		sys_err("malloc pthreadset");
	memset(pool->pthreadset,0,sizeof(pool->pthreadset));	
	
	pool->min_pthreadnum=minpthread;
	pool->max_pthreadnum=maxpthread;
	pool->busy_pthreadnum=0;
	pool->live_pthreadnum=0;
	pool->size_queue=queuesize;
	pthread_mutex_init(&pool->busy_mutex,NULL);
	pthread_mutex_init(&pool->live_mutex,NULL);
	pthread_cond_init(&pool->queue_not_full,NULL);
	pthread_cond_init(&pool->queue_not_empty,NULL);
	pool->queue_head=0;
	pool->queue_tail=0;
	pool->livenum_queue=0;
	pool->destroynum=0;
	pool->shutdown=0;
	int i;
	for(i=0;i<pool->min_pthreadnum; i++)
	{
		pthread_create(&pool->pthreadset[i],NULL,pthread_function,(void *)pool);
		pthread_mutex_lock(&pool->live_mutex);
		pool->live_pthreadnum++;
		pthread_mutex_unlock(&pool->live_mutex);
		printf("created pthread-id%d\n",pool->pthreadset[i]);
	}
	printf("suceece create a pthreadpool !");
	return pool;


}

void* pthread_function(void *arg)
{
	pthreadpool_t *ppool=(pthreadpool_t *)arg;
	while(ppool->shutdown!=1)
	{
		pthread_mutex_lock(&pool_lock);
		while((ppool->livenum_queue==0)||(ppool->shutdown!=0))
		{
			pthread_cond_wait(&ppool->queue_not_empty,&pool_lock);
			if(ppool->destroynum>0)
			{
				ppool->live_pthreadnum--;
				ppool->destroynum--;
				printf("exit pthread id%d\n",pthread_self());

				pthread_mutex_unlock(&pool_lock);
				pthread_exit(NULL);
			}
		}
		pthread_info_t pinfo=ppool->queue[ppool->queue_head];

		ppool->livenum_queue--;
		ppool->busy_pthreadnum++;
		ppool->queue_head=(ppool->queue_head+1)%(ppool->size_queue);	
		pthread_mutex_unlock(&pool_lock);
		pthread_cond_signal(&ppool->queue_not_full);

		(void *)pinfo.pthread_fun((void *)pinfo.arg);
		printf("pthread %d do_sig end!\n");
		pthread_mutex_lock(&ppool->busy_mutex);
		ppool->busy_pthreadnum--;
		pthread_mutex_unlock(&ppool->busy_mutex);
	}
}
void pth_exit(pthreadpool_t *pl)
{
}
void task_add(pthreadpool_t *ppool ,void *(*do_sig)(void *), void *argv)
{
	pthread_mutex_lock(&pool_lock);
	pthread_info_t ptask;
	while((ppool->livenum_queue>=ppool->size_queue) || (ppool->shutdown==1))
	{
		pthread_cond_wait(&ppool->queue_not_full,&pool_lock);
	}
	ptask.pthread_fun =do_sig;
	ptask.arg=argv;
	ppool->queue[ppool->queue_tail]=ptask;
	ppool->livenum_queue++;
	ppool->queue_tail=(ppool->queue_tail+1)%(ppool->size_queue);
	printf("add a task%d\n",ppool->livenum_queue);
	pthread_mutex_unlock(&pool_lock);
	pthread_cond_signal(&ppool->queue_not_empty);

}
void *do_sig(void* value)
{
	printf("execute pthread%d do_sig\n",pthread_self());
	sleep(5);
}

void* Pthread_manage(void *arg)
{
	pthreadpool_t *ppool=(pthreadpool_t *)arg;
	while(ppool->shutdown!=1)
	{
		usleep(DEFAULT_VARY_USECONDS);
		pthread_mutex_lock(&pool_lock);
		int live_pnum=ppool->live_pthreadnum;
		int max_pnum=ppool->max_pthreadnum;
		int min_pnum=ppool->min_pthreadnum;
		int live_qnum=ppool->livenum_queue;
		pthread_mutex_unlock(&pool_lock);
		pthread_mutex_lock(&ppool->busy_mutex);
		int busy_pnum =ppool->busy_pthreadnum;
		pthread_mutex_unlock(&ppool->busy_mutex);
		int i,j;
		if((busy_pnum*2 <live_pnum)&&(live_pnum >min_pnum))
		{
			pthread_mutex_lock(&pool_lock);
			ppool->destroynum=DEFAULT_VARY_NUM;
			pthread_mutex_unlock(&pool_lock);
			int k;
			for(k=0;k<DEFAULT_VARY_NUM;k++)
				pthread_cond_signal(&ppool->queue_not_empty);
		}else if((live_qnum >=MIN_WAIT_TASK_NUM)&&(live_pnum<max_pnum))
		{
			pthread_mutex_lock(&pool_lock);
			printf("add DEFAULE_VARY_NUM pthread!!\n");
			for(i=0 ,j=0;i<max_pnum;i++)
			{
			 	if(j==DEFAULT_VARY_NUM) 
					break;
				if((ppool->pthreadset[i]==0)||islive(ppool->pthreadset[i])==0)
				{
					pthread_create(&ppool->pthreadset[i],NULL,pthread_function,(void *)ppool);
					ppool->live_pthreadnum++;

					j++;
			 	 	printf("create pthread id%d\n",ppool->live_pthreadnum);
				} 
			}
			pthread_mutex_unlock(&pool_lock);
		}
	}
	printf("manage-pthread exit\n");
}


int islive(pthread_t tid)
{
	int kill_rc= pthread_kill(tid, 0);
	if(kill_rc==ESRCH)
		return 0;
	else
		return 1;
}
#if 1
int main()
{
	pthread_t tid;
	pthreadpool=pool_create(3,100,12);
	
	pthread_create(&tid, NULL,Pthread_manage,(void *)pthreadpool);
	printf("pool init!\n");
	int i;
	for(i=0;i<10;i++)
	{
		task_add(pthreadpool,do_sig,NULL);
	}
	sleep(20);
}
#endif
