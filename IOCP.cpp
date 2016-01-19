#include "StdAfx.h"
#include "IOCP.h"
#include "YHttpRequest.h"
// 每一个处理器上产生多少个线程
#define WORKER_THREADS_PER_PROCESSOR 1
// 同时投递的AcceptEx请求的数量
#define MAX_POST_ACCEPT              10
// 传递给Worker线程的退出信号
#define EXIT_CODE                    NULL


// 释放指针和句柄资源的宏

// 释放指针宏
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// 释放句柄宏
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// 释放Socket宏
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}



CIOCP::CIOCP(void):
							m_nThreads(0),
							m_hShutdownEvent(NULL),
							m_hIOCompletionPort(NULL),
							m_phWorkerThreads(NULL),
							m_strIP(DEFAULT_IP),
							m_nPort(DEFAULT_PORT),
							m_pMain(NULL),
							m_lpfnAcceptEx( NULL ),
							m_lpfnDisconnectEx(NULL),
							m_lpfnConnectEx(NULL),
							m_pListenContext( NULL ),
							m_bStopping(FALSE)
{
	LoadSocketLib();
	m_creeper.m_pIOCP = this;
}


CIOCP::~CIOCP(void)
{
	// 确保资源彻底释放
	this->Stop();
	UnloadSocketLib();
}


/*********************************************************************
*函数功能：线程函数，根据GetQueuedCompletionStatus返回情况进行处理；
*函数参数：lpParam是THREADPARAMS_WORKER类型指针；
*函数说明：GetQueuedCompletionStatus正确返回时表示某操作已经完成，第二个参数lpNumberOfBytes表示本次套接字传输的字节数，
参数lpCompletionKey和lpOverlapped包含重要的信息，请查询MSDN文档；
*********************************************************************/
DWORD WINAPI CIOCP::_WorkerThread(LPVOID lpParam)
{    
	THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
	CIOCP* pIOCPModel = (CIOCP*)pParam->pIOCPModel;
	int nThreadNo = (int)pParam->nThreadNo;

//	pIOCPModel->_LogInfo(L"工作者线程启动，ID: %d.",nThreadNo);

	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;

	//循环处理请求，直到接收到Shutdown信息为止
	while (WAIT_OBJECT_0 != WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0))
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			pIOCPModel->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);

		//接收EXIT_CODE退出标志，则直接退出
		if ( EXIT_CODE==(DWORD)pSocketContext)
		{
			break;
		}

		//返回值为0，表示出错
		if( !bReturn )  
		{  
			DWORD dwErr = GetLastError();
			// 显示一下提示信息
			if( !pIOCPModel->HandleError( pSocketContext,dwErr ) )
			{
				pIOCPModel->OnDataTransmitError(pSocketContext, NULL, dwErr);
				break;
			}

			continue;  
		}  
		else  
		{  	
			// 读取传入的参数
			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  

			// 判断是否有客户端断开了
			if((0 == dwBytesTransfered) && ( OP_RECV==pIoContext->m_OpType || OP_SEND==pIoContext->m_OpType))  
			{ 
//				pIOCPModel->_LogInfo( _T("客户端 %s:%d 断开连接."),(LPCTSTR)_bstr_t(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr)), ntohs(pSocketContext->m_ClientAddr.sin_port) );

				
				pIOCPModel->OnVisitEnd(pSocketContext->m_strUrl, pIoContext->m_listBuf);
				pIOCPModel->DiscardSocketContext( pSocketContext );

 				continue;  
			}  
			else
			{
				switch( pIoContext->m_OpType )  
				{  
					// RECV
				case OP_RECV:
					{
						pIoContext->m_nTotalBytes	= dwBytesTransfered;
						pIOCPModel->_DoRecv( pSocketContext,pIoContext );
					}
					break;

				case OP_SEND:
					{
						pIoContext->m_nSendBytes += dwBytesTransfered;
						pIOCPModel->_DoWrite(pSocketContext,pIoContext);
					}
					break;
				case OP_CONNECT:
					{
						WSABUF* p_wbuf = &pIoContext->m_wsaBuf;
						NODEBUF* pNode = pIoContext->m_listBuf.head();
						p_wbuf->buf = pNode->buf;
						p_wbuf->len = pNode->len;
						pIoContext->m_nTotalBytes = p_wbuf->len;
						pIOCPModel->PostWrite(pSocketContext, pIoContext);
					}
					break;
				case OP_DISCONNECT:
					{
						pSocketContext->m_strUrl = "";
						pIOCPModel->m_socketContextPool.recycle(pSocketContext);
						pIoContext->Reset();
						string strErr;
						strErr = "idle count is:";
						strErr += pIOCPModel->m_socketContextPool.idleCount();
						strErr += "\n";
						::OutputDebugStringA(strErr.c_str());
						pIOCPModel->m_IOPool.recycle(pIoContext);//千万不要忘记了|
					}
					break;
				default:
					break;
				} //switch
			}//if
		}//if

	}//while

	// 释放线程参数
	RELEASE(lpParam);	

	return 0;
}

