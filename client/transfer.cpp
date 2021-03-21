#include "function.h"

int send_n(int connectfd, void *data, int dataLen) {
	int len, total = 0;
	char* p = (char*)data;
	while (total < dataLen) {
		len = send(connectfd, p + total, dataLen - total, MSG_NOSIGNAL);
		if (len = -1) {
			return -1;
		}
        total += len;
	}
	return 0;
}

int recv_n(int connectfd, void* data, int dataLen) {
	int len, total = 0;
	char* p = (char*)data;
	while (total < dataLen) {
		len = recv(connectfd, p + total, dataLen - total, 0);
		if (len == 0 || len == -1) {
			return -1;
		}
		total += len;
	}
	return 0;
}

int sendFile(int connectfd, char* fileName, long existedFileSize) {
     char buf[1024] = {0};
     int dataLen;
     dataLen = strlen(fileName);      //传文件名
     if (send_n(connectfd, &dataLen, sizeof(int)) == -1) {
     	return -1;
     }
     if (send_n(connectfd, fileName, dataLen) == -1) {
     	return -1;
     }
     
    int fd = open(fileName, O_RDONLY);
    struct stat fileStat;
    fstat(fd, &fileStat);
    long len = fileStat.st_size;     //传文件大小
    dataLen = sizeof(long);
    if (send_n(connectfd, &dataLen, sizeof(int)) == -1) {
    	close(fd);
    	return -1;
    }
    if (send_n(connectfd, &len, dataLen) == -1) {
    	close(fd);
    	return -1;
    }
    long offset = existedFileSize;
     //传文件
    if (len < (long)100*1024*1024) {  //小于100M普通循环发送
        lseek(fd, offset, SEEK_SET);  //移动文件的读写位置到offset之后
        while (dataLen = read(fd, buf, sizeof(buf))) {
        	if (send_n(connectfd, &dataLen, sizeof(int)) == -1) {
                 close(fd);
                 return -1;
        	}
        	if (send_n(connectfd, buf, dataLen) == -1) {
        		close(fd);
        		return -1;
        	} 
        }
        if (send_n(connectfd, &dataLen, sizeof(int)) == -1) {
        	close(fd);
        	return -1;
        }
    }
    else {  //大于100M用mmap发送
    	char *p = (char*)mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
        dataLen = 1024;
        while(offset < len){
            if (len - offset < dataLen) dataLen = len - offset;
            if (send_n(connectfd, &dataLen, sizeof(int)) == -1) {
                munmap(p, len);
                close(fd);
                return -1;
            }
            if (send_n(connectfd, p + offset, dataLen) == -1) {
                munmap(p, len);
                close(fd);
                return -1;
            }
            offset += dataLen;     
        }
        munmap(p, len);
    }
    close(fd);
    return 0;
}


int recvFile(int connectfd, char* absPath, long existedFileSize) {
     char buf[1024] = {0};
     strcpy(buf, absPath);
     buf[strlen(buf)] = '/';
     int dataLen;
     //接收文件名
     if (recv_n(connectfd, &dataLen, sizeof(int)) == -1) {
     	return -1;
     }
     if (recv_n(connectfd, buf + strlen(buf), dataLen) == -1) {
     	return -1;
     }
     
    int fd = open(buf, O_RDWR|O_CREAT|O_APPEND, 0666);
     //接收文件大小
    long len;
    if (recv_n(connectfd, &dataLen, sizeof(int)) == -1) {
    	close(fd);
    	return -1;
    }
    if (recv_n(connectfd, &len, dataLen) == -1) {
    	close(fd);
    	return -1;
    }
   
     //接收文件
    if (len < (long)100*1024*1024) {  //小于100M普通循环接收
        while (dataLen) {
        	if (recv_n(connectfd, &dataLen, sizeof(int)) == -1) {
                 close(fd);
                 return -1;
        	}
        	if (recv_n(connectfd, buf, dataLen) == -1) {
        		close(fd);
        		return -1;
        	} 
        	write(fd, buf, dataLen);
        }
    }    
    else {  //大于100M用mmap接收
    	ftruncate(fd, len);
    	char *p = (char*)mmap(NULL, len, PROT_WRITE, MAP_SHARED, fd, 0);
        long offset = existedFileSize;
        while(offset < len){
            if (recv_n(connectfd, &dataLen, sizeof(int)) == -1) {
                munmap(p, len);
                ftruncate(fd, offset);
                close(fd);
                return -1;
            }
            if (recv_n(connectfd, p + offset, dataLen) == -1) {
                munmap(p, len);
                ftruncate(fd, offset);
                close(fd);
                return -1;
            }
            offset += dataLen;     
        }
        munmap(p, len);
    }
    close(fd);
    return 0;
}
