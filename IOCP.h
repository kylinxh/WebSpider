/**********************************************************
原创作者：http://blog.csdn.net/piggyxp/article/details/6922277
修改时间：2013年1月11日22:45:10
**********************************************************/

#pragma once

#include <winsock2.h>
#include <MSWSock.h>
#include <list>
#pragma comment(lib,"ws2_32.lib")

#include "lypool.h"
#include "BufList.h"

#include "WebCreeper.h"
//默认端口
#define DEFAULT_PORT          12345    
//默认IP地址
#define DEFAULT_IP            "127.0.0.1"


//缓冲区长度 (1024*8)
#define MAX_LOG_BUFFER_LEN    8192
#define MAX_BUFFER_LEN        8192  
#include <list>
#include <stack>
using namespace std;
// 在完成端口上投递的I/O操作的类型
typedef enum _OPERATION_TYPE  
{  
	OP_ACCEPT,                     // 标志投递的Accept操作
	OP_SEND,                       // 标志投递的是发送操作
	OP_RECV,                       // 标志投递的是接收操作
	OP_DISCONNECT,
	OP_CONNECT,
	OP_NULL                        // 用于初始化，无意义
}OPERATION_TYPE;

//每次套接字操作(如：AcceptEx, WSARecv, WSASend等)对应的数据结构：OVERLAPPED结构(标识本次操作)，关联的套接字，缓冲区，操作类型
struct _PER_SOCKET_CONTEXT;
typedef struct _PER_IO_CONTEXT
{
	OVERLAPPED     m_Overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)              
	_PER_SOCKET_CONTEXT*  m_pOwnerCntx;                               // 这个网络操作所使用的Socket
	WSABUF		   m_wsaBuf;
	LSITBUF	   m_listBuf;
	OPERATION_TYPE m_OpType;                                   // 标识网络操作的类型(对应上面的枚举)

	DWORD			m_nTotalBytes;	//数据总的字节数
	DWORD			m_nSendBytes;	//已经发送的字节数，如未发送数据则设置为0

	//构造函数
	_PER_IO_CONTEXT():m_pOwnerCntx(NULL)
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));  
		m_OpType     = OP_NULL;
		m_nTotalBytes	= 0;
		m_nSendBytes	= 0;
	}
	//析构函数
	~_PER_IO_CONTEXT()
	{
		m_listBuf.release();
	}
	//重置缓冲区内容
	void ResetBuffer()
	{
		m_listBuf.reset();
		m_nTotalBytes = 0;
		m_nSendBytes = 0;
		m_OpType = OP_NULL;
	}
	//重用，Reset
	void Reset()
	{
		m_pOwnerCntx= NULL;
		m_listBuf.reset();
		m_nTotalBytes = 0;
		m_nSendBytes = 0;
		m_OpType = OP_NULL;
	}
} PER_IO_CONTEXT, *PPER_IO_CONTEXT;


//每个SOCKET对应的数据结构(调用GetQueuedCompletionStatus传入)：-
//SOCKET，该SOCKET对应的客户端地址，作用在该SOCKET操作集合(对应结构PER_IO_CONTEXT)；
typedef struct _PER_SOCKET_CONTEXT
{  
	SOCKET      m_Socket;                                  //连接客户端的socket
	string		m_strUrl;								   //访问的网址
	SOCKADDR_IN m_ClientAddr;                              //客户端地址
	list<PER_IO_CONTEXT*> m_IoList;             //套接字操作，本例是WSARecv和WSASend共用一个PER_IO_CONTEXT

	//构造函数
	_PER_SOCKET_CONTEXT()
	{
		m_Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		memset(&m_ClientAddr, 0, sizeof(m_ClientAddr)); 
	}

	//析构函数
	~_PER_SOCKET_CONTEXT()
	{
		if( m_Socket!=INVALID_SOCKET )
		{
			closesocket( m_Socket );
			m_Socket = INVALID_SOCKET;
		}
		// 释放掉所有的IO上下文数据
		m_IoList.clear();
	}

	void AddIoContext(_PER_IO_CONTEXT* p)
	{
		m_IoList.push_back(p);
		p->m_pOwnerCntx = this;
	}

	// 从数组中移除一个指定的IoContext
	void RemoveContext( _PER_IO_CONTEXT* pContext )
	{
		list<PER_IO_CONTEXT*>::iterator it= m_IoList.begin();
		for(; it != m_IoList.end();it++ )
		{
			if( *it == pContext )
			{
				m_IoList.erase(it);			
				break;
			}
		}
	}

} PER_SOCKET_CONTEXT, *PPER_SOCKET_CONTEXT;

