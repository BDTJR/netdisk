#include "threadFactory.h"
#include "function.h"
#include "clients.h"
#include "function.h"
#include "MySQLInterface.h"

void* supervise(void *p);
void serverInit(char* conf,char* ip,char* port,char* absPath);
int fileExist(char* curPath, char* fileName, char fileType);

Factory f;
ConnectedClients clients;

int main(int argc, char* argv[]) {
	  countArgc(argc, 2);
 
	  char ip[20] = {0};
	  char port[10] = {0};
	  char absPath[64] = {0};
	  serverInit(argv[1], ip, port, absPath);

    f.startThreadPool(); //开启线程池, 线程进行put、get任务和监督任务
    int listenfd = tcpInit(ip, port); //TCP初始化
    int connectfd;
    struct sockaddr_in client;
    socklen_t addrlen = sizeof(client);

    
    //设置epoll
    int epfd; //epollfd
    struct epoll_event ev, events[2 * MAXNUM]; //ev用于注册事件，数组用于返回要处理的事件
    memset(&events, 0, sizeof(events));
    ev.events = EPOLLIN; //监听读状态
    ev.data.fd = listenfd; //初始化为监听服务端的监听套接字
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);//注册epoll事件

    //设置log
    int logfd = open("../log/log", O_WRONLY|O_APPEND|O_CREAT, 0666);
    char logBuf[128] = {0}, timeBuf[64] = {0};
    time_t tim;
  
    pthread_create(&f.pthid[THREADNUM], NULL, supervise, &epfd); //监控线程

    int orderFlag = 1;
    while (1) {
    	int nfds = epoll_wait(epfd, events, MAXNUM, -1);
    	for (int i = 0; i < nfds; i++) {
    		if (events[i].data.fd == listenfd) { //服务器从CLOSED变为LISTEN
                if (connectfd = accept(listenfd, (struct sockaddr *)&client, &addrlen) == -1) { //接收客户端的连接请求
                	perror("accept() error.");
                	return -1;
                }
                //成功与服务端连接，写入log
                printf("client %s %d connected.\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                tim = time(NULL);
                ctime_r(&tim, timeBuf);
                sprintf(logBuf, "client %s %d connected.\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), timeBuf);
                write(logfd, logBuf, strlen(logBuf));
                //用户数量已满
                if (clients.getCurrentClients() >= MAXNUM) {
                    close(listenfd);
                }
                else { //加入一个新用户
                   Client newClient(connectfd, 0, absPath);
                   clients.addCurrentClients(connectfd, &newClient); //将新连接的客户端加入mp，以connectfd为索引
                   clients.insertCircularQue(connectfd); //将新连接的客户端加入环形队列
                   //注册epoll事件
                   ev.data.fd = connectfd;
                   epoll_ctl(epfd, EPOLL_CTL_ADD, connectfd, &ev);
                }
    		}
    		else {   //监控到connectfd可读
    			      connectfd = events[i].data.fd; //取出连接套接字
                Client* oldClient = clients.getClient(connectfd); //连接的客户端
                if (oldClient->state == 0) { //刚连接
                   if (recv_n(connectfd, &orderFlag, sizeof(int)) == -1) {
                       ev.data.fd = connectfd;
                       epoll_ctl(epfd, EPOLL_CTL_DEL, connectfd, &ev);
                       clients.cutCurrentClients(connectfd);
                       continue;
                   }
                   if (orderFlag == 1) { //登录
                   	   clients.updateClient(connectfd, 1);
                   	   
                   }
                   else if (orderFlag == 2) { //注册
                   	   clients.updateClient(connectfd, 2);
                   	  
                   }
                }
                else if (oldClient->state == 1) {  //登录
                	if (clients.clientLogin(connectfd) == -1) {
                       ev.data.fd = connectfd;
                       epoll_ctl(epfd, EPOLL_CTL_DEL, connectfd, &ev);
                       clients.cutCurrentClients(connectfd);
                       continue;
                	}
                }
                else if (oldClient->state == 2) { //注册
                	if (clients.clientRegister(connectfd) == -1) {
                       ev.data.fd = connectfd;
                       epoll_ctl(epfd, EPOLL_CTL_DEL, connectfd, &ev);
                       clients.cutCurrentClients(connectfd);
                       continue;
                	}
                }
                else if (oldClient->state == 3) {
                    if (clients.clientOperation(connectfd, logfd) == -1) {
                       cout<<"Client exits."<<endl;
                       ev.data.fd = connectfd;
                       epoll_ctl(epfd, EPOLL_CTL_DEL, connectfd, &ev);
                       clients.cutCurrentClients(connectfd);
                       continue;
                	}
                }
    		}
    	}
    }
    close(epfd);
    close(logfd);
    close(connectfd);
}

void taskHandler(int connectfd) {
   pthread_mutex_lock(&f.taskQue.queMutex);
   if (f.taskQue.getCurrentSize() == f.taskQue.capacity) { //任务队列已满
      clients.cutCurrentClients(connectfd);
      int dataLen = -1;
      send_n(connectfd, &dataLen, sizeof(int));
   }
   else {
      Client* task = clients.getClient(connectfd); //取出connectfd对应的客户
      f.taskQue.queInsert(*(task));
      //ev.data.fd = connectfd;
      //epoll_ctl(epfd, EPOLL_CTL_DEL, connectfd, &ev);
      //clients.currentClients--;
      pthread_cond_signal(&f.cond);

   }
   pthread_mutex_unlock(&f.taskQue.queMutex);
}

void* threadTask(void* p){ 
   Factory* f = (Factory *) p;
   int dataLen;
   char buf[128] = {0};
   char operands[96] = {0};
   char fileName[128] = {0};
   string sql;
   struct stat sta;
   long fileSize;
   
   MySQLInterface db;
   db.connectMySQL("localhost", "rookie", "123", "netdisk");

   while (1) {
      pthread_mutex_lock(&f->taskQue.queMutex);
      if (f->taskQue.getCurrentSize() == 0) {
         pthread_cond_wait(&f->cond, &f->taskQue.queMutex);
      }
      Client task = f->taskQue.quePop();
      pthread_mutex_unlock(&f->taskQue.queMutex);
      int connectfd = task.connectfd;
      //重新接收命令
      recv_n(connectfd, &dataLen, sizeof(int));
      recv_n(connectfd, buf, dataLen);
      string orders(buf);
      stringstream ss(orders);
      string operate, operand;
      ss >> operate >> operand;
      strcpy(operands, operand.c_str());

      if (task.taskType == 1) {  //puts
          //查找数据库,看虚拟目录是否有相同文件
           recv_n(connectfd, &dataLen, sizeof(int));
           recv_n(connectfd, fileName, dataLen);//文件hash值
           buf[dataLen] = 0;
           string name(fileName);
           sql = "SELECT fileName FROM fileInfo WHERE fileName = '" + name + "'";
           int orderFlag = db.sqlFind(sql);
           if (orderFlag == 1) { //虚拟目录存在此文件,执行秒传
                orderFlag = 2;
                send_n(connectfd, &orderFlag, sizeof(int));
                strcpy(buf, clients.mp[connectfd]->virtualAddr);
                buf[strlen(buf) + 1] = 0;
                buf[strlen(buf)] = '/';
                strcat(buf, fileName);//fileName是虚拟目录
                strcpy(fileName, clients.mp[connectfd]->absPath);
                strcat(fileName, clients.mp[connectfd]->curPath);
                fileName[strlen(fileName) + 1] = 0;
                fileName[strlen(fileName)] = '/';
                strcat(fileName, operands);//fileName是用户目录
                link(buf, fileName);
                continue;
           }
           strcpy(buf, clients.mp[connectfd]->absPath);
           strcat(buf, clients.mp[connectfd]->curPath);
           orderFlag = fileExist(buf, operands, '-');
           if (orderFlag == 1) { //文件存在，断点续传
               send_n(connectfd, &orderFlag, sizeof(int));
               int index = strlen(buf);
               buf[strlen(buf) + 1] = 0;
               buf[strlen(buf)] = '/';
               strcat(buf, operands);
               stat(buf, &sta);
               send_n(connectfd, &sta.st_size, sizeof(long));
               fileSize = sta.st_size;
               buf[index] = 0;
           }
           else if (orderFlag == 0) {
               fileSize = 0; 
               send_n(connectfd, &orderFlag, sizeof(int));
           }
           dataLen = recvFile(connectfd, buf, fileSize);
           if (dataLen == 0) {
              string filename(fileName);
              sql = "INSERT INTO fileInfo VALUES('" + filename + "')";
              strcpy(buf, clients.mp[connectfd]->virtualAddr);
              buf[strlen(buf) + 1] = 0;
              buf[strlen(buf)] = '/';  
              strcat(buf, fileName);//buf为虚拟目录路径
              strcpy(fileName, clients.mp[connectfd]->absPath);
              strcat(fileName, clients.mp[connectfd]->curPath);
              fileName[strlen(fileName) + 1]=0;
              fileName[strlen(fileName)] = '/';
              strcat(fileName, operands);//fileName为用户空间目录路径
              link(fileName, buf);//建立硬链接
              db.sqlChange(sql);
           }
           else if (dataLen == -1) {
              perror("puts failed.");
           }       
      }
      else if (task.taskType == 2) {  //gets
           recv_n(connectfd, &dataLen,sizeof(int));
            if(dataLen == 1){//文件存在,进行断点续传
                recv_n(connectfd, &fileSize, sizeof(int));
            }
            else{
                fileSize=0;
            }
            strcpy(buf, clients.mp[connectfd]->absPath);
            strcat(buf, clients.mp[connectfd]->curPath);
            buf[strlen(buf) + 1] = 0;
            buf[strlen(buf)] = '/';
            strcat(buf, operands);
            dataLen = sendFile(connectfd, buf, fileSize);
            if(dataLen == 0) cout << "gets success." << endl;
            else if(dataLen == -1) perror("gets failed.");
      }
  }
}


int fileExist(char* curPath, char* fileName, char fileType){
    DIR *dir=opendir(curPath);
    struct dirent *dnt;
    char buf[128] = {0};
    strcpy(buf, curPath);
    buf[strlen(buf)] = '/';
    int index = strlen(buf);
    struct stat sta;
    char type;
    while((dnt = readdir(dir)) != NULL){
        buf[index] = 0;
        strcat(buf, dnt->d_name);
        stat(buf,&sta);
        if(sta.st_mode >> 12 == 4){//4是目录
            type='d';
        }
        else if(sta.st_mode >> 12 == 8){//8是文件
            type='-';
        }
        if(strcmp(dnt->d_name, fileName)==0 && fileType == type){
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}


void* supervise(void *p){
    //每扫描一次将set里面有的fd关闭,表明30秒超时
    set<int>::iterator it;
    int epfd = *(int*)p;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    while (1) {
        sleep(1);
        set<int> fdSet = clients.getCurFdSet();
        for (it = fdSet.begin(); it != fdSet.end(); it++) {
            ev.data.fd = *it;
            epoll_ctl(epfd, EPOLL_CTL_DEL, *it, &ev);
            clients.cutCurrentClients(*it);
        }
    }
}


void serverInit(char* conf,char* ip,char* port,char* absPath){
    int fd=open(conf, O_RDONLY);
    char buf[128]={0};
    read(fd,buf,sizeof(buf));
    close(fd);
    int cnt=0;
    int start=0;
    while(buf[start+cnt]!='\n'){
        cnt++;
    }
    strncpy(ip,buf,cnt);
    start=start+cnt+1;
    cnt=0;
    while(buf[start+cnt]!='\n'){
        cnt++;
    }
    strncpy(port,buf+start,cnt);
    start=start+cnt+1;
    while(buf[start+cnt]!='\n'){
        cnt++;
    }
    strncpy(absPath,buf+start,cnt);
}
