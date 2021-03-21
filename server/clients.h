#ifndef _CLIENTS_
#define _CLIENTS_
#include "workQue.h"
#include "function.h"
#include "MySQLInterface.h"

void taskHandler(int connectfd);
int fileExist(char* curPath, char* fileName, char fileType);


class ConnectedClients
{
public:
	ConnectedClients() {
		currentClients = 0; //当前连接的用户数量
		currentIndex = 0; //环形队列中正在检查的slot
	}
	int getCurrentClients() {
		return currentClients;
	}
	void addCurrentClients(int connectfd, Client* newClient) {
		currentClients++;
		mp[connectfd] = newClient;
	}
	void cutCurrentClients(int connectfd) { //用户断开连接
        currentClients--;
        free(mp[connectfd]);
        mp.erase(connectfd);
        close(connectfd);
	}
	void insertCircularQue(int connectfd) {
		pthread_mutex_lock(&setMutex);
		mp[connectfd]->circularQueIndex = currentIndex == 0 ? TIMEOUT : currentIndex - 1;
    circularQue[mp[connectfd]->circularQueIndex].insert(connectfd); //将用户加入环形队列
    pthread_mutex_unlock(&setMutex);
	}
	Client* getClient(int connectfd) {
		pthread_mutex_lock(&setMutex); //更新环形队列
		circularQue[mp[connectfd]->circularQueIndex].erase(connectfd); //将客户移出原来所在的环形队列位置
    mp[connectfd]->circularQueIndex = currentIndex == 0 ? TIMEOUT : currentIndex - 1; //更新位置
	  circularQue[mp[connectfd]->circularQueIndex].insert(connectfd); //将用户加入环形队列
    pthread_mutex_unlock(&setMutex);
	  return mp[connectfd];
	}
	void updateClient(int connectfd, int state) {
         mp[connectfd]->updateState(state);
	} 
	set<int> getCurFdSet() {
		pthread_mutex_lock(&setMutex); 
		set<int> res = circularQue[currentIndex];
		circularQue[currentIndex].clear();
		currentIndex = (currentIndex + 1) % (TIMEOUT + 1);
		pthread_mutex_unlock(&setMutex); 
    return res;
	}
	int clientLogin(int connectfd);
	int clientRegister(int connectfd);
	int clientOperation(int connectfd, int logfd);
	set<int> circularQue[TIMEOUT + 1];  //每个slot是连接上的客户端集合，会被多个线程访问
	map<int, Client*> mp;
	int  currentIndex; //环形队列中正在检查的slot
	int currentClients;
	pthread_mutex_t setMutex;
  MySQLInterface db;
};

int ConnectedClients::clientLogin(int connectfd) {  //客户端的登录操作

    db.connectMySQL("localhost", "rookie", "123", "netdisk");
    char step = '0';
    char buf[128]={0};
    char cipher[128]={0};
    char salt[32]={0};
    int dataLen;
    string sql;
    recv_n(connectfd, &step, 1);
    if (step == '1') {  //接收用户名，发送盐值
    	recv_n(connectfd, &dataLen, sizeof(int));
        recv_n(connectfd, buf, dataLen);
        buf[dataLen] = '\0';
        string username(buf);
        sql = "SELECT salt FROM userInfo WHERE username = '" + username + "'";
        db.sqlSelect(sql, salt);
        dataLen = strlen(salt);
        send_n(connectfd, &dataLen, sizeof(int));
        send_n(connectfd, salt, dataLen);
        strcpy(mp[connectfd]->userName, username.c_str());
        return 0;
    }
    else if (step == '2') { 
    	strcpy(buf, mp[connectfd]->userName); //取出用户名
    	dataLen = strlen(buf);
    	buf[dataLen] = '\0';
    	string username(buf);
    	memset(buf, 0, sizeof(buf));
        recv_n(connectfd, &dataLen, sizeof(int));
        recv_n(connectfd, buf, dataLen); //接收盐值加密后的密码
        sql = "SELECT password FROM userInfo WHERE username = '" + username + "'";
        db.sqlSelect(sql, cipher);
        if (strcmp(cipher, buf) == 0) { //登录成功
        	dataLen = 1;
        	send_n(connectfd, &dataLen, sizeof(int));
        	mp[connectfd]->absPath[strlen(mp[connectfd]->absPath)] = '/';
        	strcat(mp[connectfd]->absPath, mp[connectfd]->userName);
        	mp[connectfd]->state = 3;
        }
        else {
        	dataLen = 0;
        	send_n(connectfd, &dataLen, sizeof(int));
        }
        return 0;
    }
    return -1;
}