//函数功能：初始化套接字
bool CIOCP::LoadSocketLib()
{    
	WSADATA wsaData;
	int nResult;
	nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	// 错误(一般都不可能出现)
	if (NO_ERROR != nResult)
	{
		this->_LogInfo(_T("初始化WinSock 2.2失败！\n"));
		return false; 
	}

	return true;
}


//函数功能：启动服务器
bool CIOCP::Start(string strIp, int iPort)
{
	// 初始化线程互斥量
	InitializeCriticalSection(&m_csContextList);

	// 建立系统退出的事件通知
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 初始化IOCP
	if (false == _InitializeIOCP())
	{
		this->_LogInfo(_T("初始化IOCP失败！\n"));
		return false;
	}

	// 初始化Socket
	if( false==_InitializeListenSocket() )
	{
		this->_LogInfo(_T("Listen Socket初始化失败！\n"));
		this->_DeInitialize();
		return false;
	}

	this->_LogInfo(_T("系统准备就绪，等候连接....\n"));
	return true;
}

void CIOCP::_DeInitialize()
{
	// 删除客户端列表的互斥量
	DeleteCriticalSection(&m_csContextList);

	// 关闭系统退出事件句柄
	RELEASE_HANDLE(m_hShutdownEvent);

	// 释放工作者线程句柄指针
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}
	
	RELEASE(m_phWorkerThreads);

	// 关闭IOCP句柄
	RELEASE_HANDLE(m_hIOCompletionPort);


	_LogInfo(L"释放资源完毕.\n");
}

////////////////////////////////////////////////////////////////////
//	开始发送系统退出消息，退出完成端口和线程资源
void CIOCP::Stop()
{
	if( m_hIOCompletionPort!= INVALID_HANDLE_VALUE && m_hIOCompletionPort)
	{
		// 激活关闭消息通知
		m_bStopping = TRUE;
		SetEvent(m_hShutdownEvent);
		
		for (int i = 0; i < m_nThreads; i++)
		{
			// 通知所有的完成端口操作退出
			PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
		}

		// 等待所有的客户端资源退出
		WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);

		m_bStopping= FALSE;
		// 释放其他资源
		this->_DeInitialize();

		this->_LogInfo(L"停止监听\n");
	}	
}

bool CIOCP::_InitializeIOCP()
{
	// 建立第一个完成端口
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

	if ( NULL == m_hIOCompletionPort)
	{
		this->_LogInfo(_T("建立完成端口失败！错误代码: %d!\n"), WSAGetLastError());
		return false;
	}

	m_nThreads = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();
	

	m_phWorkerThreads = new HANDLE[m_nThreads];

	DWORD nThreadID;
	for (int i = 0; i < m_nThreads; i++)
	{
		THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
		pThreadParams->pIOCPModel = this;
		pThreadParams->nThreadNo  = i+1;
		m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 0, &nThreadID);
	}

	_LogInfo(L" 建立 _WorkerThread %d 个.\n", m_nThreads );

	return true;
}


