//@author: adan shaw
//@E-mail: adan_shaw@qq.com
//@brief: SHTTPD多客户端支持的实现

#include "shttpd.h"
static int workersnum = 0;			//工作线程的数量
extern struct conf_opts conf_para;
pthread_mutex_t thread_init = PTHREAD_MUTEX_INITIALIZER;	//这里就已经初始化了互斥锁
int WORKER_ISSTATUS (int status);
static struct worker_ctl *wctls = NULL;	//线程选项
void Worker_Init ();
int Worker_Add (int i);
void Worker_Delete (int i);
void Worker_Destory ();
/******************************************************
函数名:do_work(struct worker_ctl *wctl)
参数:控制结构
功能:执行任务
*******************************************************/
static void do_work (struct worker_ctl *wctl)
{
	printEx ("LCW==>do_work\n");
	struct timeval tv;						//超时时间
	fd_set rfds;									//读文件集
	int fd = wctl->conn.cs;				//客户端的套接字描述符
	struct vec *req = &wctl->conn.con_req.req;	//请求缓冲区向量

	int retval = 1;								//返回值

	for (; retval > 0;)
	{
		FD_ZERO (&rfds);						//清读文件集
		FD_SET (fd, &rfds);					//将客户端连接描述符放入读文件集
		//设置超时
		tv.tv_sec = 300;						//conf_para.TimeOut;
		tv.tv_usec = 0;
		//超时读数据
		retval = select (fd + 1, &rfds, NULL, NULL, &tv);
		switch (retval)
		{
		case -1:										//错误
			close (fd);
			break;
		case 0:										//超时
			close (fd);
			break;
		default:
			printf ("select retval:%d\n", retval);
			if (FD_ISSET (fd, &rfds))	//检测文件
			{
				memset (wctl->conn.dreq, 0, sizeof (wctl->conn.dreq));
				//读取客户端数据
				req->len = read (wctl->conn.cs, wctl->conn.dreq, sizeof (wctl->conn.dreq));
				req->ptr = wctl->conn.dreq;
				printEx ("Read %d bytes,'%s'\n", req->len, req->ptr);
				if (req->len > 0)
				{
					//分析客户端的数据
					wctl->conn.con_req.err = Request_Parse (wctl);	//待实现
					//处理并响应客户端请求
					Request_Handle (wctl);	//待实现
				}
				else
				{
					close (fd);
					retval = -1;
				}
			}
		}
	}
	printEx ("LCW<==do_work\n");
}

/******************************************************
函数名:worker(void *arg)
参数:worker_ctl *wctls
功能:线程处理函数
*******************************************************/
static void *worker (void *arg)
{
	printEx ("LCW==>worker\n");
	struct worker_ctl *ctl = (struct worker_ctl *) arg;	//为何不直接传这个类型过来？
	struct worker_opts *self_opts = &ctl->opts;	//定义一个选项结构
	pthread_mutex_unlock (&thread_init);	//解锁互斥
	self_opts->flags = WORKER_IDEL;	//初始化线程为空闲,等待任务
	//如果主控线程没有让此线程退出,则循环处理任务
	for (; self_opts->flags != WORKER_DETACHING;)	//while(self_opts->flags != WORKER_DETACHING)
	{
		//printEx("work:%d,status:%d\n",(int)self_opts->th,self_opts->flags );
		//查看是否有任务分配
		int err = pthread_mutex_trylock (&self_opts->mutex);	//互斥预锁定
		//pthread_mutex_trylock()是pthread_mutex_lock() 的非阻塞版本
		if (err)
		{
			//printEx("NOT LOCK\n");
			sleep (1);
			continue;
		}
		else
		{
			//有任务,do it
			printEx ("Do task\n");
			self_opts->flags = WORKER_RUNNING;	//执行标志
			do_work (ctl);
			close (ctl->conn.cs);			//关闭套接字
			ctl->conn.cs = -1;
			if (self_opts->flags == WORKER_DETACHING)
				break;
			else
				self_opts->flags = WORKER_IDEL;
		}
	}
	//主控发送退出命令
	//设置状态为已卸载
	self_opts->flags = WORKER_DETACHED;
	workersnum--;									//工作线程-1

	printEx ("LCW<==worker\n");
	return NULL;
}

