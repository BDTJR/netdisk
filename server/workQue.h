#ifndef _WORKQUE_
#define _WORKQUE_
#include "function.h"


class Client 
{
public:
	Client(int fd, int status, char path[64]) : connectfd(fd), state(status) {
        strcpy(virtualAddr, VirtualPath);
        strcpy(absPath, path);
        taskType = 0;
        circularQueIndex = 0;
	}
    void updateState(int status) {
    	state = status;
    }
	int connectfd; //与客户端建立TCP链接的套接字
	int circularQueIndex; //处在环形中的位置
	char absPath[64];
	char curPath[192];
	char userName[30];
	char virtualAddr[64];
	int state; //用户状态：刚刚建立连接为0，登录前为1，注册后为2， 登录后为3
	int taskType; //puts为1， gets为2
};

class WorkQue
{
public:
	WorkQue() {
		pthread_mutex_init(&queMutex, NULL);
        capacity = MAXNUM;
	}
	void queInsert(const Client task) {
         deq.push_back(task);
	}
	Client quePop() {
         Client task = deq.front();
         deq.pop_front();
         return task;
	}
	int getCurrentSize() {
		return deq.size();
	}
	pthread_mutex_t queMutex;
	int capacity;
	deque<Client> deq;
};

#endif
