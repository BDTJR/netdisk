#include "function.h"

void* threadTask(void* s);//子线程执行puts和gets
int fileExist(char* curPath, char* fileName, char fileType);

char ip[20] = {0};
char port[10] = {0};

int main(int argc, char* argv[]) {
	countArgc(argc, 3);

	strcpy(ip,argv[1]);
  strcpy(port,argv[2]);

  int sockfd;
  struct sockaddr_in server;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    	perror("socket() error.");
    	return -1;
  }
  bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(port));
	server.sin_addr.s_addr = inet_addr(ip);
	if (connect(sockfd,(struct sockaddr*)&server,sizeof(server)) == -1) {
		perror("connect() error.");
    	return -1;
	}
	cout << "Connected success." << endl;

	int orderFlag;
	string username;
	string password;
  string salt;
  char step = '1';
  int dataLen;
  char buf[128] = {0};
  long fileSize;
  pthread_t pthid;

begin:
    cout<<"Please enter 1 or 2 to login or register:"<<endl;
    cout << "1.login" << endl;
    cout << "2.register" << endl;
    cin >> orderFlag;
    while (orderFlag !=1 && orderFlag != 2) {
        cout << "Please enter 1 or 2 to login or register:" << endl;
        cin >> orderFlag;
    }
    send_n(sockfd, &orderFlag, sizeof(int));
    if (orderFlag == 1) {
       cout << "Please input your username:";
       cin >> username;
       cout << "Please input your password:";
       cin >> password;
       step = '1'; //第一步：发送用户名
       dataLen = strlen(username.c_str());
       memcpy(buf, username.c_str(), dataLen);
       send_n(sockfd, &step, 1);
       send_n(sockfd, &dataLen, sizeof(int));
       send_n(sockfd, buf, dataLen);
       memset(buf,0,sizeof(buf));  //接收盐值
       recv_n(sockfd, &dataLen, sizeof(int));
       if (dataLen == 0) {
          cout << "Username does not exist." << endl;
          goto begin;
       }
       recv_n(sockfd, buf, dataLen);
       //加密接收的盐值再发送
       char *cipher = crypt(password.c_str(), buf);
       dataLen = strlen(cipher);
       memcpy(buf, cipher, dataLen); 
       step = '2'; //第二步：发送密码
       send_n(sockfd, &step, 1);
       send_n(sockfd, &dataLen, sizeof(int));
       send_n(sockfd, buf, dataLen);
       recv_n(sockfd, &orderFlag, sizeof(int));
       if (orderFlag == 1) {
           cout << "Login success." << endl;
           goto operation;
       }
       else if (orderFlag == 0) {
           cout << "Login fail." << endl;
           return -1;
       }
    }
    else {  //注册
       cout << "Please input your username:";
       cin >> username;
       cout << "Please input your password:";
       cin >> password;
       step = '1'; //第一步：发送用户名
       send_n(sockfd, &step, 1);
       dataLen = strlen(username.c_str());
       memcpy(buf, username.c_str(), dataLen);
       send_n(sockfd, &dataLen, sizeof(int));
       send_n(sockfd, buf, dataLen);
       recv_n(sockfd, &orderFlag, sizeof(int));
       while (orderFlag == 0) {
           cout << "Username has already existed, please input a new name:" << endl;
           username = "";
           cin >> username;
           dataLen = strlen(username.c_str());
           memcpy(buf, username.c_str(), dataLen);
           send_n(sockfd, &dataLen, sizeof(int));
           send_n(sockfd, buf, dataLen);
           recv_n(sockfd, &orderFlag, sizeof(int));
       }
       string salt(saltGenerator());
       dataLen = strlen(salt.c_str());
       memcpy(buf, salt.c_str(), dataLen);
       //发送盐值
       send_n(sockfd, &dataLen, sizeof(int));
       send_n(sockfd, buf, dataLen);
       char *cipher = crypt(password.c_str(), buf);
       dataLen = strlen(cipher);
       memcpy(buf, cipher, dataLen); 
       step = '2'; //第二步：发送密码
       send_n(sockfd, &step, 1);
       send_n(sockfd, &dataLen, sizeof(int));
       send_n(sockfd, buf, dataLen);
       cout<<"Register success."<<endl;
       goto begin;
    }
    while (1);
