//@author: adan shaw
//@E-mail: adan_shaw@qq.com
//@brief: SHTTPD支持CGI的实现
#include "shttpd.h"
/******************************************************
函数名: GenerateDirFile(struct worker_ctl *wctl)
参数:
功能:生成目录下的文件列表
*******************************************************/
int GenerateDirFile (struct worker_ctl *wctl)
{
	struct conn_request *req = &wctl->conn.con_req;
	struct conn_response *res = &wctl->conn.con_res;
	char *command = strstr (req->uri, CGISTR) + strlen (CGISTR);
	char *arg[ARGNUM];
	int num = 0;
	char *rpath = wctl->conn.con_req.rpath;
	struct stat *fs = &wctl->conn.con_res.fsate;
	//打开目录
	DIR *dir = opendir (rpath);
	if (dir == NULL)
	{
		//错误
		res->status = 500;
		goto EXITgenerateIndex;
	}
	//建立临时文件保存目录列表
	FILE *fs_tmp;
	char tmpbuff[2048];
	int filesize = 0;
	char *uri = wctl->conn.con_req.uri;
	//以wb+形式创建一个临时二进制文件
	fs_tmp = tmpfile ();
	//标题部分
	sprintf (tmpbuff, "%s%s%s", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n<HTML><HEAD><TITLE>", uri, "</TITLE></HEAD>\n");
	fprintf (fs_tmp, "%s", tmpbuff);
	filesize += strlen (tmpbuff);
	//标识部分
	sprintf (tmpbuff, "%s %s %s", "<BODY><H1>Index of:", uri, " </H1> <HR><P><I>Date: </I> <I>Size: </I></P><HR>");
	fprintf (fs_tmp, "%s", tmpbuff);
	filesize += strlen (tmpbuff);
	//读取目录中的文件列表
	struct dirent *de;

	char path[PATHLENGTH];
	char tmpath[PATHLENGTH];
	char linkname[PATHLENGTH];
	//struct stat fs;               //stat 结构定义于:/usr/include/sys/stat.h 文件中
	strcpy (path, rpath);
	if (rpath[strlen (rpath)] != '/')
	{
		rpath[strlen (rpath)] = '/';
	}
	while ((de = readdir (dir)) != NULL)	//读取一个文件
	{
		memset (tmpath, 0, sizeof (tmpath));
		memset (linkname, 0, sizeof (linkname));
		if (strcmp (de->d_name, "."))	//不是当前目录
		{
			if (strcmp (de->d_name, ".."))	//不是父目录
			{
				strcpy (linkname, de->d_name);	//将目录名称作为链接名称
			}
			else											//是父目录
			{
				strcpy (linkname, "Parent Directory");	//将父目录作为链接名称
			}

			sprintf (tmpath, "%s%s", path, de->d_name);	//构建当前文件的全路径
			//stat (tmpath, &fs);       //获得文件信息
			stat (tmpath, fs);
			if (S_ISDIR (fs->st_mode))	//是一个目录
			{
				//打印目录的连接为目录名称
				sprintf (tmpbuff, "<A HREF=\"%s/\">%s/</A><BR>\n", de->d_name, tmpath);
			}
			else											//正常文件
			{
				char size_str[32];
				off_t size_int;
				size_int = fs->st_size;	//文件大小
				if (size_int < 1024)		//不到1K
					sprintf (size_str, "%d bytes", (int) size_int);
				else if (size_int < 1024 * 1024)	//不到1M
					sprintf (size_str, "%1.2f Kbytes", (float) size_int / 1024);
				else										//其他
					sprintf (size_str, "%1.2f Mbytes", (float) size_int / (1024 * 1024));
				//输出文件大小
				sprintf (tmpbuff, "<A HREF=\"%s\">%s</A> (%s)<BR>\n", de->d_name, linkname, size_int);
			}
			//将形成的字符串写入临时文件
			fprintf (fs_tmp, "%s", tmpbuff);
			filesize += strlen (tmpbuff);
		}
	}
	//生成临时的文件信息,主要是文件大小
	fs->st_ctime = time (NULL);
	fs->st_mtime = time (NULL);
	fs->st_size = filesize;
	fseek (fs_tmp, (long) 0, SEEK_SET);	//移动文件指针到头部
EXITgenerateIndex:
	return 0;
}

extern struct conf_opts conf_para;
/******************************************************
函数名:cgiHandler(struct worker_ctl *wctl)
参数:
功能:
*******************************************************/
int cgiHandler (struct worker_ctl *wctl)
{
	struct conn_request *req = &wctl->conn.con_req;
	struct conn_response *res = &wctl->conn.con_res;
	//strstr(str1,str2);str1:被查找目标 str2:要查找对象　 
	char *command = strstr (req->uri, CGISTR) + strlen (CGISTR);	//获得匹配字符串/cgi-bin/
	char *arg[ARGNUM];
	int num = 0;
	char *rpath = wctl->conn.con_req.rpath;
	struct stat *fs = &wctl->conn.con_res.fsate;
	int retval = -1;
	char *pos = command;					//查找CGI的命令
	for (; *pos != '?' && *pos != '\0'; pos++) ;	//找到命令尾
	{
		*pos = '\0';
	}
	sprintf (rpath, "%s%s", conf_para.CGIRoot, command);	//构建全路径
	//查找CGI的参数
	pos++;
	for (; *pos != '\0' && num < ARGNUM;)
	{															//CGI的参数为紧跟CGI命令后的？的字符串,多个变量之间用+连接起来,所以可以根据加号的个数确定参数的个数
		arg[num] = pos;							//参数头
		for (; *pos != '+' && *pos != '\0'; pos++) ;
		if (*pos == '+')
		{
			*pos = '\0';							//参数尾
			pos++;
			num++;
		}
	}
	arg[num] = NULL;
	//命令的属性
	if (stat (rpath, fs) < 0)
	{
		//错误
		res->status = 403;
		retval = -1;
		goto EXITcgiHandler;
	}
	else if ((fs->st_mode & S_IFDIR) == S_IFDIR)
	{
		//是一个目录,列出目录下的文件
		GenerateDirFile (wctl);
		retval = 0;
		goto EXITcgiHandler;
	}
	else if ((fs->st_mode & S_IXUSR) != S_IXUSR)
	{
		//所指文件不能执行
		res->status = 403;
		retval = -1;
		goto EXITcgiHandler;
	}
	//创建进程间通信的管道
	int pipe_in[2];
	int pipe_out[2];

	if (pipe (pipe_in) < 0)				//创建管道
	{
		res->status = 500;
		retval = -1;
		goto EXITcgiHandler;
	}
	if (pipe (pipe_out) < 0)
	{
		res->status = 500;
		retval = -1;
		goto EXITcgiHandler;
	}
	//进程分叉
	int pid = 0;
	pid = fork ();
	if (pid < 0)									//错误
	{
		res->status = 500;
		retval = -1;
		goto EXITcgiHandler;
	}
	else if (pid > 0)							//父进程
	{
		close (pipe_out[WRITEOUT]);	//关闭写端
		close (pipe_in[READIN]);		//关闭读端
		//主进程从CGI的标准输出读取数据,并将数据发送到网络资源请求的客户端
		int size = 0;								//这里初始化为0,怎么进入while循环？改为下面的情况
		int end = 0;
		//读取CGI进程数据
		size = read (pipe_out[READIN], res->res.ptr, sizeof (wctl->conn.dres));
		while (size > 0 && !end)
		{
			if (size > 0)
			{													//将数据发送给客户端
				write (wctl->conn.cs, res->res.ptr, strlen (res->res.ptr));
			}
			else
			{
				end = 1;
			}
			size = read (pipe_out[READIN], res->res.ptr, sizeof (wctl->conn.dres));
		}
		wait (&end);								//等待其子进程全部结束
		close (pipe_out[READIN]);		//关闭管道
		close (pipe_in[WRITEOUT]);
		retval = 0;
	}
	else													//子进程
	{
		char cmdarg[2048];
		char onearg[2048];
		char *pos = NULL;
		int i = 0;
		//形成执行命令
		memset (onearg, 0, 2048);
		for (i = 0; i < num; i++)
			sprintf (cmdarg, "%s %s", onearg, arg[i]);
		//将写入的管道绑定到标注输出
		close (pipe_out[READIN]);		//关闭无用的读管道
		dup2 (pipe_out[WRITEOUT], 1);	//将写管道绑定到标准输出 
		close (pipe_out[WRITEOUT]);	//关闭写管道

		close (pipe_in[WRITEOUT]);	// 关闭无用的写管道 
		dup2 (pipe_in[READIN], 0);	// 将读管道绑定到标准输入
		close (pipe_in[READIN]);		// 关闭写管道
		//execlp()会从PATH 环境变量所指的目录中查找符合参数file的文件名,
		//找到后便执行该文件,然后将第二个以后的参数当做该文件的argv[0]、
		//argv[1]……,最后一个参数必须用空指针(NULL)作结束
		execlp (rpath, arg);				//执行命令,命令的输出需要为标准输出
	}
EXITcgiHandler:
	return retval;
}
