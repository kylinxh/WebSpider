
// XCreeperDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "XCreeper.h"
#include "XCreeperDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CXCreeperDlg 对话框




CXCreeperDlg::CXCreeperDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CXCreeperDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CXCreeperDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CXCreeperDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_RUN, &CXCreeperDlg::OnBnClickedBtnRun)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CXCreeperDlg 消息处理程序

BOOL CXCreeperDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	GetDlgItem(IDC_EDIT_URL)->SetWindowTextW(L"http://www.qq.com/");
	m_iocp.Start();
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CXCreeperDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CXCreeperDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CXCreeperDlg::OnBnClickedBtnRun()
{
	// TODO: 在此添加控件通知处理程序代码
	CString strUrl;
	GetDlgItem(IDC_EDIT_URL)->GetWindowTextW(strUrl);
	m_iocp.VisitUrl((LPCSTR)_bstr_t(strUrl));
//	m_iocp.VisitUrl("http://erge.qqbaobao.com/s/jingdianergedq100/index.html");
//	m_iocp.VisitUrl("http://www.bz55.com/uploads/allimg/150309/139-150309101A0.jpg");
}


void CXCreeperDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	m_iocp.Stop();
	CDialogEx::OnClose();
}