int ConnectedClients::clientRegister(int connectfd) {

    db.connectMySQL("localhost", "rookie", "123", "netdisk");
	  char step = '0';
    char buf[128]={0};
    int dataLen;
    string sql;
    recv_n(connectfd, &step, 1);
    if (step == '1') { //接收用户名
        recv_n(connectfd, &dataLen, sizeof(int));
        recv_n(connectfd, buf, dataLen);
        buf[dataLen] = '\0';
        string username(buf);
        sql = "SELECT * FROM userInfo WHERE username = '" + username + "'";
        dataLen = db.sqlFind(sql);
        send_n(connectfd, &dataLen, sizeof(int));
        if (dataLen) {
        	strcpy(mp[connectfd]->userName, username.c_str());
        }
        return 0;
    }
    else if (step == '2') {//接收密码
    	strcpy(buf, mp[connectfd]->userName); 
    	dataLen = strlen(buf);
    	buf[dataLen] = '\0';
    	string username(buf); 
    	memset(buf, 0, sizeof(buf)); //取出用户名
        recv_n(connectfd, &dataLen, sizeof(int));
        recv_n(connectfd, buf, dataLen); //接收盐值
        dataLen = strlen(buf);
    	buf[dataLen] = '\0';
    	string salt(buf);
    	memset(buf, 0, sizeof(buf));
    	recv_n(connectfd, &dataLen, sizeof(int));
        recv_n(connectfd, buf, dataLen); //接收密码
        buf[dataLen] = '\0';
    	string cipher(buf);
    	sql = "INSERT INTO userInfo(username, salt, password) VALUES('" + username + "', '" + salt + "', '" + cipher + "')";
        db.sqlChange(sql);
        memset(buf, 0, sizeof(buf));
        strcpy(buf, mp[connectfd]->absPath);
        mp[connectfd]->absPath[strlen(mp[connectfd]->absPath)] = '/';
        strcat(mp[connectfd]->absPath, mp[connectfd]->userName);
        mkdir(buf, 0775);
        mp[connectfd]->state = 0;
        return 0;
    }
    return -1;
}

