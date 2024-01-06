#include "shttpd.h"

//初始化时服务器的默认配置
/*
struct conf_opts conf_para = {
	"/usr/local/var/www/cgi-bin/",	//CGI根目录
	"index.html",										//默认文件名称
	"/usr/local/var/www/",					//根文件目录
	"/etc/SHTTPD.conf",							//配置文件路径和名称
	8080,														//监听端口
	4,															//最大客户端数量
	3,															//超时时间
	2																//初始化线程数量
};
*/
extern struct conf_opts conf_para;

struct vec _shttpd_methods[] = {
	{"GET", 3, METHOD_GET},
	{"POST", 4, METHOD_POST},
	{"PUT", 3, METHOD_PUT},
	{"DELETE", 6, METHOD_DELETE},
	{"HEAD", 4, METHOD_HEAD},
	{NULL, 0}
};

/******************************************************
函数名:sig_int(int num)
参数:
功能:SIGINT信号截取函数
*******************************************************/
static void sig_int (int num)
{
	Worker_ScheduleStop ();
	return;
}

/******************************************************
函数名:
参数:
功能:SIGPIPE信号截取函数
*******************************************************/
static void sig_pipe (int num)
{
	return;
}

/******************************************************
函数名:do_listen()
参数:
功能:套接字初始化
*******************************************************/
int do_listen ()
{
	struct sockaddr_in server;
	int ss = -1;
	int err = -1;
	int reuse = 1;
	int ret = -1;
	// 初始化服务器地址
	memset (&server, 0, sizeof (server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port = htons (conf_para.ListenPort);
	//信号截取函数
	signal (SIGINT, sig_int);
	signal (SIGPIPE, sig_pipe);
	//生成套接字文件描述符 
	ss = socket (AF_INET, SOCK_STREAM, 0);
	if (ss == -1)
	{
		printf ("socket() error\n");
		ret = -1;
		goto EXITshttpd_listen;
	}
	//设置套接字地址和端口复用
	err = setsockopt (ss, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));
	if (err == -1)
	{
		printf ("setsockopt SO_REUSEADDR failed\n");
	}
	//绑定IP和套接字描述符
	err = bind (ss, (struct sockaddr *) &server, sizeof (server));
	if (err == -1)
	{
		printf ("bind() error\n");
		ret = -2;
		goto EXITshttpd_listen;
	}
	//设置服务器侦听队列长度
	err = listen (ss, conf_para.MaxClient * 2);
	if (err)
	{
		printf ("listen() error\n");
		ret = -3;
		goto EXITshttpd_listen;
	}

	ret = ss;
EXITshttpd_listen:
	return ret;
}

int l_main ()
{
	int ss = -1;
	ss = do_listen ();
	return 0;
}

/******************************************************
函数名:main(int argc, char *argv[])
参数:
功能:主函数
*******************************************************/
int main (int argc, char *argv[])
{
	int s;
	signal (SIGINT, sig_int);			//挂接信号
	Para_Init (argc, argv);				//参数初始化
	s = do_listen ();							//套接字初始化
	Worker_ScheduleRun (s);				//任务调度
	return 0;
}
