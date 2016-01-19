
// XCreeperDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "XCreeper.h"
#include "XCreeperDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CXCreeperDlg �Ի���




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


// CXCreeperDlg ��Ϣ�������

BOOL CXCreeperDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
	GetDlgItem(IDC_EDIT_URL)->SetWindowTextW(L"http://www.qq.com/");
	m_iocp.Start();
	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CXCreeperDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CXCreeperDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CXCreeperDlg::OnBnClickedBtnRun()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	CString strUrl;
	GetDlgItem(IDC_EDIT_URL)->GetWindowTextW(strUrl);
	m_iocp.VisitUrl((LPCSTR)_bstr_t(strUrl));
//	m_iocp.VisitUrl("http://erge.qqbaobao.com/s/jingdianergedq100/index.html");
//	m_iocp.VisitUrl("http://www.bz55.com/uploads/allimg/150309/139-150309101A0.jpg");
}


void CXCreeperDlg::OnClose()
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	m_iocp.Stop();
	CDialogEx::OnClose();
}