/******************************************************
函数名:WORKER_ISSTATUS(int status)
参数:欲查询的线程状态
功能:查询线程状态
*******************************************************/
int WORKER_ISSTATUS (int status)
{
	int i = 0;
	for (i = 0; i < conf_para.MaxClient; i++)
	{
		if (wctls[i].opts.flags == status)
			return i;									//返回符合的线程
	}
	return -1;										//没有符合的线程状态
}

/*****************************************************
函数名:Worker_Init()
参数:无
功能:初始化线程
******************************************************/
void Worker_Init ()
{
	printEx ("LCW==>Worker_Init");
	int i = 0;
	//初始化总控参数
	wctls = (struct worker_ctl *) malloc (sizeof (struct worker_ctl) * conf_para.MaxClient);	//开辟空间
	memset (wctls, 0, sizeof (*wctls) * conf_para.MaxClient);	//清零
	//初始化一些参数
	for (i = 0; i < conf_para.MaxClient; i++)
	{
		//opt&connn结构和worker_ctl结构形成回指针
		wctls[i].opts.work = &wctls[i];
		wctls[i].conn.work = &wctls[i];
		//opts结构部分的初始化
		wctls[i].opts.flags = WORKER_DETACHED;
		//wctls[i].opts.mutex = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_init (&wctls[i].opts.mutex, NULL);	//初始化互斥锁
		pthread_mutex_lock (&wctls[i].opts.mutex);
		//conn部分的初始化
		//con_req&con_res与conn结构形成回指
		wctls[i].conn.con_req.conn = &wctls[i].conn;
		wctls[i].conn.con_res.conn = &wctls[i].conn;
		wctls[i].conn.cs = -1;			//客户端socket连接为空
		//conn.con_req部分初始化:请求结构
		wctls[i].conn.con_req.req.ptr = wctls[i].conn.dreq;
		wctls[i].conn.con_req.head = wctls[i].conn.dreq;
		wctls[i].conn.con_req.uri = wctls[i].conn.dreq;
		//conn.con_res部分初始化:响应结构
		wctls[i].conn.con_res.fd = -1;
		wctls[i].conn.con_res.res.ptr = wctls[i].conn.dres;

	}
	for (i = 0; i < conf_para.InitClient; i++)
	{
		//增加规定个数工作线程
		Worker_Add (i);
	}
	printEx ("LCW<==Worker_Init\n");
}

/******************************************************
函数名:Worker_Add(int i)
参数:
功能:增加线程
*******************************************************/
int Worker_Add (int i)
{
	printEx ("LCW==>Worker_Add\n");
	pthread_t th;									//线程参数
	int err = -1;									//返回值
	if (wctls[i].opts.flags == WORKER_RUNNING)
	{
		return 1;										//如果线程已经在工作,则返回
	}
	pthread_mutex_lock (&thread_init);	//进入互斥区(之前有初始化过了)
	wctls[i].opts.flags = WORKER_INITED;	//状态为已初始化
	err = pthread_create (&th, NULL, worker, (void *) &wctls[i]);	//建立线程
	//线程处理函数为worker
	pthread_mutex_unlock (&thread_init);	//解锁互斥
	//更新线程选项
	wctls[i].opts.th = th;				//线程ID
	workersnum++;									//线程数量增加1

	printEx ("LCW<==Worker_Add\n");
	return 0;
}

/******************************************************
函数名:Worker_Delete(int i)
参数:线程序号
功能:减少线程
*******************************************************/
void Worker_Delete (int i)
{
	printEx ("LCW==>Worker_Delete\n");
	wctls[i].opts.flags = WORKER_DETACHING;	//线程状态改为正在卸载
	printEx ("LCW<==Worker_Delete\n");
}

