#pragma once
#include <map>
#include <stack>
#include <Windows.h>
using namespace std;
#ifndef LONG
#define LONG long
#endif
template<class T>
class lypool
{
private:

public:
	CRITICAL_SECTION m_crisec;
	lypool(int iCount=0){
		InitializeCriticalSection(&m_crisec);
		if(iCount)
			addReserve(iCount);
		
	};

	virtual ~lypool(void)
	{
		drain();
		DeleteCriticalSection(&m_crisec);
	};
	void drain(){
		while(!m_itemStack.empty())
		{
			T* p = m_itemStack.top();
			m_itemStack.pop();
			delete p;
		}
		map<LONG, T*>::iterator itr = m_itemUsed.begin();
		for (; itr != m_itemUsed.end(); itr++)
		{
			delete(itr->second);
		}
		m_itemUsed.clear();
	};
	stack<T*> m_itemStack;
	map<LONG, T*> m_itemUsed;

	T* getItem(){
		EnterCriticalSection(&m_crisec);
		T* p = NULL;
		if (!m_itemStack.empty())
		{
			p = m_itemStack.top();
			m_itemStack.pop();
			m_itemUsed.insert(make_pair((LONG)p, p));
		}
		else
		{
			p = new T;
			m_itemUsed.insert(make_pair((LONG)p, p));
		}
		LeaveCriticalSection(&m_crisec);
		return p;
	}
	int size(){
		return m_itemUsed.size()+m_itemStack.size();
	}
	int idleCount(){
		return m_itemStack.size();
	}
	int usedCount(){
		return m_itemUsed.size();
	}
	void recycle(T* p)
	{
		EnterCriticalSection(&m_crisec);
		map<LONG, T*>::iterator itr = m_itemUsed.find((LONG)p);

		if (itr != m_itemUsed.end())
		{
//			printf("recycle: %d\n", p);
			m_itemStack.push(p);
			m_itemUsed.erase(itr);
		}
		LeaveCriticalSection(&m_crisec);
	}
	void reduceIdle(int iCount){
		EnterCriticalSection(&m_crisec);
		if(iCount<=0) return;
		int idleNum = idleCount();
		iCount = iCount>idleNum?idleNum:iCount;
		for(int i = 0; i< iCount; i++)
		{
			T* p = m_itemStack.top();
			delete p;
			m_itemStack.pop();
		}
//		printf("reduce idle %d\n", iCount);
		LeaveCriticalSection(&m_crisec);
	}
	void addReserve(int iCount=1){
		EnterCriticalSection(&m_crisec);
		if(iCount<0) return;
		for (int i=0; i< iCount; i++)
		{
			m_itemStack.push(new T);
		}
		LeaveCriticalSection(&m_crisec);
	}
};

