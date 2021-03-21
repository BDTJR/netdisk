#pragma once
#include "function.h"
#include "clients.h"

class ConnectedClients;

class MySQLInterface
{
friend ConnectedClients;
public:
	MySQLInterface();
	virtual ~MySQLInterface();
    bool connectMySQL(char* server, char* username, char* password, char* database);  
    int sqlSelect(string sql,char* buf);
    int sqlFind(string sql);
    int sqlChange(string sql);

    void errorIntoMySQL();
    void closeMySQL(); 
public:  
        int errorNum;                    //错误代号  
        const char* errorInfo;             //错误提示  
private:  
        MYSQL mysqlInstance;              //MySQL对象
        MYSQL_RES *result;             //用于存放结果   
};

MySQLInterface::MySQLInterface() : errorNum(0), errorInfo("ok") {  
        mysql_library_init(0, NULL, NULL);  
        mysql_init(&mysqlInstance);  
        mysql_options(&mysqlInstance, MYSQL_SET_CHARSET_NAME, "gbk");  
}  

MySQLInterface::~MySQLInterface() {  

}  

bool MySQLInterface::connectMySQL(char* server, char* username, char* password, char* database) {  
      if(mysql_real_connect(&mysqlInstance,server,username,password,database,0,0,0) != NULL)  
          return true;  
      else  
          errorIntoMySQL();  
      return false;  
}  

int MySQLInterface::sqlSelect(string sql,char* buf) {
	if (mysql_query(&mysqlInstance, sql.c_str())) {  
         errorIntoMySQL();  
         return -1;  
    }  
    result = mysql_store_result(&mysqlInstance);
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
    	strcpy(buf, row[0]);
    }
    else {
    	mysql_free_result(result);
         return -1;  
    }
    return 0;
}

int MySQLInterface::sqlChange(string sql) {
	if (mysql_query(&mysqlInstance, sql.c_str())) {  
         errorIntoMySQL();  
         return -1;  
    } 
    return 0; 
}

int MySQLInterface::sqlFind(string sql) {
	if (mysql_query(&mysqlInstance, sql.c_str())) {  
         errorIntoMySQL();  
         return -1;  
    } 
    result = mysql_store_result(&mysqlInstance);
    MYSQL_ROW row = mysql_fetch_row(result); 
    if (row) {
    	mysql_free_result(result);
        return 1;  
    }
    else {
    	mysql_free_result(result);
        return 0; 
    }
    mysql_free_result(result);
    return 0; 
}

void MySQLInterface::errorIntoMySQL() {  
        errorNum = mysql_errno(&mysqlInstance);  
        errorInfo = mysql_error(&mysqlInstance);  
}  
 
void MySQLInterface::closeMySQL() {  
        mysql_close(&mysqlInstance);  
}  