LPVOID CIOCP::LoadSockExFunctions(GUID& FUN_GUID)
{
	SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	DWORD dwBytes = 0;
	LPVOID lpFun=NULL;
	if(SOCKET_ERROR == WSAIoctl(
		s, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&FUN_GUID, 
		sizeof(FUN_GUID), 
		&lpFun, 
		sizeof(lpFun), 
		&dwBytes, 
		NULL, 
		NULL))  
	{  
		this->_LogInfo(L"WSAIoctl 未能获取AcceptEx函数指针。错误代码: %d\n", WSAGetLastError()); 
		this->_DeInitialize();
		return false;  
	}
	closesocket(s);
	return lpFun;
}
/////////////////////////////////////////////////////////////////
// 初始化Socket
bool CIOCP::_InitializeListenSocket()
{
	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidAcceptEx = WSAID_ACCEPTEX;  
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
	GUID GuidDisconnectEx = WSAID_DISCONNECTEX;
	GUID GuidConnectEx = WSAID_CONNECTEX;                   


	m_lpfnAcceptEx = (LPFN_ACCEPTEX)LoadSockExFunctions(GuidAcceptEx);
	m_lpfnGetAcceptExSockAddrs = (LPFN_GETACCEPTEXSOCKADDRS)LoadSockExFunctions(GuidGetAcceptExSockAddrs);
	m_lpfnDisconnectEx = (LPFN_DISCONNECTEX)LoadSockExFunctions(GuidDisconnectEx);
	m_lpfnConnectEx = (LPFN_CONNECTEX)LoadSockExFunctions(GuidConnectEx);
	return true;
}

bool CIOCP::PostConnect(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	sockaddr_in local_addr ;

    ZeroMemory (&local_addr , sizeof (sockaddr_in ));

    local_addr .sin_family = AF_INET ;

	int irt = ::bind (pSocketContext->m_Socket , (sockaddr *)(&local_addr ), sizeof (sockaddr_in ));

	BOOL bRet = this->_AssociateWithIOCP(pSocketContext);
	DWORD dwSendBytes=0;
	pIoContext->m_OpType = OP_CONNECT;
	PVOID lpSendBuffer = NULL;
    DWORD dwSendDataLength = 0;
	bRet = m_lpfnConnectEx(pSocketContext->m_Socket, (SOCKADDR*)&pSocketContext->m_ClientAddr, sizeof(SOCKADDR),
		lpSendBuffer, dwSendDataLength, &dwSendBytes, &(pIoContext->m_Overlapped));
	DWORD errCode = WSAGetLastError();
	return TRUE;
}


bool CIOCP::PostDisconnect(PER_SOCKET_CONTEXT* pSocketContext,PER_IO_CONTEXT* pIoContext)
{
	pIoContext->m_OpType = OP_DISCONNECT;
	BOOL bRet = m_lpfnDisconnectEx(pSocketContext->m_Socket, &(pIoContext->m_Overlapped), TF_REUSE_SOCKET,0);
	bRet = WSAGetLastError();
	return TRUE;
}

