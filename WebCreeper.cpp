#include "StdAfx.h"
#include "IOCP.h"
#include "WebCreeper.h"
#include <fstream>
#include <regex>
int CWebCreeper::m_iSerialNum = 0;
CWebCreeper::CWebCreeper(void)
{
	::InitializeCriticalSection(&m_csUrl);
}


CWebCreeper::~CWebCreeper(void)
{
	::DeleteCriticalSection(&m_csUrl);
}

void CWebCreeper::Digest(HTTPPACKAGE& package)
{
//	char aChar;
//	aChar = *package.pCon;
//	*package.pCon = '\0';
	char buf[200]={0};
	char* pType = strstr(package.pHead, "Content-Type: ");
	if(pType){
		pType = pType + strlen("Content-Type: ");
		sscanf(pType, "%[^\r\n]", buf);
		char* pos1= strstr(buf,";");
		if(pos1)
			*pos1='\0';
	}

	char szType[200]={0};
	char szCatelog[200]={0};
	if(strlen(buf)>0)
	{
		sscanf(buf, "%[^/]", szCatelog);
		sscanf(buf, "%*[^/]/%s",szType);
	}


	::OutputDebugStringA("save image");
	string filePath;
	CreatePath(filePath, (char*)package.strUrl.c_str(), szType);
	if(filePath.size() == 0) return;//居然有些没有类型，无语
	if(strcmp(szCatelog,"text")==0)
	{
//		ofstream ofile(filePath.c_str(), ios::app);
//		if(ofile.is_open())
//		{
			//		int iLen = ofile.tellp();
//			ofile<<package.pCon;
//			ofile.close();
//		}
		ParseWebPage(package);
	}
	else{
		ofstream ofile(filePath.c_str(), ios::binary);
		int PerWriteSize = 4096;
		int iTotalSize = package.iCon;
		int iHaveWrite=0;
		while(iTotalSize - iHaveWrite > PerWriteSize)
		{
			ofile.write(package.pCon + iHaveWrite, iTotalSize);
			iHaveWrite += PerWriteSize;
		}
		ofile.write(package.pCon + iHaveWrite, iTotalSize - iHaveWrite);
		ofile.close();
	}

}

char* CWebCreeper::charrpos(char* p, char c)
{
	char* pTag = NULL;
	for(; *p != '\0'; p++)
		if(*p == c)
			pTag = p;
	return pTag;
}

void CWebCreeper::CreateDir(char* path)
{
	char* p = strstr(path,"/");
	while(p)
	{
		*p='\0';
		if(!::PathIsDirectoryA(path))
			CreateDirectoryA(path, NULL);
		*p='/';
		p +=1;
		p = strstr(p, "/");
	}
}
void CWebCreeper::CreatePath(string& path, char* url, string strFileSuffix)
{
	//http://www.qq.com
	//http://www.qq.com/wx
	//http://www.qq.com/wx/
	//http://www.qq.com/wx/0
	//http://www.qq.com/wx/0qw.jpg
	//http://www.qq.com/wx/index.html;
	//http://www.so-elite.com/wx/BackAdmin/TopicList.php?CateId=4
	if(strFileSuffix.length()<=0) return;//有些没类型，无语啊

	string fileName;
	::InterlockedIncrement((long*)&m_iSerialNum);
	char numbuf[50];
	itoa(m_iSerialNum,numbuf,10);
	string tmp = numbuf;
	tmp += "_";
	fileName = tmp+"."+strFileSuffix;
	path = string("./") + "images/" + fileName;
	CreateDir((char*)path.c_str());
}

BOOL CWebCreeper::ParseUrl(string url, string& host, string& resource)
{
	char* pUrl = (char*)url.c_str();
	char* pos = strstr(pUrl, "http://");
	if(!pos) return false;
	pos += strlen("http://");

	char* pos1 = strstr(pos, "/");
	if(pos1)
	{
		char pHost[100];
		char pResource[500];
		sscanf(pos, "%[^/]%s", pHost, pResource);
		host = pHost;
		resource = pResource;
		return true;
	}
	else
	{
		return false;
	}
	return true;
}

void CWebCreeper::ParseWebPage(HTTPPACKAGE& package)
{
	//	http://inews.gtimg.com/newsapp_match/0/59852679/0 这种是可以的是个图片，随便加个什么后缀都是图片，一般是jpg
	//  http://inews.gtimg.com/newindex/				  这可能是个主页文件
	string url = package.strUrl;
	string host, resource;
	ParseUrl(url, host, resource);

	string page = package.pCon;

	//regex e("http://[^\'\"]*");
	regex e("http://[^\s>\"]*");
	smatch sm;
	string::const_iterator itr = page.begin();
	string::const_iterator itrE = page.end();
	while(regex_search(itr, itrE, sm, e))
	{
		VisitUrl(sm[0].str());
		itr = sm[0].second;
	}
}

void CWebCreeper::VisitUrl(string& strUrl)
{
	if(IsUrlVisited(strUrl)) return;
	m_pIOCP->VisitUrl(strUrl);
	RecordUrl(strUrl);
}

BOOL CWebCreeper::IsUrlVisited(string& strUrl)
{
	::OutputDebugStringA(("Query Url:"+strUrl +"\n").c_str());
	::EnterCriticalSection(&m_csUrl);
	return m_visitedUrlSet.end()!=m_visitedUrlSet.find(strUrl);
	::LeaveCriticalSection(&m_csUrl);
}
void CWebCreeper::RecordUrl(string& strUrl)
{
	::OutputDebugStringA(("Record Url:"+strUrl +"\n").c_str());
	::EnterCriticalSection(&m_csUrl);
	m_visitedUrlSet.insert(strUrl);
	::LeaveCriticalSection(&m_csUrl);
}