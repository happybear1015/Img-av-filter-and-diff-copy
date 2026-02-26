#pragma once

class CMatchConfigDlg : public CDialogEx
{
public:
    CMatchConfigDlg(CWnd* pParent = nullptr);

    enum { IDD = IDD_MATCH_CONFIG_DIALOG };

    CString m_strDiffPath;
    CString m_strSavePath;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedButtonBrowseDiff();
    afx_msg void OnBnClickedButtonBrowseSave();

private:
    CEdit m_editDiffPath;
    CEdit m_editSavePath;
    CButton m_btnBrowseDiff;
    CButton m_btnBrowseSave;

    CString GetFolderPath();
};
