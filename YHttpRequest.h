#pragma once
#include <string>
#include <map>
using namespace std;
class YHttpRequest
{
public:
	YHttpRequest(void);
	YHttpRequest(string strUrl);
	virtual ~YHttpRequest(void);
	string Package();
	BOOL ParseUrl(string url);
	string m_strHostUrl;
	string m_strResPath;
	string m_strHost;
	SOCKADDR_IN m_targetAddr;
};