bool CIOCP::PostRecv(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;

	NODEBUF* pNode= pIoContext->m_listBuf.getNode();
	WSABUF *p_wbuf = &pIoContext->m_wsaBuf;
	p_wbuf->buf = pNode->buf+ pNode->len;
	p_wbuf->len =	pNode->size() - pNode->len;

	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	pIoContext->m_OpType = OP_RECV;
	pIoContext->m_nSendBytes = 0;
	pIoContext->m_nTotalBytes= 0;
	// 初始化完成后，，投递WSARecv请求
//	CString str;
//	str.Format(L"offset=%d,post recv len=%d\n\n",pNode->len,p_wbuf->len);
//	AfxMessageBox(str);
	int nBytesRecv = WSARecv( pSocketContext->m_Socket, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
	if (SOCKET_ERROR == nBytesRecv)
	{
		
		DWORD errCode =WSAGetLastError();
		if(errCode != WSA_IO_PENDING)
		{
			this->_LogInfo(L"投递WSARecv失败,Error Code=%d\n", WSAGetLastError());
			return false;
		}
	}
	return true;
}

bool CIOCP::PostWrite(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwSendNumBytes = 0;
	WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	pIoContext->m_OpType = OP_SEND;
	//投递WSASend请求 -- 需要修改
	ZeroMemory(p_ol,sizeof(OVERLAPPED));
	
	int nRet = WSASend(pSocketContext->m_Socket, p_wbuf, 1, &dwSendNumBytes, dwFlags,
		&pIoContext->m_Overlapped, NULL);

	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
	if (SOCKET_ERROR == nRet)
	{
		DWORD errCode =WSAGetLastError();
		if(errCode != WSA_IO_PENDING)
		{
			this->_LogInfo(L"投递WSASend失败,Error Code=%d\n", WSAGetLastError());
			return false;
		}
	}
	return true;
}

bool CIOCP::_DoRecv( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext )
{
	OnReadCompleted(pSocketContext, pIoContext);
	NODEBUF* pNode = pIoContext->m_listBuf.pNode;
	pNode->len += pIoContext->m_nTotalBytes;
	PostRecv(pSocketContext, pIoContext);
	//iocp性能太高了，要一
	return true;
}

bool CIOCP::_DoWrite(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	if (pIoContext->m_nSendBytes < pIoContext->m_nTotalBytes)
	{
		//数据未能发送完，继续发送数据
		WSABUF* p_wbuf = &pIoContext->m_wsaBuf;
		p_wbuf->buf += pIoContext->m_nSendBytes;
		p_wbuf->len -= pIoContext->m_nSendBytes;
		PostWrite(pSocketContext,pIoContext);
	}
	else
	{
		NODEBUF* pNode = pIoContext->m_listBuf.pNode;
		pNode = pNode->pNext;
		if(pNode&&pNode->len>0)
		{
			WSABUF* p_wbuf = &pIoContext->m_wsaBuf;
			p_wbuf->buf = pNode->buf;
			p_wbuf->len = pNode->len;
			PostWrite(pSocketContext,pIoContext);
		}
		else
		{
			pIoContext->ResetBuffer();
			PostRecv(pSocketContext,pIoContext);
		}
	}
	return TRUE;
}

bool CIOCP::_DoConnect(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	return TRUE;
}

bool CIOCP::_AssociateWithIOCP( PER_SOCKET_CONTEXT *pContext )
{
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->m_Socket, m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
//		this->_LogInfo((L"执行CreateIoCompletionPort()出现错误.错误代码：%d"),GetLastError());
		return false;
	}

	return true;
}



////////////////////////////////////////////////////////////////
//	移除某个特定的Context
void CIOCP::DiscardSocketContext( PER_SOCKET_CONTEXT *pSocketContext, BOOL bReuse)
{
	list<PER_IO_CONTEXT*>::iterator itr;
	int iSize = (pSocketContext->m_IoList).size();
	if(iSize)
	{
		for(itr= (pSocketContext->m_IoList).begin(); iSize>0 ; iSize--)
		{
			(*itr)->Reset();
			m_IOPool.recycle(*itr);
		}
	}
	pSocketContext->m_IoList.clear();
	if(bReuse)
	{
		PER_IO_CONTEXT* pIO = m_IOPool.getItem();
		pIO->m_pOwnerCntx = pSocketContext;
		PostDisconnect(pSocketContext,pIO);
	}
	else
	{
		RELEASE(pSocketContext)
	}
}

