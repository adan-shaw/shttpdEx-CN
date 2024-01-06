//@author: adan shaw
//@E-mail:adan_shaw@qq.com
//@brief: SHTTPD服务器的实现:主要的数据结构

#ifndef _SHTTP_H_
#define _SHTTP_H_
#include <stdio.h>
#include <getopt.h>							//getopt_long()函数所在库函数
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>					// for sockaddr_in
#include <netdb.h>							// for hostent
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>							// we want to catch some of these after all
#include <unistd.h>							// protos for read, write, close, etc
#include <dirent.h>							// for MAXNAMLEN
#include <limits.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <stddef.h>

// This structure tells how HTTP headers must be parsed.
// Used by parse_headers() function.
#define OFFSET(x) offsetof(struct headers, x)
#define JUMPOVER_CHAR(p,over) do{for(;*p == over;p++);}while(0);
#define JUMPTO_CHAR(p,to) do{for(;*p != to;p++);}while(0);
#define HEXTOI(x) (isdigit(x) ? (x) - '0' : (x) - 'W')

#define printEx printf
#define big_int_t long

#define CGISTR "/cgi-bin/"			//CGI目录的字符串
#define ARGNUM (16)							//CGI程序变量的最大个数
#define READIN (0)							//读出管道
#define WRITEOUT 1							//写入管道
#define URI_MAX (16384)					// Default max request size
#define STATUS_RUNNING (1)
#define STATSU_STOP (0)
#define PATHLENGTH (2048)
#define LINELENGTH (256)
#define K (1024)

//线程的状态值
enum
{
	WORKER_INITED,								//初始化
	WORKER_RUNNING,								//正在执行
	WORKER_DETACHING,							//正在卸载
	WORKER_DETACHED,							//已经卸载
	WORKER_IDEL										//空闲
};

struct conf_opts
{
	char CGIRoot[128];						//CGI根目录
	char DefaultFile[128];				//默认文件名称
	char DocumentRoot[128];				//根文件目录
	char ConfigFile[128];					//配置文件路径和名称
	int ListenPort;								//监听端口
	int MaxClient;								//最大客户端数量
	int TimeOut;									//超时时间
	int InitClient;								//初始化线程数量
};

// HTTP协议的方法 
typedef enum SHTTPD_METHOD_TYPE
{
	METHOD_GET,										//GET方法
	METHOD_POST,									//POST方法
	METHOD_PUT,										//PUT方法
	METHOD_DELETE,								//DELETE方法
	METHOD_HEAD,									//HEAD方法
	METHOD_CGI,										//CGI方法
	METHOD_NOTSUPPORT
} SHTTPD_METHOD_TYPE;

enum
{ HDR_DATE, HDR_INT, HDR_STRING };	//HTTP头部类型

typedef struct shttpd_method
{
	SHTTPD_METHOD_TYPE type;
	int name_index;
} shttpd_method;

typedef struct vec
{
	char *ptr;										//字符串
	int len;											//字符串长度
	SHTTPD_METHOD_TYPE type;			//字符串表示类型
} vec;

struct http_header
{
	int len;											// Header name length
	int type;											// Header type
	size_t offset;								// Value placeholder
	char *name;										// Header name
};

union variant
{
	char *v_str;
	int v_int;
	big_int_t v_big_int;
	time_t v_time;
	void (*v_func) (void);
	void *v_void;
	struct vec v_vec;
};

//头部结构
struct headers
{
	union variant cl;							//内容长度
	union variant ct;							//内容类型
	union variant connection;			//连接状态
	union variant ims;						//最后修改时间
	union variant user;						//用户名称
	union variant auth;						//权限
	union variant useragent;			//用户代理
	union variant referer;				//参考
	union variant cookie;					//Cookie
	union variant location;				//位置
	union variant range;					//范围
	union variant status;					//状态值
	union variant transenc;				//编码类型
};

struct cgi
{
	int iscgi;
	struct vec bin;
	struct vec para;
};

struct worker_ctl;							//要先声明

struct worker_opts
{
	pthread_t th;									//线程的ID号
	int flags;										//线程状态
	pthread_mutex_t mutex;				//线程任务互斥
	struct worker_ctl *work;			//本线程的总控结构
};

struct worker_conn;							//要先声明

//请求结构
struct conn_request
{
	struct vec req;								//请求向量
	char *head;										//请求头部'\0'结尾
	char *uri;										//请求URI'\0'结尾
	char rpath[URI_MAX];					//请求文件的真实地址\0'结尾
	int method;										//请求类型
	//HTTP的版本信息 
	unsigned long major;					//主版本
	unsigned long minor;					//副版本
	struct headers ch;						//头部结构
	struct worker_conn *conn;			//连接结构指针
	int err;
};

//响应结构 
struct conn_response
{
	struct vec res;								//响应向量
	time_t birth_time;						//建立时间
	time_t expire_time;						//超时时间
	int status;										//响应状态值
	int cl;												//响应内容长度
	int fd;												//请求文件描述符
	struct stat fsate;						//请求文件状态
	struct worker_conn *conn;			//连接结构指针
};

struct worker_conn
{
	char dreq[16 * K];						//请求缓冲区
	char dres[16 * K];						//响应缓冲区
	int cs;												//客户端套接字文件描述符
	int to;												//客户端无响应时间超时退出时间
	struct conn_response con_res;
	struct conn_request con_req;
	struct worker_ctl *work;			//本线程的总控结构
};

struct worker_ctl
{
	struct worker_opts opts;			//用于表示线程的状态
	struct worker_conn conn;			//用于表示客户端请求的状态和值
};

//文件内容的类型格式
struct mine_type
{
	char *extension;							//扩展名
	int type;											//类型
	int ext_len;									//扩展名长度
	char *mime_type;							//内容类型
};

//API list
void Para_Init (int argc, char *argv[]);
int Request_Parse (struct worker_ctl *wctl);
int Request_Handle (struct worker_ctl *wctl);

int Worker_ScheduleRun ();
int Worker_ScheduleStop ();
void Method_Do (struct worker_ctl *wctl);
void uri_parse (char *src, int len);
struct mine_type *Mine_Type (char *uri, int len, struct worker_ctl *wctl);

#endif
