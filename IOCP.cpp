#include "StdAfx.h"
#include "IOCP.h"
#include "YHttpRequest.h"
// ÿһ���������ϲ������ٸ��߳�
#define WORKER_THREADS_PER_PROCESSOR 1
// ͬʱͶ�ݵ�AcceptEx���������
#define MAX_POST_ACCEPT              10
// ���ݸ�Worker�̵߳��˳��ź�
#define EXIT_CODE                    NULL


// �ͷ�ָ��;����Դ�ĺ�

// �ͷ�ָ���
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// �ͷž����
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// �ͷ�Socket��
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
	// ȷ����Դ�����ͷ�
	this->Stop();
	UnloadSocketLib();
}


/*********************************************************************
*�������ܣ��̺߳���������GetQueuedCompletionStatus����������д���
*����������lpParam��THREADPARAMS_WORKER����ָ�룻
*����˵����GetQueuedCompletionStatus��ȷ����ʱ��ʾĳ�����Ѿ���ɣ��ڶ�������lpNumberOfBytes��ʾ�����׽��ִ�����ֽ�����
����lpCompletionKey��lpOverlapped������Ҫ����Ϣ�����ѯMSDN�ĵ���
*********************************************************************/
DWORD WINAPI CIOCP::_WorkerThread(LPVOID lpParam)
{    
	THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
	CIOCP* pIOCPModel = (CIOCP*)pParam->pIOCPModel;
	int nThreadNo = (int)pParam->nThreadNo;

//	pIOCPModel->_LogInfo(L"�������߳�������ID: %d.",nThreadNo);

	OVERLAPPED           *pOverlapped = NULL;
	PER_SOCKET_CONTEXT   *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;

	//ѭ����������ֱ�����յ�Shutdown��ϢΪֹ
	while (WAIT_OBJECT_0 != WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0))
	{
		BOOL bReturn = GetQueuedCompletionStatus(
			pIOCPModel->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);

		//����EXIT_CODE�˳���־����ֱ���˳�
		if ( EXIT_CODE==(DWORD)pSocketContext)
		{
			break;
		}

		//����ֵΪ0����ʾ����
		if( !bReturn )  
		{  
			DWORD dwErr = GetLastError();
			// ��ʾһ����ʾ��Ϣ
			if( !pIOCPModel->HandleError( pSocketContext,dwErr ) )
			{
				pIOCPModel->OnDataTransmitError(pSocketContext, NULL, dwErr);
				break;
			}

			continue;  
		}  
		else  
		{  	
			// ��ȡ����Ĳ���
			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  

			// �ж��Ƿ��пͻ��˶Ͽ���
			if((0 == dwBytesTransfered) && ( OP_RECV==pIoContext->m_OpType || OP_SEND==pIoContext->m_OpType))  
			{ 
//				pIOCPModel->_LogInfo( _T("�ͻ��� %s:%d �Ͽ�����."),(LPCTSTR)_bstr_t(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr)), ntohs(pSocketContext->m_ClientAddr.sin_port) );

				
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
						pIOCPModel->m_IOPool.recycle(pIoContext);//ǧ��Ҫ������|
					}
					break;
				default:
					break;
				} //switch
			}//if
		}//if

	}//while

	// �ͷ��̲߳���
	RELEASE(lpParam);	

	return 0;
}

//�������ܣ���ʼ���׽���
bool CIOCP::LoadSocketLib()
{    
	WSADATA wsaData;
	int nResult;
	nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	// ����(һ�㶼�����ܳ���)
	if (NO_ERROR != nResult)
	{
		this->_LogInfo(_T("��ʼ��WinSock 2.2ʧ�ܣ�\n"));
		return false; 
	}

	return true;
}


//�������ܣ�����������
bool CIOCP::Start(string strIp, int iPort)
{
	// ��ʼ���̻߳�����
	InitializeCriticalSection(&m_csContextList);

	// ����ϵͳ�˳����¼�֪ͨ
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// ��ʼ��IOCP
	if (false == _InitializeIOCP())
	{
		this->_LogInfo(_T("��ʼ��IOCPʧ�ܣ�\n"));
		return false;
	}

	// ��ʼ��Socket
	if( false==_InitializeListenSocket() )
	{
		this->_LogInfo(_T("Listen Socket��ʼ��ʧ�ܣ�\n"));
		this->_DeInitialize();
		return false;
	}

	this->_LogInfo(_T("ϵͳ׼���������Ⱥ�����....\n"));
	return true;
}

void CIOCP::_DeInitialize()
{
	// ɾ���ͻ����б�Ļ�����
	DeleteCriticalSection(&m_csContextList);

	// �ر�ϵͳ�˳��¼����
	RELEASE_HANDLE(m_hShutdownEvent);

	// �ͷŹ������߳̾��ָ��
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}
	
	RELEASE(m_phWorkerThreads);

	// �ر�IOCP���
	RELEASE_HANDLE(m_hIOCompletionPort);


	_LogInfo(L"�ͷ���Դ���.\n");
}

////////////////////////////////////////////////////////////////////
//	��ʼ����ϵͳ�˳���Ϣ���˳���ɶ˿ں��߳���Դ
void CIOCP::Stop()
{
	if( m_hIOCompletionPort!= INVALID_HANDLE_VALUE && m_hIOCompletionPort)
	{
		// ����ر���Ϣ֪ͨ
		m_bStopping = TRUE;
		SetEvent(m_hShutdownEvent);
		
		for (int i = 0; i < m_nThreads; i++)
		{
			// ֪ͨ���е���ɶ˿ڲ����˳�
			PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
		}

		// �ȴ����еĿͻ�����Դ�˳�
		WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);

		m_bStopping= FALSE;
		// �ͷ�������Դ
		this->_DeInitialize();

		this->_LogInfo(L"ֹͣ����\n");
	}	
}

