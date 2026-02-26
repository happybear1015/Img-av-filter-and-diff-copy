#include "pch.h"
#include "AIImageClassifier.h"
#include "MatchConfigDlg.h"
#include "afxdialogex.h"
#include <shlobj.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CMatchConfigDlg::CMatchConfigDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MATCH_CONFIG_DIALOG, pParent)
{
}

void CMatchConfigDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_DIFF_PATH, m_editDiffPath);
    DDX_Control(pDX, IDC_EDIT_SAVE_PATH, m_editSavePath);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_DIFF, m_btnBrowseDiff);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_SAVE, m_btnBrowseSave);
}

BOOL CMatchConfigDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    if (!m_strDiffPath.IsEmpty()) {
        m_editDiffPath.SetWindowText(m_strDiffPath);
    }
    if (!m_strSavePath.IsEmpty()) {
        m_editSavePath.SetWindowText(m_strSavePath);
    }

    return TRUE;
}

BEGIN_MESSAGE_MAP(CMatchConfigDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_DIFF, &CMatchConfigDlg::OnBnClickedButtonBrowseDiff)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_SAVE, &CMatchConfigDlg::OnBnClickedButtonBrowseSave)
END_MESSAGE_MAP()

void CMatchConfigDlg::OnBnClickedButtonBrowseDiff()
{
    CString folderPath;
    IFileOpenDialog* pFileOpen;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFileOpen->GetOptions(&dwOptions);
        pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        pFileOpen->SetTitle(_T("选择Diff图像路径"));

        hr = pFileOpen->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    folderPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();

        if (!folderPath.IsEmpty()) {
            m_editDiffPath.SetWindowText(folderPath);
            m_strDiffPath = folderPath;
        }
    }
    else {
        folderPath = GetFolderPath();
        if (!folderPath.IsEmpty()) {
            m_editDiffPath.SetWindowText(folderPath);
            m_strDiffPath = folderPath;
        }
    }
}

void CMatchConfigDlg::OnBnClickedButtonBrowseSave()
{
    CString folderPath;
    IFileOpenDialog* pFileOpen;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFileOpen->GetOptions(&dwOptions);
        pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        pFileOpen->SetTitle(_T("选择保存路径"));

        hr = pFileOpen->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    folderPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();

        if (!folderPath.IsEmpty()) {
            m_editSavePath.SetWindowText(folderPath);
            m_strSavePath = folderPath;
        }
    }
    else {
        folderPath = GetFolderPath();
        if (!folderPath.IsEmpty()) {
            m_editSavePath.SetWindowText(folderPath);
            m_strSavePath = folderPath;
        }
    }
}

CString CMatchConfigDlg::GetFolderPath()
{
    CString folderPath;
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = _T("选择文件夹");
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != 0) {
        TCHAR path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path)) {
            folderPath = path;
        }
        CoTaskMemFree(pidl);
    }
    return folderPath;
}
