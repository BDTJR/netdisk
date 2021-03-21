#include "function.h"

char* saltGenerator() {
	char buf[STR_LEN]={0};
    int i, flag;
    
    srand(time(NULL)); 
    for (i = 0; i < STR_LEN; i++) {
		flag = rand() % 3;
		switch (flag)
		{
		case 0:
			buf[i] = rand() % 26 + 'a';
			break;
		case 1:
			buf[i] = rand() % 26 + 'A';
			break;
		case 2:
			buf[i] = rand() % 10 + '0';
			break;
		}
	}
	return buf;
}