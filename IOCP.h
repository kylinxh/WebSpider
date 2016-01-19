/**********************************************************
ԭ�����ߣ�http://blog.csdn.net/piggyxp/article/details/6922277
�޸�ʱ�䣺2013��1��11��22:45:10
**********************************************************/

#pragma once

#include <winsock2.h>
#include <MSWSock.h>
#include <list>
#pragma comment(lib,"ws2_32.lib")

#include "lypool.h"
#include "BufList.h"

#include "WebCreeper.h"
//Ĭ�϶˿�
#define DEFAULT_PORT          12345    
//Ĭ��IP��ַ
#define DEFAULT_IP            "127.0.0.1"


//���������� (1024*8)
#define MAX_LOG_BUFFER_LEN    8192
#define MAX_BUFFER_LEN        8192  
#include <list>
#include <stack>
using namespace std;
// ����ɶ˿���Ͷ�ݵ�I/O����������
typedef enum _OPERATION_TYPE  
{  
	OP_ACCEPT,                     // ��־Ͷ�ݵ�Accept����
	OP_SEND,                       // ��־Ͷ�ݵ��Ƿ��Ͳ���
	OP_RECV,                       // ��־Ͷ�ݵ��ǽ��ղ���
	OP_DISCONNECT,
	OP_CONNECT,
	OP_NULL                        // ���ڳ�ʼ����������
}OPERATION_TYPE;

//ÿ���׽��ֲ���(�磺AcceptEx, WSARecv, WSASend��)��Ӧ�����ݽṹ��OVERLAPPED�ṹ(��ʶ���β���)���������׽��֣�����������������
struct _PER_SOCKET_CONTEXT;
typedef struct _PER_IO_CONTEXT
{
	OVERLAPPED     m_Overlapped;                               // ÿһ���ص�����������ص��ṹ(���ÿһ��Socket��ÿһ����������Ҫ��һ��)              
	_PER_SOCKET_CONTEXT*  m_pOwnerCntx;                               // ������������ʹ�õ�Socket
	WSABUF		   m_wsaBuf;
	LSITBUF	   m_listBuf;
	OPERATION_TYPE m_OpType;                                   // ��ʶ�������������(��Ӧ�����ö��)

	DWORD			m_nTotalBytes;	//�����ܵ��ֽ���
	DWORD			m_nSendBytes;	//�Ѿ����͵��ֽ�������δ��������������Ϊ0

	//���캯��
	_PER_IO_CONTEXT():m_pOwnerCntx(NULL)
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));  
		m_OpType     = OP_NULL;
		m_nTotalBytes	= 0;
		m_nSendBytes	= 0;
	}
	//��������
	~_PER_IO_CONTEXT()
	{
		m_listBuf.release();
	}
	//���û���������
	void ResetBuffer()
	{
		m_listBuf.reset();
		m_nTotalBytes = 0;
		m_nSendBytes = 0;
		m_OpType = OP_NULL;
	}
	//���ã�Reset
	void Reset()
	{
		m_pOwnerCntx= NULL;
		m_listBuf.reset();
		m_nTotalBytes = 0;
		m_nSendBytes = 0;
		m_OpType = OP_NULL;
	}
} PER_IO_CONTEXT, *PPER_IO_CONTEXT;


