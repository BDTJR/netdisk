#ifndef _THRADFACTORY_
#define _THRADFACTORY_
#include "workQue.h"

void* threadTask(void* f);

class Factory
{
public:
	Factory() {
       state = 0;
       pthread_cond_init(&cond, NULL);
	}
	
	void startThreadPool() {
		for (int i = 0; i < THREADNUM; i++) {
			pthread_create(&pthid[i], NULL, threadTask, this);
		}
		cout << "Thread pool start success..." << endl;
		sleep(1);
		state = 1;
	}
	pthread_t pthid[THREADNUM + 1]; //线程ID
    WorkQue taskQue; //任务队列
    pthread_cond_t cond; //条件变量 
	int state;
};

#endif