// 工作者线程的线程参数
class CIOCP;
typedef struct _tagThreadParams_WORKER
{
	CIOCP* pIOCPModel;                                   //类指针，用于调用类中的函数
	int         nThreadNo;                                    //线程编号

} THREADPARAMS_WORKER,*PTHREADPARAM_WORKER; 

typedef PER_SOCKET_CONTEXT CIOCPContext;
typedef PER_IO_CONTEXT CIOCPBuffer;
// CIOCP类
class CIOCP
{
public:
	CIOCP(void);
	~CIOCP(void);

public:
	// 加载Socket库
	bool LoadSocketLib();
	// 卸载Socket库，彻底完事
	void UnloadSocketLib() { WSACleanup(); }

	// 启动服务器
	bool Start(string strIp = "127.0.0.1", int iPort=12306);
	//	停止服务器
	void Stop();

	// 获得本机的IP地址
	string GetLocalIP();
	// 设置监听端口
	void SetPort( const int& nPort ) { m_nPort = nPort; }

	// 设置主界面的指针，用于调用显示信息到界面中
	void SetMainDlg( LPVOID p ) { m_pMain=p; }

	void VisitUrl(string strUrl);
	
	void OnVisitEnd(string strUrl, LSITBUF& listBuf);

public:
	bool PostWrite( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext); 

	bool PostRecv( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	bool PostConnect(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	bool PostDisconnect( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	bool _DoWrite(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	bool _DoConnect(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);

	bool _DoRecv( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext );

	bool _DoDisconnect(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext);
protected:

	LPVOID LoadSockExFunctions(GUID& FUN_GUID);
	// 初始化IOCP
	bool _InitializeIOCP();
	// 初始化Socket
	bool _InitializeListenSocket();
	// 最后释放资源
	void _DeInitialize();

	void DiscardSocketContext( PER_SOCKET_CONTEXT *pSocketContext , BOOL bReuse=TRUE);

	// 将句柄绑定到完成端口中
	bool _AssociateWithIOCP( PER_SOCKET_CONTEXT *pContext);

	// 处理完成端口上的错误
	bool HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr );

	//线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	//获得本机的处理器数量
	int _GetNoOfProcessors();

	//判断客户端Socket是否已经断开
	bool _IsSocketAlive(SOCKET s);

	//在主界面中显示信息
	void _LogInfo(LPCTSTR lpszFormat, ...) const;

	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// 一个连接关闭
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// 在一个连接上发生了错误
	virtual void OnDataTransmitError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	// 一个连接上的读操作完成
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// 一个连接上的写操作完成
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

private:
	CWebCreeper					 m_creeper;
	lypool<PER_SOCKET_CONTEXT>	 m_socketContextPool;
	lypool<PER_IO_CONTEXT>		 m_IOPool;

	BOOL						 m_bStopping;					//如是为真，说明主线程在等待，等待线程完全退出不要去操作界面
	HANDLE                       m_hShutdownEvent;              // 用来通知线程系统退出的事件，为了能够更好的退出线程

	HANDLE                       m_hIOCompletionPort;           // 完成端口的句柄

	HANDLE*                      m_phWorkerThreads;             // 工作者线程的句柄指针

	int		                     m_nThreads;                    // 生成的线程数量

	string                       m_strIP;                       // 服务器端的IP地址
	int                          m_nPort;                       // 服务器端的监听端口

	LPVOID                       m_pMain;                       // 主界面的界面指针，用于在主界面中显示消息

	CRITICAL_SECTION             m_csContextList;               // 用于Worker线程同步的互斥量

//	list<PER_SOCKET_CONTEXT*>   m_ClientContextList;          // 客户端Socket的Context信息        

	PER_SOCKET_CONTEXT*          m_pListenContext;              // 用于监听的Socket的Context信息

	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;
	LPFN_DISCONNECTEX			 m_lpfnDisconnectEx;
	LPFN_CONNECTEX				 m_lpfnConnectEx;
};

