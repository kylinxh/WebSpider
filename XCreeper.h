
// XCreeper.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CXCreeperApp:
// �йش����ʵ�֣������ XCreeper.cpp
//

class CXCreeperApp : public CWinApp
{
public:
	CXCreeperApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CXCreeperApp theApp;