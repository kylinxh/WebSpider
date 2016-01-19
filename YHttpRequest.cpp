#include "StdAfx.h"
#include "YHttpRequest.h"


YHttpRequest::YHttpRequest()
{
}

YHttpRequest::~YHttpRequest(void)
{

}

BOOL YHttpRequest::ParseUrl(string url)
{
	if(url.length()<=0) return false;
//	::OutputDebugStringA(string("ParseUrl:"+url+"\n").c_str());
	char* pUrl = (char*)url.c_str();
	char* pos = strstr(pUrl, "http://");
	if(!pos) return false;
	pos += strlen("http://");

	char szHost[100];
	char szResPath[500];
	sscanf(pos, "%[^/]%s", szHost, szResPath);
	m_strHost = szHost;
	m_strResPath = szResPath;

	HOSTENT* pHost = gethostbyname(m_strHost.c_str());
	if(!pHost)
		return false;
	::OutputDebugStringA(string("we'll visit Url:"+url+"\n").c_str());
	memset(&m_targetAddr, 0, sizeof(m_targetAddr));
	m_targetAddr.sin_family = AF_INET;
	m_targetAddr.sin_port = htons(80);
	memcpy(&m_targetAddr.sin_addr, pHost->h_addr, 4);
	return true;
}

string YHttpRequest::Package()
{
	string request = "GET "+ m_strResPath + " HTTP/1.0\r\nHost: "+ m_strHost + "\r\nConnection:Close\r\n\r\n";
	return request;
}