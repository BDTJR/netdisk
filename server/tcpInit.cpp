#include "function.h"

int tcpInit(char* ip, char* port) {
	int listenfd;
	struct sockaddr_in server;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { //创建TCP套接字
        perror("socket() error");   //处理异常
        return -1;
	}
	int opt = SO_REUSEADDR;  //设置套接字地址重用选项
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(port));
	server.sin_addr.s_addr = inet_addr(ip);
	if (bind(listenfd, (struct sockaddr *) &server, sizeof(server)) == -1) {
		perror("bind() error");
		return -1;
	}
	if (listen(listenfd, MAXNUM) == -1) {
		perror("listen() error");
		return -1;
	}
	return listenfd;
}