bool CIOCP::_InitializeIOCP()
{
	// ������һ����ɶ˿�
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

	if ( NULL == m_hIOCompletionPort)
	{
		this->_LogInfo(_T("������ɶ˿�ʧ�ܣ��������: %d!\n"), WSAGetLastError());
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

	_LogInfo(L" ���� _WorkerThread %d ��.\n", m_nThreads );

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
		this->_LogInfo(L"WSAIoctl δ�ܻ�ȡAcceptEx����ָ�롣�������: %d\n", WSAGetLastError()); 
		this->_DeInitialize();
		return false;  
	}
	closesocket(s);
	return lpFun;
}
/////////////////////////////////////////////////////////////////
// ��ʼ��Socket
bool CIOCP::_InitializeListenSocket()
{
	// AcceptEx �� GetAcceptExSockaddrs ��GUID�����ڵ�������ָ��
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
	// ��ʼ������
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
	// ��ʼ����ɺ󣬣�Ͷ��WSARecv����
//	CString str;
//	str.Format(L"offset=%d,post recv len=%d\n\n",pNode->len,p_wbuf->len);
//	AfxMessageBox(str);
	int nBytesRecv = WSARecv( pSocketContext->m_Socket, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	if (SOCKET_ERROR == nBytesRecv)
	{
		
		DWORD errCode =WSAGetLastError();
		if(errCode != WSA_IO_PENDING)
		{
			this->_LogInfo(L"Ͷ��WSARecvʧ��,Error Code=%d\n", WSAGetLastError());
			return false;
		}
	}
	return true;
}

bool CIOCP::PostWrite(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	// ��ʼ������
	DWORD dwFlags = 0;
	DWORD dwSendNumBytes = 0;
	WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	pIoContext->m_OpType = OP_SEND;
	//Ͷ��WSASend���� -- ��Ҫ�޸�
	ZeroMemory(p_ol,sizeof(OVERLAPPED));
	
	int nRet = WSASend(pSocketContext->m_Socket, p_wbuf, 1, &dwSendNumBytes, dwFlags,
		&pIoContext->m_Overlapped, NULL);

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	if (SOCKET_ERROR == nRet)
	{
		DWORD errCode =WSAGetLastError();
		if(errCode != WSA_IO_PENDING)
		{
			this->_LogInfo(L"Ͷ��WSASendʧ��,Error Code=%d\n", WSAGetLastError());
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
	//iocp����̫���ˣ�Ҫһ
	return true;
}

bool CIOCP::_DoWrite(PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext)
{
	if (pIoContext->m_nSendBytes < pIoContext->m_nTotalBytes)
	{
		//����δ�ܷ����꣬������������
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
//		this->_LogInfo((L"ִ��CreateIoCompletionPort()���ִ���.������룺%d"),GetLastError());
		return false;
	}

	return true;
}



////////////////////////////////////////////////////////////////
//	�Ƴ�ĳ���ض���Context
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
	// ��ñ���������
	char hostname[MAX_PATH] = {0};
	gethostname(hostname,MAX_PATH);                
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if(lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	LPSTR lpAddr = lpHostEnt->h_addr_list[0];      

	// ��IP��ַת�����ַ�����ʽ
	struct in_addr inAddr;
	memmove(&inAddr,lpAddr,4);
	m_strIP =  inet_ntoa(inAddr);        

	return m_strIP;
}

///////////////////////////////////////////////////////////////////
// ��ñ����д�����������
int CIOCP::_GetNoOfProcessors()
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}

/////////////////////////////////////////////////////////////////////
// ������������ʾ��ʾ��Ϣ
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
// �жϿͻ���Socket�Ƿ��Ѿ��Ͽ���������һ����Ч��Socket��Ͷ��WSARecv����������쳣
// ʹ�õķ����ǳ��������socket�������ݣ��ж����socket���õķ���ֵ
// ��Ϊ����ͻ��������쳣�Ͽ�(����ͻ��˱������߰ε����ߵ�)��ʱ�򣬷����������޷��յ��ͻ��˶Ͽ���֪ͨ��

bool CIOCP::_IsSocketAlive(SOCKET s)
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) 
		return false;
	return true;
}

///////////////////////////////////////////////////////////////////
//�������ܣ���ʾ��������ɶ˿��ϵĴ���
bool CIOCP::HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr )
{
	// ����ǳ�ʱ�ˣ����ټ����Ȱ�  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// ȷ�Ͽͻ����Ƿ񻹻���...
		if( !_IsSocketAlive( pContext->m_Socket) )
		{
			this->_LogInfo( _T("��⵽�ͻ����쳣�˳���") );
			this->DiscardSocketContext( pContext );
			return true;
		}
		else
		{
			this->_LogInfo( _T("���������ʱ��������...") );
			return true;
		}
	}  

	// �����ǿͻ����쳣�˳���
	else if( ERROR_NETNAME_DELETED==dwErr )
	{
		this->_LogInfo( _T("��⵽�ͻ����쳣�˳���") );
		this->DiscardSocketContext( pContext );
		return true;
	}

	else
	{
		this->_LogInfo( _T("��ɶ˿ڲ������ִ����߳��˳���������룺%d"),dwErr );
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