/******************************************************
函数名:Worker_Destory()
参数:
功能:销毁线程
*******************************************************/
void Worker_Destory ()
{
	printEx ("LCW==>Worker_Destory\n");
	int i = 0;
	int clean = 0;

	for (i = 0; i < conf_para.MaxClient; i++)
	{
		printEx ("thread %d,status %d\n", i, wctls[i].opts.flags);
		if (wctls[i].opts.flags != WORKER_DETACHED)	//如果状态不是已经卸载
			Worker_Delete (i);
	}

	while (!clean)
	{
		clean = 1;
		for (i = 0; i < conf_para.MaxClient; i++)
		{
			printEx ("thread %d,status %d\n", i, wctls[i].opts.flags);
			if (wctls[i].opts.flags == WORKER_RUNNING || wctls[i].opts.flags == WORKER_DETACHING)
				clean = 0;
		}
		if (!clean)
			sleep (1);
	}
	printEx ("LCW<==Worker_Destory\n");
}

//定义调度状态

static int SCHEDULESTATUS = STATUS_RUNNING;
/******************************************************
函数名:Worker_ScheduleRun(int ss)
参数:文件描述符
功能:当有客户端连接到来的时候,将客户端连接分配给空闲客户端,由客户端处理到来的请求
*******************************************************/
int Worker_ScheduleRun (int ss)
{
	printEx ("LCW==>Worker_ScheduleRun!!!\n");
	struct sockaddr_in client;
	socklen_t len = sizeof (client);
	//初始化线程服务
	Worker_Init ();

	int i = 0;

	for (; SCHEDULESTATUS == STATUS_RUNNING;)
	{
		struct timeval tv;					//超时时间
		fd_set rfds;								//读文件集
		//printf("SCHEDULESTATUS:%d\n",SCHEDULESTATUS);
		int retval = -1;						//返回值
		FD_ZERO (&rfds);						//清读文件集,将客户端连接
		FD_SET (ss, &rfds);					//描述符放入读文件集
		//设置超时
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		//超时读数据
		retval = select (ss + 1, &rfds, NULL, NULL, &tv);
		switch (retval)
		{
		case -1:										//错误
		case 0:										//超时
			continue;
			break;
		default:
			if (FD_ISSET (ss, &rfds))	//检测文件
			{
				int sc = accept (ss, (struct sockaddr *) &client, &len);
				printf ("client comming\n");	//接受请求
				i = WORKER_ISSTATUS (WORKER_IDEL);	//查找空闲业务处理线程
				if (i == -1)
				{
					i = WORKER_ISSTATUS (WORKER_DETACHED);	//没有找到
					if (i != -1)
						Worker_Add (i);			//增加一个业务处理线程
				}
				if (i != -1)						//业务处理线程空闲,分配任务
				{
					wctls[i].conn.cs = sc;	//套接字描述符
					pthread_mutex_unlock (&wctls[i].opts.mutex);	//告诉业务线程有任务
				}
			}
		}
	}

	printEx ("LCW<==Worker_ScheduleRun\n");
	return 0;
}

/******************************************************
函数名:Worker_ScheduleStop()
参数:
功能:停止调度过程
*******************************************************/
int Worker_ScheduleStop ()
{
	printEx ("LCW==>Worker_ScheduleStop\n");
	SCHEDULESTATUS = STATSU_STOP;	//给任务分配线程设置终止条件
	int i = 0;
	Worker_Destory ();						//销毁业务线程
	int allfired = 0;
	for (; !allfired;)						//查询并等待业务线程终止
	{
		allfired = 1;
		for (i = 0; i < conf_para.MaxClient; i++)
		{
			int flags = wctls[i].opts.flags;
			if (flags == WORKER_DETACHING || flags == WORKER_IDEL)	//线程正活动
				allfired = 0;
		}
	}

	pthread_mutex_destroy (&thread_init);	//销毁互斥变量
	for (i = 0; i < conf_para.MaxClient; i++)
		pthread_mutex_destroy (&wctls[i].opts.mutex);	//销毁业务吃力线程的互斥
	free (wctls);									//销毁业务数据

	printEx ("LCW<==Worker_ScheduleStop\n");
	return 0;
}