operation:
    while (1) {
       cout << username << "@client:";
       fflush(stdout);
       bzero(buf, sizeof(buf));
       if (dataLen = read(STDIN_FILENO, buf, sizeof(buf)) == -1) {
          perror("read() error.");
          return -1;
       }
       dataLen = strlen(buf);
       //确认连接是否因超时而关闭
       if (send_n(sockfd, &dataLen, sizeof(int)) == -1) {
          cout<<"Connection close, no operation in 30 seconds."<<endl;
          goto end;
       }
       if (send_n(sockfd, &buf, dataLen) == -1) {
          cout<<"Connection close, no operation in 30 seconds."<<endl;
          goto end;
       }
       string orders(buf);
       stringstream ss(orders);
       string operate, operand;
       ss >> operate >> operand;       
       if (operate == "cd") {
          orderFlag = recv_n(sockfd, &dataLen, sizeof(int));
          if (orderFlag == -1) {
             cout << "Connection close." << endl;
             goto end;
          }
          if (dataLen == 0) {
             cout << "No such file or directory." << endl;
             continue;
          }
       }
       else if(operate == "ls") {
          cout << "type                        name                        size\n";
          while (1) {
              orderFlag = recv_n(sockfd, buf, 1); //接收文件类型
              if (orderFlag == -1) {
                 cout << "Connection close." << endl;
                 goto end;
              }
              if(buf[0] == 0)  break;
              cout << buf <<"                           ";
              recv_n(sockfd, &dataLen, sizeof(int)); //接收文件名
              recv_n(sockfd, buf, dataLen); 
              buf[dataLen] = '\0';
              cout << buf <<"                           ";
              recv_n(sockfd, &fileSize, sizeof(long)); //接收文件大小
              cout << fileSize <<"B" << endl;
          }
       }
       else if (operate == "rm") {
          orderFlag = recv_n(sockfd, &dataLen, sizeof(int));
          if (orderFlag == -1) {
             cout << "Connection close." << endl;
             goto end;
          }
          if (dataLen == 0) {
             cout << "No such file or directory." << endl;
             continue;
          }
       }
       else if (operate == "mkdir") {
          orderFlag = recv_n(sockfd, &dataLen, sizeof(int));
          if (orderFlag == -1) {
             cout << "Connection close." << endl;
             goto end;
          }
          if (dataLen == -1) {
             cout << "File or directory has already existed." << endl;
             continue;
          }
       }
       else if (operate == "pwd") {
           memset(buf,0,sizeof(buf));
           recv_n(sockfd, &dataLen, sizeof(int));
           if (recv_n(sockfd, buf, dataLen) == -1) {
              cout << "Connection close." << endl;
              goto end;
           }
           cout << buf << endl;
       }
       else if (operate == "puts") {
           int fd = open(operand.c_str(), O_RDONLY);
           if (fd == -1) {
              cout << "No such file or directory." << endl;
              continue;
           }
           if (send_n(sockfd, &fd, sizeof(int)) == -1) {
              cout << "Connection close." << endl;
              goto end;
           }
           recv_n(sockfd, &orderFlag, sizeof(int));
           if (orderFlag == -1) {
              cout << "File or directory has already existed." << endl;
              continue;
           } 
           memset(buf,0,sizeof(buf));
           strcpy(buf, orders.c_str());
           pthread_create(&pthid, NULL, threadTask, buf); //子线程执行传文件任务
           pthread_join(pthid, NULL); 
       }
       else if (operate == "gets") {
           orderFlag = recv_n(sockfd, &dataLen, sizeof(int));
           if (orderFlag == -1) {
              cout << "Connection close." << endl;
              goto end;
           }
           if (dataLen == -1) {
              cout << "File or directory has already existed." << endl;
              continue;
           }
           memset(buf,0,sizeof(buf));
           strcpy(buf, orders.c_str());
           pthread_create(&pthid, NULL, threadTask, buf);
           pthread_join(pthid, NULL);
       }
    }
end:
    close(sockfd);
    return 0;
}

void *threadTask(void* s) {
     string operation((char*)s);
     int dataLen;
     char buf[128] = {0};
     struct stat sta;
     char *pwd,*secret;
     long fileSize=0;
     stringstream ss(operation); 
     string operate, operand;
     ss >> operate >> operand;
     char operands[96] = {0};
     strcpy(operands, operand.c_str());   
     //子线程重新连接开启新的描述符
    int sockfd; 
    struct sockaddr_in server;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket() error.");   //处理异常
        pthread_exit(NULL);
    }
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(port));
    server.sin_addr.s_addr = inet_addr(ip);
    if (connect(sockfd,(struct sockaddr*)&server,sizeof(server)) == -1) {
        perror("connect() error.");
        pthread_exit(NULL);
    }
    recv_n(sockfd, &dataLen, sizeof(int)); //服务端任务队列已满
    if(dataLen == -1){
        cout << "puts failed." << endl;
        close(sockfd);
        return (void*) - 1;
    }
    //重传命令给服务器执行get、put的子线程，传递所需的文件名
    strcpy(buf, operation.c_str());
    dataLen = strlen(buf);
    send_n(sockfd, &dataLen, sizeof(int));
    send_n(sockfd, buf, dataLen);
    if (operate == "puts") {
         //生成文件Hash值
        int fd = open(operands, O_RDONLY);
        stat(operands, &sta);
        pwd = (char*)mmap(NULL, sta.st_size, PROT_READ, MAP_SHARED, fd, 0);
        secret =(char*)MD5((unsigned char*)pwd, (unsigned long)sta.st_size, NULL);
        munmap(pwd, sta.st_size);
        close(fd);
        dataLen = strlen(secret);
        send_n(sockfd, &dataLen, sizeof(int));
        send_n(sockfd, secret, dataLen);
        recv_n(sockfd, &dataLen, sizeof(int));
        if (dataLen == 2)  {//秒传
            cout<<"100.00%"<<endl;
            cout<<"puts success"<<endl;
        }
        else if (dataLen == 1) {//断点续传
            recv_n(sockfd, &fileSize, sizeof(long));
            dataLen = sendFile(sockfd, operands, fileSize);
            if(dataLen == 0) cout<<"puts success."<<endl;
            else if(dataLen == -1)  perror("puts failed.");
        }
        else  if (dataLen == 0) {
            fileSize = 0;
            dataLen = sendFile(sockfd, operands, fileSize);
            if(dataLen == 0) cout<<"puts success."<<endl;
            else if(dataLen == -1)  perror("puts failed.");
        }
    }
    else if (operate == "gets") {
        dataLen = fileExist((char*)".", operands, '-');
        send_n(sockfd, &dataLen, sizeof(int));
        if (dataLen == 1) { //文件存在,进行断点续传
            stat(operands, &sta);
            fileSize = sta.st_size;
            send_n(sockfd, &fileSize, sizeof(long));
        }
        else {
            fileSize = 0;
        }
        dataLen = recvFile(sockfd, operands, fileSize);
        if (dataLen == 0)  cout << "gets success." << endl;
        else if (dataLen == -1) {
          perror("gets failed.");
        }
    }
    fflush(stdout);
    close(sockfd);
    return (void*)0;
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


