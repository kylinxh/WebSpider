
// XCreeperDlg.h : ͷ�ļ�
//

#pragma once
#include "IOCP.h"

// CXCreeperDlg �Ի���
class CXCreeperDlg : public CDialogEx
{
// ����
public:
	CXCreeperDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_XCREEPER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

public:
	CIOCP m_iocp;
// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnRun();
	afx_msg void OnClose();
};
