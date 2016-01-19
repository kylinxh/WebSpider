#pragma once
#include <hash_set>
#include "BufList.h"
class CIOCP;
class CWebCreeper
{
public:
	CWebCreeper(void);
	~CWebCreeper(void);
	CIOCP* m_pIOCP;
	void Digest(HTTPPACKAGE& package);
	static int m_iSerialNum;
	char* charrpos(char* p, char c);
	void CreateDir(char* path);
	void CreatePath(string& path,char* url, string strFileSuffix);
	BOOL ParseUrl(string url, string& host, string& resource);
	void ParseWebPage(HTTPPACKAGE& package);
	hash_set<string>m_visitedUrlSet;
	CRITICAL_SECTION	m_csUrl;
	void VisitUrl(string& strUrl);
	BOOL IsUrlVisited(string& strUrl);
	void RecordUrl(string& strUrl);
};