//ÿ��SOCKET��Ӧ�����ݽṹ(����GetQueuedCompletionStatus����)��-
//SOCKET����SOCKET��Ӧ�Ŀͻ��˵�ַ�������ڸ�SOCKET��������(��Ӧ�ṹPER_IO_CONTEXT)��
typedef struct _PER_SOCKET_CONTEXT
{  
	SOCKET      m_Socket;                                  //���ӿͻ��˵�socket
	string		m_strUrl;								   //���ʵ���ַ
	SOCKADDR_IN m_ClientAddr;                              //�ͻ��˵�ַ
	list<PER_IO_CONTEXT*> m_IoList;             //�׽��ֲ�����������WSARecv��WSASend����һ��PER_IO_CONTEXT

	//���캯��
	_PER_SOCKET_CONTEXT()
	{
		m_Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		memset(&m_ClientAddr, 0, sizeof(m_ClientAddr)); 
	}

	//��������
	~_PER_SOCKET_CONTEXT()
	{
		if( m_Socket!=INVALID_SOCKET )
		{
			closesocket( m_Socket );
			m_Socket = INVALID_SOCKET;
		}
		// �ͷŵ����е�IO����������
		m_IoList.clear();
	}

	void AddIoContext(_PER_IO_CONTEXT* p)
	{
		m_IoList.push_back(p);
		p->m_pOwnerCntx = this;
	}

	// ���������Ƴ�һ��ָ����IoContext
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

// �������̵߳��̲߳���
class CIOCP;
typedef struct _tagThreadParams_WORKER
{
	CIOCP* pIOCPModel;                                   //��ָ�룬���ڵ������еĺ���
	int         nThreadNo;                                    //�̱߳��

} THREADPARAMS_WORKER,*PTHREADPARAM_WORKER; 

typedef PER_SOCKET_CONTEXT CIOCPContext;
typedef PER_IO_CONTEXT CIOCPBuffer;
// CIOCP��
class CIOCP
{
public:
	CIOCP(void);
	~CIOCP(void);

public:
	// ����Socket��
	bool LoadSocketLib();
	// ж��Socket�⣬��������
	void UnloadSocketLib() { WSACleanup(); }

	// ����������
	bool Start(string strIp = "127.0.0.1", int iPort=12306);
	//	ֹͣ������
	void Stop();

	// ��ñ�����IP��ַ
	string GetLocalIP();
	// ���ü����˿�
	void SetPort( const int& nPort ) { m_nPort = nPort; }

	// �����������ָ�룬���ڵ�����ʾ��Ϣ��������
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
	// ��ʼ��IOCP
	bool _InitializeIOCP();
	// ��ʼ��Socket
	bool _InitializeListenSocket();
	// ����ͷ���Դ
	void _DeInitialize();

	void DiscardSocketContext( PER_SOCKET_CONTEXT *pSocketContext , BOOL bReuse=TRUE);

	// ������󶨵���ɶ˿���
	bool _AssociateWithIOCP( PER_SOCKET_CONTEXT *pContext);

	// ������ɶ˿��ϵĴ���
	bool HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr );

	//�̺߳�����ΪIOCP�������Ĺ������߳�
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	//��ñ����Ĵ���������
	int _GetNoOfProcessors();

	//�жϿͻ���Socket�Ƿ��Ѿ��Ͽ�
	bool _IsSocketAlive(SOCKET s);

	//������������ʾ��Ϣ
	void _LogInfo(LPCTSTR lpszFormat, ...) const;

	virtual void OnConnectionEstablished(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// һ�����ӹر�
	virtual void OnConnectionClosing(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// ��һ�������Ϸ����˴���
	virtual void OnDataTransmitError(CIOCPContext *pContext, CIOCPBuffer *pBuffer, int nError);
	// һ�������ϵĶ��������
	virtual void OnReadCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);
	// һ�������ϵ�д�������
	virtual void OnWriteCompleted(CIOCPContext *pContext, CIOCPBuffer *pBuffer);

private:
	CWebCreeper					 m_creeper;
	lypool<PER_SOCKET_CONTEXT>	 m_socketContextPool;
	lypool<PER_IO_CONTEXT>		 m_IOPool;

	BOOL						 m_bStopping;					//����Ϊ�棬˵�����߳��ڵȴ����ȴ��߳���ȫ�˳���Ҫȥ��������
	HANDLE                       m_hShutdownEvent;              // ����֪ͨ�߳�ϵͳ�˳����¼���Ϊ���ܹ����õ��˳��߳�

	HANDLE                       m_hIOCompletionPort;           // ��ɶ˿ڵľ��

	HANDLE*                      m_phWorkerThreads;             // �������̵߳ľ��ָ��

	int		                     m_nThreads;                    // ���ɵ��߳�����

	string                       m_strIP;                       // �������˵�IP��ַ
	int                          m_nPort;                       // �������˵ļ����˿�

	LPVOID                       m_pMain;                       // ������Ľ���ָ�룬����������������ʾ��Ϣ

	CRITICAL_SECTION             m_csContextList;               // ����Worker�߳�ͬ���Ļ�����

//	list<PER_SOCKET_CONTEXT*>   m_ClientContextList;          // �ͻ���Socket��Context��Ϣ        

	PER_SOCKET_CONTEXT*          m_pListenContext;              // ���ڼ�����Socket��Context��Ϣ

	LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����
	LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;
	LPFN_DISCONNECTEX			 m_lpfnDisconnectEx;
	LPFN_CONNECTEX				 m_lpfnConnectEx;
};

