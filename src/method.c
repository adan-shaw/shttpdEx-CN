//@author: adan shaw
//@E-mail: adan_shaw@qq.com
//@brief: 服务器SHTTPD请求方法解析
#include "shttpd.h"

/******************************************************
函数名: Method_DoGet(struct worker_ctl *wctl)
参数:
功能:GET方法
*******************************************************/
static int Method_DoGet (struct worker_ctl *wctl)
{
	printEx ("LCW==>Method_DoGet\n");
	struct conn_response *res = &wctl->conn.con_res;
	struct conn_request *req = &wctl->conn.con_req;
	char path[URI_MAX];
	memset (path, 0, URI_MAX);
	size_t n;
	unsigned long r1, r2;
	char *fmt = "%a, %d %b %Y %H:%M:%S GMT";	//时间格式
	//需要确定的参数
	size_t status = 200;					//状态值,已确定
	char *msg = "OK";							//状态信息,已确定
	char date[64] = "";						//时间
	char lm[64] = "";							//请求文件最后修改信息
	char etag[64] = "";						//etag信息
	big_int_t cl;									//内容长度
	char range[64] = "";					//范围
	struct mine_type *mine = NULL;
	time_t t = time (NULL);				//取得当前时间
	//根据fmt指向字符串的命令将localtime(&t)中的时间信息储存在date中
	(void) strftime (date, sizeof (date), fmt, localtime (&t));
	//最后修改时间
	(void) strftime (lm, sizeof (lm), fmt, localtime (&res->fsate.st_mtime));
	//ETAG 将可变个参数(...)按照"%lx.%lx"格式化成字符串,然后将其复制到etag中
	(void) snprintf (etag, sizeof (etag), "%lx.%lx", (unsigned long) res->fsate.st_mtime, (unsigned long) res->fsate.st_size);
	//发送的MIME类型
	mine = Mine_Type (req->uri, strlen (req->uri), wctl);
	//内容长度
	cl = (big_int_t) res->fsate.st_size;
	//范围range
	memset (range, 0, sizeof (range));
	n = -1;
	if (req->ch.range.v_vec.len > 0)	//取出请求范围
	{
		printf ("request range:%d\n", req->ch.range.v_vec.len);
		//从ptr里读进数据,依照第二个参数的格式将数据写入到后面的参数里
		n = sscanf (req->ch.range.v_vec.ptr, "bytes=%lu-%lu", &r1, &r2);
	}
	printf ("n:%d\n", n);
	if (n > 0)
	{
		status = 206;
		lseek (res->fd, r1, SEEK_SET);
		//n==2取前一个
		cl = n == 2 ? r2 - r1 + 1 : cl - r1;
		(void) snprintf (range, sizeof (range), "Content-Range: bytes %lu-%lu/%lu\r\n", r1, r1 + cl - 1, (unsigned long) res->fsate.st_size);
		msg = "Partial Content";
	}
	//构建输出的头部
	memset (res->res.ptr, 0, sizeof (wctl->conn.dres));
	snprintf (res->res.ptr,				//缓冲区
		sizeof (wctl->conn.dres),		//缓冲区长度
		"HTTP/1.1 %d %s\r\n"				//状态和状态信息
		"Date: %s\r\n"							//日期
		"Last-Modified: %s\r\n"			//最后修改时间
		"Etag: \"%s\"\r\n"					//Web资源标记号
		"Content-Type: %.*s\r\n"		//内容类型
		"Content-Length: %lu\r\n"		//内容长度
		//"Connection:close\r\n"
		"Accept-Ranges: bytes\r\n"	//发送范围
		"%s\r\n",										//范围起始
		status, msg, date, lm, etag, strlen (mine->mime_type), mine->mime_type, cl, range);
	res->cl = cl;
	res->status = status;
	printf ("content length:%d, status:%d\n", res->cl, res->status);
	printEx ("LCW<==Method_DoGet\n");
	return 0;
}

/******************************************************
未实现的方法
*******************************************************/
static int Method_DoPost (struct worker_ctl *wctl)
{
	return 0;
}

static int Method_DoHead (struct worker_ctl *wctl)
{
	Method_DoGet (wctl);
	close (wctl->conn.con_res.fd);
	wctl->conn.con_res.fd = -1;
	return 0;
}

static int Method_DoPut (struct worker_ctl *wctl)
{
	return 0;
}

static int Method_DoDelete (struct worker_ctl *wctl)
{
	return 0;
}

static int Method_DoCGI (struct worker_ctl *wctl)
{
	return 0;
}

static int Method_DoList (struct worker_ctl *wctl)
{
	return 0;
}

/******************************************************
函数名:Method_Do(struct worker_ctl *wctl)
参数:业务和线程状态结构
功能:匹配方法
*******************************************************/
void Method_Do (struct worker_ctl *wctl)
{
	printEx ("LCW==>Method_Do\n");
	if (0)												//????什么意思 不执行？
		Method_DoCGI (wctl);
	switch (wctl->conn.con_req.method)	//匹配请求类型
	{
	case METHOD_PUT:							//PUT方法
		Method_DoPut (wctl);
		break;
	case METHOD_DELETE:					//DELETE方法
		Method_DoDelete (wctl);
		break;
	case METHOD_GET:							//GET方法(这里只实现GET方法)
		Method_DoGet (wctl);
		break;
	case METHOD_POST:						//POST方法
		Method_DoPost (wctl);
		break;
	case METHOD_HEAD:						//HEAD方法
		Method_DoHead (wctl);
		break;
	default:
		Method_DoList (wctl);
	}

	printEx ("LCW<==Method_Do\n");
}