string CIOCP::GetLocalIP()
{
	// 获得本机主机名
	char hostname[MAX_PATH] = {0};
	gethostname(hostname,MAX_PATH);                
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if(lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	LPSTR lpAddr = lpHostEnt->h_addr_list[0];      

	// 将IP地址转化成字符串形式
	struct in_addr inAddr;
	memmove(&inAddr,lpAddr,4);
	m_strIP =  inet_ntoa(inAddr);        

	return m_strIP;
}

///////////////////////////////////////////////////////////////////
// 获得本机中处理器的数量
int CIOCP::_GetNoOfProcessors()
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}

/////////////////////////////////////////////////////////////////////
// 在主界面中显示提示信息
void CIOCP::_LogInfo(LPCTSTR lpszFormat, ...)const
{
	va_list list;
	TCHAR tszInfo[MAX_LOG_BUFFER_LEN]={0};
	va_start(list,lpszFormat);
	wvsprintf(tszInfo, lpszFormat, list);
	va_end(list);

//	TRACE(lpszFormat);
	OutputDebugString(tszInfo);
 //   CProxyServerDlg* pDlg = static_cast<CProxyServerDlg*>(m_pMain);
//	if(pDlg&&!m_bStopping)
//		pDlg->LogInfo(tszInfo);
}

/////////////////////////////////////////////////////////////////////
// 判断客户端Socket是否已经断开，否则在一个无效的Socket上投递WSARecv操作会出现异常
// 使用的方法是尝试向这个socket发送数据，判断这个socket调用的返回值
// 因为如果客户端网络异常断开(例如客户端崩溃或者拔掉网线等)的时候，服务器端是无法收到客户端断开的通知的

bool CIOCP::_IsSocketAlive(SOCKET s)
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) 
		return false;
	return true;
}

///////////////////////////////////////////////////////////////////
//函数功能：显示并处理完成端口上的错误
bool CIOCP::HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr )
{
	// 如果是超时了，就再继续等吧  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// 确认客户端是否还活着...
		if( !_IsSocketAlive( pContext->m_Socket) )
		{
			this->_LogInfo( _T("检测到客户端异常退出！") );
			this->DiscardSocketContext( pContext );
			return true;
		}
		else
		{
			this->_LogInfo( _T("网络操作超时！重试中...") );
			return true;
		}
	}  

	// 可能是客户端异常退出了
	else if( ERROR_NETNAME_DELETED==dwErr )
	{
		this->_LogInfo( _T("检测到客户端异常退出！") );
		this->DiscardSocketContext( pContext );
		return true;
	}

	else
	{
		this->_LogInfo( _T("完成端口操作出现错误，线程退出。错误代码：%d"),dwErr );
		return false;
	}
}


void CIOCP::VisitUrl(string strUrl)
{
	YHttpRequest req;
	if(!req.ParseUrl(strUrl.c_str()))
		return;
	PER_SOCKET_CONTEXT* pSocketCntx = m_socketContextPool.getItem();
	pSocketCntx->m_ClientAddr = req.m_targetAddr;
	pSocketCntx->m_strUrl = strUrl;
	PER_IO_CONTEXT* pIOCntx = m_IOPool.getItem();
	pSocketCntx->AddIoContext(pIOCntx);
	pIOCntx->m_listBuf.copy((char*)req.Package().c_str());
	PostConnect(pSocketCntx, pIOCntx);
}

void CIOCP::OnVisitEnd(string strUrl, LSITBUF& listBuf)
{
//	TRACE(CString((LPCTSTR)_bstr_t(("visited:" + strUrl+"\n").c_str())));
	int count = listBuf.len();
	char* pData = new char[count];
	listBuf.copyTo(pData);
	char* p = strstr(pData,"\r\n\r\n");

	HTTPPACKAGE package;
	package.strUrl = strUrl;
	package.pHead = pData;
	package.iHead = p - pData+strlen("\r\n\r\n");
	package.pCon = pData+ package.iHead;
	package.iCon = count - package.iHead;
	m_creeper.Digest(package);
	delete  pData;
}

void CIOCP::OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}


void CIOCP::OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}

void CIOCP::OnDataTransmitError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError)
{

}

void CIOCP::OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}

void CIOCP::OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer)
{

}

