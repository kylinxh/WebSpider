#pragma once
#ifndef BYTE
#define BYTE unsigned char 
#endif

#ifndef NULL
#define NULL 0
#endif

#define SINGLE_NODE_BUF_SIZE 8192

struct NODE{
	
};

struct LIST_BUF_ITERATOR;
struct LIST_SEGMENT;

struct NODEBUF
{
	NODEBUF* pPrev;
	NODEBUF* pNext;
	char buf[SINGLE_NODE_BUF_SIZE];
	int len;
	NODEBUF()
		:pPrev(NULL)
		,pNext(NULL)
		,len(0)
	{
		buf[0]=0;
		len = 0;
	}
	int size()
	{
		return SINGLE_NODE_BUF_SIZE;
	}
	bool isFull()
	{
		return len - SINGLE_NODE_BUF_SIZE==0?true:false;
	}
	int copy(char* pStr, int iLen=-1)
	{
		if(iLen == -1)
		{
			iLen = strlen(pStr);
			
		}
		int copyLen = SINGLE_NODE_BUF_SIZE - this->len;
		copyLen=copyLen>iLen?iLen:copyLen;
		memcpy(buf+this->len, pStr, copyLen);
		this->len += copyLen;
		return copyLen;
	}
	int copyTo(char* pStr)
	{
		memcpy(pStr, buf, this->len);
		return this->len;
	}
	void Reset()
	{
		len = 0;
		memset(buf,0, SINGLE_NODE_BUF_SIZE);
	}
};

struct ATOM_ITERATOR
{
	NODEBUF* pNode;
	int iPos;
	ATOM_ITERATOR& operator++() //前缀自加重载；（前置版本prefix）
	{
		int iSize = pNode->size();
		if(iPos < iSize-1)
			iPos++;
		else
		{
			pNode=pNode->pNext;
			iPos=0;
		}
		return (*this);
	}

	ATOM_ITERATOR operator++(int) //前缀自加重载；（前置版本prefix）
	{
		ATOM_ITERATOR old(*this);
		++(*this);
		return old;
	}

	ATOM_ITERATOR* operator+(int offset)
	{
		for(int i=0; i<offset; i++)
			++(*this);
	}
};


struct LSITBUF
{
	NODEBUF* pNode;
	LSITBUF()
		:pHead(NULL)
		,pBack(NULL)
		,pNode(NULL)
	{
		newNode();
	}
	~LSITBUF()
	{}

	void reset()
	{
		pNode = pHead;
		while(pNode)
		{
			pNode->Reset();
			pNode = pNode->pNext;
		}
		pNode = pHead;
	}
	void release()
	{
		NODEBUF* pNode= pHead;
		while(pNode)
		{
			NODEBUF* p=pNode;
			pNode = pNode->pNext;
			delete p;
		}
	}
private:
	NODEBUF* pHead;
	NODEBUF* pBack;
	
private:
	NODEBUF* newNode()
	{
		pNode = new NODEBUF;
		
		if(pHead==NULL)
		{
			pHead =pBack= pNode;
		}
		else
		{
			pNode->pPrev = pBack;
			pBack->pNext = pNode;
			pBack = pNode;
		}
		return pNode;
	}
public:
	NODEBUF* head(){return pHead;}
	NODEBUF* back(){return pBack;}
	NODEBUF* getNode(){
		if(pNode==NULL||pNode->isFull())
			pNode = newNode();
		return pNode;
	}
	void copy(char* pStr, int iLen=-1)
	{
		iLen = iLen==-1?strlen(pStr):iLen;
		pNode = pHead;
		int copyLen = 0;
		while(iLen>0)
		{
			copyLen = pNode->copy(pStr,iLen);
			pNode = getNode();
			iLen -= copyLen;
		}
	}

	int copyTo(char* pStr)
	{
		int count = 0;
		pNode = pHead;
		while(pNode&&pNode->len>0)
		{
			count += pNode->copyTo(pStr+count);
			pNode = pNode->pNext;
		}
		return count;
	}
	int len()
	{
		int count = 0;
		NODEBUF* pNode= pHead;
		while(pNode)
		{
			count += pNode->len;
			pNode=pNode->pNext;
		}
		return count;
	}

	ATOM_ITERATOR find(char)
	{

	}
};

struct HTTPPACKAGE
{
	string strUrl;
	string strConCatlog[100];
	string strConType[100];
	char* pHead;
	int iHead;
	char* pCon;
	int iCon;
};