int ConnectedClients::clientOperation(int connectfd, int logfd) {

    db.connectMySQL("localhost", "rookie", "123", "netdisk");

	  char buf[128]={0};
	  char log[128]={0};
	  char sql[216]={0};
    char operands[96] = {0};
	  int dataLen, fd, orderFlag;
    
    time_t tim;
    tim = time(NULL);

    if (recv_n(connectfd, &dataLen, sizeof(int)) == -1) {
    	return -1;
    }
    if (recv_n(connectfd, buf, dataLen) == -1) {
    	return -1;
    }
    string operation(buf);
    stringstream ss(operation);
    string operate, operand;
    ss >> operate >> operand; 
    strcpy(operands, operand.c_str());
    if (operate == "cd") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       if (operand == ".") {
           dataLen = 1;
           send_n(connectfd, &dataLen, sizeof(int));
           return 0;
       }
       else if (operand == "..") {
       	   if (strlen(mp[connectfd]->curPath) == 0) {
       	   	   dataLen = 1;
               send_n(connectfd, &dataLen, sizeof(int));
               return 0;
       	   }
       	   int index = strlen(mp[connectfd]->curPath) - 1;
       	   while (index >= 0 && mp[connectfd]->curPath[index] != '/') {
       	   	   index--;
       	   }
       	   if (index >= 0) mp[connectfd]->curPath[index] = 0;
       }
       else if (operand[0] == '/')  { //切换同级目录 
       	   strcat(mp[connectfd]->curPath, operands);
       }
       else {
       	   mp[connectfd]->curPath[strlen(mp[connectfd]->curPath)] = 0;
       	   strcpy(buf, mp[connectfd]->absPath);
       	   strcat(buf, mp[connectfd]->curPath);
       	   dataLen = fileExist(buf, operands, 'd');
       	   if (dataLen == 0) {
       	   	   send_n(connectfd, &dataLen, sizeof(int));
       	   	   return 0;
       	   }
       	   mp[connectfd]->curPath[strlen(mp[connectfd]->curPath)] = '/';
       	   strcat(mp[connectfd]->curPath, operands);
       }
       dataLen = 1;
       send_n(connectfd, &dataLen, sizeof(int));
    }
    else if (operate == "ls") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       memset(buf, 0, sizeof(buf));
       strcpy(buf, mp[connectfd]->absPath);
       strcat(buf, mp[connectfd]->curPath);
       DIR *dir = opendir(buf);
       int index = strlen(buf);
       buf[index] = '/';
       index++;
       struct dirent *dnt = readdir(dir);
       struct stat sta;
       while (dnt != nullptr) {
          if(dnt->d_name[0]=='.'||!strcmp("..",dnt->d_name)||!strcmp(".",dnt->d_name)) continue;
          buf[index]=0;
          strcat(buf,dnt->d_name);
          stat(buf, &sta);
          //传文件类型
          char fileType;
          dataLen = sta.st_mode >> 12;
            if(dataLen == 8){//8为文件
                fileType = '-';
                send_n(connectfd, &fileType, 1);
            }
            else if(dataLen == 4){//4为目录
                fileType = 'd';
                send_n(connectfd, &fileType, 1);
            }
             //传文件名
            strcpy(buf, dnt->d_name);
            dataLen = strlen(buf);
            send_n(connectfd, &dataLen, sizeof(int));
            send_n(connectfd, buf, dataLen);
            
            //传文件大小
            send_n(connectfd, &sta.st_size, sizeof(long));
       }
       dataLen = 0;
       send_n(connectfd, &dataLen, sizeof(int));
       closedir(dir);
       dir = nullptr;
    }
    else if (operate == "rm") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       memset(buf, 0, sizeof(buf));
       strcpy(buf, mp[connectfd]->absPath);
       strcat(buf, mp[connectfd]->curPath);
       dataLen = fileExist(buf, operands, 'd');
       if (dataLen == 1) {
       	  send_n(connectfd, &dataLen, sizeof(int));
          buf[strlen(buf) + 1] = 0;
          buf[strlen(buf)] = '/';
          strcat(buf, operands);
          remove(buf);
          return 0;
       }
       dataLen = fileExist(buf, operands, '-');
       if (dataLen == 1) {
       	  send_n(connectfd, &dataLen, sizeof(int));
       	  buf[strlen(buf) + 1] = 0;
          buf[strlen(buf)] = '/';
          strcat(buf, operands);
          struct stat sta;
          stat(buf,&sta);
          if(sta.st_nlink > 2){
              remove(buf);
          }
          else{ //删除用户目录文件、虚拟目录文件
                int fd = open(buf, O_RDONLY);
                char *pwd = (char*)mmap(NULL, sta.st_size, PROT_READ, MAP_SHARED, fd, 0);
                char *secret=(char*)MD5((unsigned char*)pwd, sta.st_size, NULL);
                munmap(pwd, sta.st_size);
                close(fd);
                char buf1[128]={0};
                strcpy(buf1, VirtualPath);
                buf1[strlen(buf1) + 1]=0;
                buf1[strlen(buf1)]='/';
                strcat(buf1, secret);
                sprintf(sql,"dELETE FROM fileInfo WHERE fileName ='%s'", secret);
                db.sqlChange(sql);
                remove(buf);
                remove(buf1);
            }
            return 0;
       }
       else if (dataLen == 0) {
       	   send_n(connectfd, &dataLen, sizeof(int));
       	   return 0;
       }
    }
    else if (operate == "mkdir") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       memset(buf, 0, sizeof(buf));
       strcpy(buf, mp[connectfd]->absPath);
       strcat(buf, mp[connectfd]->curPath);
       dataLen = fileExist(buf, operands, 'd');
       if (dataLen == 1) dataLen = -1; //文件夹已存在
       else if (dataLen == 0) dataLen = fileExist(buf, operands, '-'); //是否有同名文件
       if (dataLen == 1) dataLen = -1; //文件已存在
       send_n(connectfd, &dataLen, sizeof(int));
       if (dataLen == 0) {
       	  buf[strlen(buf) + 1] = 0;
          buf[strlen(buf)] = '/';
          strcat(buf, operands);
          mkdir(buf, 0775);
       }
    }
    else if (operate == "pwd") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       memset(buf, 0, sizeof(buf));
       strcat(buf, mp[connectfd]->curPath);
       if (strlen(buf) == 0) {
       	  buf[0] = '/';
       }
       dataLen = strlen(buf);
       send_n(connectfd, &dataLen, sizeof(int));
       send_n(connectfd, buf, dataLen);
    }
    else if (operate == "puts") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       memset(buf, 0, sizeof(buf));
       strcpy(buf, mp[connectfd]->absPath);
       strcat(buf, mp[connectfd]->curPath);
       recv_n(connectfd, &dataLen, sizeof(int));
       if (dataLen == -1) return 0;
       dataLen = fileExist(buf, operands, 'd');
       if (dataLen == 1) {
       	   dataLen = -1;
       	   send_n(connectfd, &dataLen, sizeof(int));
       	   return 0;
       }
       dataLen = fileExist(buf, operands, '-');
       if (dataLen == 1) {
       	   dataLen = -1;
       	   send_n(connectfd, &dataLen, sizeof(int)); 
       	   return 0;
       }
       send_n(connectfd, &dataLen, sizeof(int));
       if (dataLen == -1) {
       	  mp[connectfd]->taskType = 1;
       	  taskHandler(connectfd);
       }
    }
    else if (operate == "gets") {
       memset(buf, 0, sizeof(buf));
       ctime_r(&tim, buf);
       sprintf(log, "%s: %s %s %s", mp[connectfd]->userName, operate.c_str(), operand.c_str(), buf);
       write(logfd, log, strlen(log));
       memset(buf, 0, sizeof(buf));
       strcpy(buf, mp[connectfd]->absPath);
       strcat(buf, mp[connectfd]->curPath);
       dataLen = fileExist(buf, operands, '-');
       if (dataLen == 0) {
       	  dataLen = -1;
       	  send_n(connectfd, &dataLen, sizeof(int));
       	  return 0;
       }
       else if (dataLen == 1) {
       	 send_n(connectfd, &dataLen, sizeof(int));
       }
       if (dataLen == 1) {
       	  mp[connectfd]->taskType = 1;
       	  taskHandler(connectfd);
       }
    }
}

#endif