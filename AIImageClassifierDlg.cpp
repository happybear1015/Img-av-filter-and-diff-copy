// AIImageClassifierDlg.cpp
#include "pch.h"
#include "AIImageClassifier.h"
#include "AIImageClassifierDlg.h"
#include "CSVManager.h"  // 添加这行
#include "MatchConfigDlg.h"
#include "afxdialogex.h"
#include <filesystem>
#include <thread>
#include <fstream>
#include <shlobj.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <iostream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace fs = std::filesystem;

// 线程参数结构体
struct ThreadParams {
    CAIImageClassifierDlg* pDlg;
    CString sourcePath;
    CString outputPath;
};

// 样品信息结构体
struct SampleInfo {
    std::wstring sampleName;
    std::wstring samplePath;
    std::vector<std::wstring> imageFiles;
    int imageCount;
};

// CAIImageClassifierDlg 对话框
CAIImageClassifierDlg::CAIImageClassifierDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_AIIMAGECLASSIFIER_DIALOG, pParent)
    , m_bIsProcessing(false)
    , m_nCopyCount(0)
    , m_bMatchConfigEnabled(false)
{
}

void CAIImageClassifierDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_SOURCE_PATH, m_editSourcePath);
    DDX_Control(pDX, IDC_EDIT_OUTPUT_PATH, m_editOutputPath);
    DDX_Control(pDX, IDC_EDIT_MODEL_PATH, m_editModelPath);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_SOURCE, m_btnBrowseSource);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_OUTPUT, m_btnBrowseOutput);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_MODEL, m_btnBrowseModel);
    DDX_Control(pDX, IDC_BUTTON_LOAD_MODEL, m_btnLoadModel);
    DDX_Control(pDX, IDC_BUTTON_START, m_btnStart);
    DDX_Control(pDX, IDC_LIST_CATEGORIES, m_listCategories);
    DDX_Control(pDX, IDC_PROGRESS, m_progressCtrl);
    DDX_Control(pDX, IDC_LIST_RESULTS, m_listResults);
    DDX_Control(pDX, IDC_STATIC_PREVIEW, m_staticPreview);
    DDX_Control(pDX, IDC_STATIC_STATUS, m_staticStatus);
    DDX_Control(pDX, IDC_STATIC_STATS, m_staticStats);
    DDX_Control(pDX, IDC_BUTTON_OpenResultDir, m_btnOpenResultDir);
    DDX_Control(pDX, IDC_BUTTON_VIEW_SAMPLES, m_btnViewSamples);

    DDX_Control(pDX, IDC_SLIDER_THRESHOLD, m_sliderThreshold);
    DDX_Control(pDX, IDC_STATIC_THRESHOLD_VALUE, m_staticThresholdValue);
    DDX_Control(pDX, IDC_LIST_DISTRIBUTION, m_listDistribution);
    DDX_Control(pDX, IDC_BUTTON_APPLY_THRESHOLD, m_btnApplyThreshold);
    DDX_Control(pDX, IDC_STATIC_THRESHOLD_STATS, m_staticThresholdStats);
    DDX_Control(pDX, IDC_BUTTON_MATCH_CONFIG, m_btnMatchConfig);
    DDX_Control(pDX, IDC_STATIC_COPY_COUNT, m_staticCopyCount);
}

BEGIN_MESSAGE_MAP(CAIImageClassifierDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_SOURCE, &CAIImageClassifierDlg::OnBnClickedButtonBrowsesource)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_OUTPUT, &CAIImageClassifierDlg::OnBnClickedButtonBrowseoutput)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_MODEL, &CAIImageClassifierDlg::OnBnClickedButtonBrowsemodel)
    ON_BN_CLICKED(IDC_BUTTON_LOAD_MODEL, &CAIImageClassifierDlg::OnBnClickedButtonLoadmodel)
    ON_BN_CLICKED(IDC_BUTTON_START, &CAIImageClassifierDlg::OnBnClickedButtonStart)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_RESULTS, &CAIImageClassifierDlg::OnLvnItemchangedListResults)
    ON_BN_CLICKED(IDC_BUTTON_OpenResultDir, &CAIImageClassifierDlg::OnBnClickedButtonOpenResultDir)
    ON_BN_CLICKED(IDC_BUTTON_VIEW_SAMPLES, &CAIImageClassifierDlg::OnBnClickedButtonViewSamples)
    ON_MESSAGE(WM_USER + 100, &CAIImageClassifierDlg::OnProcessingFinished)
    ON_MESSAGE(WM_USER + 101, &CAIImageClassifierDlg::OnUpdateProgress)
    ON_MESSAGE(WM_USER + 102, &CAIImageClassifierDlg::OnSetProgressRange)
    ON_MESSAGE(WM_USER + 103, &CAIImageClassifierDlg::OnUpdatePreview)
    ON_WM_TIMER()
    ON_WM_HSCROLL()
    ON_BN_CLICKED(IDC_BUTTON_APPLY_THRESHOLD, &CAIImageClassifierDlg::OnBnClickedButtonApplyThreshold)
    ON_BN_CLICKED(IDC_BUTTON_MATCH_CONFIG, &CAIImageClassifierDlg::OnBnClickedButtonMatchConfig)
END_MESSAGE_MAP()

BOOL CAIImageClassifierDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置图标
    SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
    SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);

    // 初始化列表控件
    m_listCategories.InsertColumn(0, _T("类别名称"), LVCFMT_LEFT, 180);

    m_listResults.InsertColumn(0, _T("文件名"), LVCFMT_LEFT, 200);
    m_listResults.InsertColumn(1, _T("类别"), LVCFMT_LEFT, 100);
    m_listResults.InsertColumn(2, _T("置信度"), LVCFMT_LEFT, 80);
    m_listResults.InsertColumn(3, _T("处理时间"), LVCFMT_LEFT, 100);

    m_listClassImages.InsertColumn(0, _T("图像文件"), LVCFMT_LEFT, 180);

    // 设置列表样式
    m_listResults.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listClassImages.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);


    // 新增：初始化CSV相关变量
    m_bIsProcessing = false;
    m_bStopRequested = false;  // 新增：初始化停止标志

    // 设置进度条范围为0-100（百分比）
    m_progressCtrl.SetRange(0, 100);
    m_progressCtrl.SetPos(0);

    // 初始化状态
    m_staticStatus.SetWindowText(_T("准备就绪"));
    m_staticStats.SetWindowText(_T("统计信息将在这里显示"));

    // 设置控件状态
    m_btnStart.EnableWindow(FALSE);
    m_btnStart.SetWindowText(_T("开始筛选"));  // 确保初始文本正确
    m_btnMatchConfig.EnableWindow(FALSE);
    UpdateCopyCountDisplay();

    // 启动定时器用于更新进度
    SetTimer(1, 100, NULL);

    // 初始化阈值控件
    InitializeThresholdControls();

    return TRUE;
}

// 字符编码转换函数
std::string CAIImageClassifierDlg::CT2UTF8(const CString& str)
{
    if (str.IsEmpty()) return std::string();

    int wideLen = str.GetLength();
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, str.GetString(), wideLen, NULL, 0, NULL, NULL);
    if (utf8Len == 0) return std::string();

    std::vector<char> buffer(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, str.GetString(), wideLen, buffer.data(), utf8Len, NULL, NULL);

    return std::string(buffer.data(), utf8Len);
}

CString CAIImageClassifierDlg::UTF82CT(const std::string& utf8Str)
{
    if (utf8Str.empty()) return CString();

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    if (wideLen == 0) return CString();

    std::vector<wchar_t> buffer(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, buffer.data(), wideLen);

    return CString(buffer.data());
}

std::wstring CAIImageClassifierDlg::CStringToWString(const CString& str)
{
    return std::wstring(str.GetString());
}

CString CAIImageClassifierDlg::WStringToCString(const std::wstring& wstr)
{
    return CString(wstr.c_str());
}

// 文件路径处理函数
bool CAIImageClassifierDlg::CreateDirectoryRecursive(const CString& path)
{
    if (path.IsEmpty()) return false;

    if (PathExists(path)) return true;

    int pos = path.ReverseFind('\\');
    if (pos != -1) {
        CString parentDir = path.Left(pos);
        if (!CreateDirectoryRecursive(parentDir)) {
            return false;
        }
    }

    return CreateDirectory(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool CAIImageClassifierDlg::PathExists(const CString& path)
{
    DWORD attr = GetFileAttributes(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
}

CString CAIImageClassifierDlg::GetFolderPath()
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

void CAIImageClassifierDlg::OnBnClickedButtonBrowsesource()
{
    CString folderPath;
    IFileOpenDialog* pFileOpen;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFileOpen->GetOptions(&dwOptions);
        pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        pFileOpen->SetTitle(_T("选择源图像文件夹"));

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

        // 只有当成功获取到路径时才设置编辑框
        if (!folderPath.IsEmpty()) {
            m_editSourcePath.SetWindowText(folderPath);
        }
        // 如果用户取消了对话框，直接返回，不显示旧式对话框
        else {
            return;
        }
    }
    else {
        // 如果IFileOpenDialog创建失败，再回退到旧式对话框
        folderPath = GetFolderPath();
        if (!folderPath.IsEmpty()) {
            m_editSourcePath.SetWindowText(folderPath);
        }
    }
}

void CAIImageClassifierDlg::OnBnClickedButtonBrowseoutput()
{
    CString folderPath;
    IFileOpenDialog* pFileOpen;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFileOpen->GetOptions(&dwOptions);
        pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        pFileOpen->SetTitle(_T("选择输出文件夹"));

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

        // 只有当成功获取到路径时才设置编辑框
        if (!folderPath.IsEmpty()) {
            m_editOutputPath.SetWindowText(folderPath);
        }
        // 如果用户取消了对话框，直接返回，不显示旧式对话框
        else {
            return;
        }
    }
    else {
        // 如果IFileOpenDialog创建失败，再回退到旧式对话框
        folderPath = GetFolderPath();
        if (!folderPath.IsEmpty()) {
            m_editOutputPath.SetWindowText(folderPath);
        }
    }
}

void CAIImageClassifierDlg::OnBnClickedButtonBrowsemodel()
{
    CFileDialog dlg(TRUE, _T("onnx"), NULL, OFN_FILEMUSTEXIST,
        _T("ONNX模型文件 (*.onnx)|*.onnx|所有文件 (*.*)|*.*||"));
    if (dlg.DoModal() == IDOK) {
        m_editModelPath.SetWindowText(dlg.GetPathName());
    }
}

void CAIImageClassifierDlg::OnBnClickedButtonLoadmodel()
{
    CString modelPath, labelsPath;
    m_editModelPath.GetWindowText(modelPath);

    if (modelPath.IsEmpty()) {
        AfxMessageBox(_T("请先选择模型文件"));
        return;
    }

    if (!PathExists(modelPath)) {
        AfxMessageBox(_T("模型文件不存在"));
        return;
    }

    int lastSlash = modelPath.ReverseFind('\\');
    if (lastSlash != -1) {
        labelsPath = modelPath.Left(lastSlash) + _T("\\labels.txt");
    }
    else {
        labelsPath = _T("labels.txt");
    }

    if (!PathExists(labelsPath)) {
        CString msg;
        msg.Format(_T("未找到labels.txt文件：\n%s"), labelsPath);
        AfxMessageBox(msg);
        return;
    }

    try {
        std::string modelPathStr = CT2UTF8(modelPath);
        std::string labelsPathStr = CT2UTF8(labelsPath);

        // 关键：这里可以添加一个开关让用户选择是否使用GPU
        bool useGPU = true;  // 改成false测试CPU模式

        m_classifier = std::make_unique<YOLO11Classifier>(
            modelPathStr, labelsPathStr, useGPU);

        LoadClassNames(labelsPath);
        m_btnStart.EnableWindow(TRUE);

        CString msg;
        msg.Format(_T("模型加载成功！\n使用GPU加速: %s\n输入尺寸: %dx%d"),
            useGPU ? _T("是") : _T("否"),
            m_classifier->getInputShape().width,
            m_classifier->getInputShape().height);
        AfxMessageBox(msg);
    }
    catch (const std::exception& e) {
        CString msg;
        msg.Format(_T("模型加载失败：%s"), CA2T(e.what()));
        AfxMessageBox(msg);
    }
}

void CAIImageClassifierDlg::LoadClassNames(const CString& labelsPath)
{
    m_listCategories.DeleteAllItems();
    m_classNames.clear();

    std::wstring w_labelsPath = CStringToWString(labelsPath);
    std::wifstream file(w_labelsPath);

    if (!file) {
        AfxMessageBox(_T("无法打开labels.txt文件"));
        return;
    }

    std::wstring w_line;
    int index = 0;

    while (std::getline(file, w_line)) {
        if (!w_line.empty()) {
            if (w_line.back() == L'\r') {
                w_line.pop_back();
            }

            if (index == 0 && w_line.size() >= 1 && w_line[0] == 0xFEFF) {
                w_line = w_line.substr(1);
            }

            std::string line = CT2UTF8(WStringToCString(w_line));
            m_classNames.push_back(line);
            m_listCategories.InsertItem(index, UTF82CT(line));
            index++;
        }
    }

    // 检查是否包含DG和ZT类别
    m_bMatchConfigEnabled = HasDGAndZTCategories();
    m_btnMatchConfig.EnableWindow(m_bMatchConfigEnabled);
}

void CAIImageClassifierDlg::OnBnClickedButtonStart()
{
    if (m_bIsProcessing) {
        // 如果正在处理，显示停止确认对话框
        if (AfxMessageBox(_T("确定要停止当前处理任务吗？\n\n已处理的文件将会保存，下次可以从断点继续。"),
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
            m_bStopRequested = true;
            m_staticStatus.SetWindowText(_T("正在停止，请稍候..."));
        }
        return;
    }

    // 重置复制计数
    m_nCopyCount = 0;
    UpdateCopyCountDisplay();

    CString sourcePath, outputPath;
    m_editSourcePath.GetWindowText(sourcePath);
    m_editOutputPath.GetWindowText(outputPath);

    if (sourcePath.IsEmpty() || outputPath.IsEmpty()) {
        AfxMessageBox(_T("请选择源文件夹和输出文件夹"));
        return;
    }

    if (!m_classifier) {
        AfxMessageBox(_T("请先加载模型"));
        return;
    }

    if (!PathExists(sourcePath)) {
        AfxMessageBox(_T("源文件夹不存在"));
        return;
    }

    // 检查输出目录权限
    if (!PathExists(outputPath)) {
        if (!CreateDirectoryRecursive(outputPath)) {
            CString errorMsg;
            errorMsg.Format(_T("无法创建输出文件夹，请检查权限: %s"), outputPath);
            AfxMessageBox(errorMsg);
            return;
        }
    }
    else {
        // 检查输出目录是否可写
        CString testFile = outputPath + _T("\\test_write.tmp");
        HANDLE hFile = CreateFile(testFile, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            CString errorMsg;
            errorMsg.Format(_T("输出文件夹不可写，请检查权限: %s"), outputPath);
            AfxMessageBox(errorMsg);
            return;
        }
        CloseHandle(hFile);
        DeleteFile(testFile);
    }

    // 初始化临时目录（用于断点续传）
    if (!InitializeTempDirectory()) {
        AfxMessageBox(_T("无法创建临时目录，断点续传功能将不可用"));
        // 可以选择继续或返回
        if (AfxMessageBox(_T("是否继续处理但不启用断点续传功能？"), MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }

    // 加载已处理的文件列表
    LoadProcessedFiles();

    // 初始化CSV文件
    if (!m_csvManager.InitializeCSVFile(outputPath)) {
        CString errorMsg;
        errorMsg.Format(_T("无法创建结果记录文件。\n请检查以下可能的原因：\n"
            "1. 输出目录权限不足\n"
            "2. 磁盘空间不足\n"
            "3. 文件被其他程序占用\n"
            "输出路径: %s"), outputPath);
        AfxMessageBox(errorMsg);
        if (AfxMessageBox(_T("是否继续处理但不保存详细结果？"), MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }

    // 初始化界面
    m_listResults.DeleteAllItems();
    m_listClassImages.DeleteAllItems();
    m_classifiedImages.clear();
    m_imagePathMap.clear();
    m_imagePathMapW.clear();

    // 设置处理状态
    m_bIsProcessing = true;
    m_bStopRequested = false;  // 重置停止标志
    m_btnStart.SetWindowText(_T("停止"));  // 修改按钮文本
    m_progressCtrl.SetPos(0);

    // 显示断点续传信息
    if (!m_processedFiles.empty()) {
        CString info;
        info.Format(_T("检测到 %d 个已处理文件，将从断点继续..."), m_processedFiles.size());
        m_staticStatus.SetWindowText(info);
    }
    else {
        m_staticStatus.SetWindowText(_T("开始处理图像..."));
    }

    // 启动处理线程
    ThreadParams* params = new ThreadParams{ this, sourcePath, outputPath };
    AfxBeginThread(ProcessingThread, params);
}


UINT CAIImageClassifierDlg::ProcessingThread(LPVOID pParam)
{
    ThreadParams* params = (ThreadParams*)pParam;
    CAIImageClassifierDlg* pDlg = params->pDlg;

    try {
        pDlg->ProcessImages();
    }
    catch (const std::exception& e) {
        CString msg;
        msg.Format(_T("处理过程中发生错误：%s"), CA2T(e.what()));
        AfxMessageBox(msg);
    }

    pDlg->PostMessage(WM_USER + 100, 0, 0);
    delete params;
    return 0;
}


// ProcessImages函数
void CAIImageClassifierDlg::ProcessImages()
{
    CString sourcePath, outputPath;
    m_editSourcePath.GetWindowText(sourcePath);
    m_editOutputPath.GetWindowText(outputPath);

    std::set<std::string> actualCategories;
    std::vector<SampleInfo> samples;
    int skippedCount = 0;
    int processed = 0;
    int totalImages = 0;

    // 修改：不再需要清空 m_detailedResults，因为现在从CSV读取
    // m_detailedResults.clear();

    try {
        std::wstring w_sourcePath = CStringToWString(sourcePath);
        std::wstring w_outputPath = CStringToWString(outputPath);

        if (!fs::exists(fs::path(w_sourcePath))) {
            throw std::runtime_error("源文件夹不存在");
        }

        // 检测目录结构
        bool isBatchMode = false;

        // 检查源目录下是否有直接的图像文件
        bool hasDirectImages = false;
        for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
            if (entry.is_regular_file()) {
                std::wstring w_ext = entry.path().extension().wstring();
                std::transform(w_ext.begin(), w_ext.end(), w_ext.begin(), ::towlower);
                if (w_ext == L".jpg" || w_ext == L".jpeg" || w_ext == L".png" || w_ext == L".bmp") {
                    hasDirectImages = true;
                    break;
                }
            }
        }

        // 检查源目录下是否有子目录
        bool hasSubdirectories = false;
        for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
            if (entry.is_directory()) {
                hasSubdirectories = true;
                break;
            }
        }

        // 分析目录结构
        if (hasDirectImages && !hasSubdirectories) {
            // 单样品模式：源目录直接包含图像文件
            SampleInfo singleSample;
            singleSample.sampleName = L"单样品";
            singleSample.samplePath = w_sourcePath;
            
            // 收集图像文件
            for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
                if (entry.is_regular_file()) {
                    std::wstring w_ext = entry.path().extension().wstring();
                    std::transform(w_ext.begin(), w_ext.end(), w_ext.begin(), ::towlower);
                    if (w_ext == L".jpg" || w_ext == L".jpeg" || w_ext == L".png" || w_ext == L".bmp") {
                        std::wstring w_fullPath = entry.path().wstring();
                        singleSample.imageFiles.push_back(w_fullPath);
                    }
                }
            }
            singleSample.imageCount = singleSample.imageFiles.size();
            samples.push_back(singleSample);
            totalImages = singleSample.imageCount;
            
            // 显示提示
            CString msg;
            msg.Format(_T("检测到单样品模式，包含 %d 张图像。"), singleSample.imageCount);
            AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
        }
        else if (hasSubdirectories) {
            // 批量样品模式：源目录包含多个子目录，每个子目录是一个样品
            isBatchMode = true;
            
            for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
                if (entry.is_directory()) {
                    SampleInfo sample;
                    sample.sampleName = entry.path().filename().wstring();
                    sample.samplePath = entry.path().wstring();
                    
                    // 收集该样品目录下的图像文件
                    for (const auto& imageEntry : fs::directory_iterator(entry.path())) {
                        if (imageEntry.is_regular_file()) {
                            std::wstring w_ext = imageEntry.path().extension().wstring();
                            std::transform(w_ext.begin(), w_ext.end(), w_ext.begin(), ::towlower);
                            if (w_ext == L".jpg" || w_ext == L".jpeg" || w_ext == L".png" || w_ext == L".bmp") {
                                std::wstring w_fullPath = imageEntry.path().wstring();
                                sample.imageFiles.push_back(w_fullPath);
                            }
                        }
                    }
                    sample.imageCount = sample.imageFiles.size();
                    
                    if (sample.imageCount > 0) {
                        samples.push_back(sample);
                        totalImages += sample.imageCount;
                    }
                }
            }
            
            // 显示批量样品信息
            if (!samples.empty()) {
                CString msg;
                msg.Format(_T("检测到批量样品模式，共 %d 个样品：\r\n\r\n"), samples.size());
                
                for (const auto& sample : samples) {
                    CString sampleName = WStringToCString(sample.sampleName);
                    CString sampleInfo;
                    sampleInfo.Format(_T("%s: %d 张图像\r\n"), sampleName, sample.imageCount);
                    msg += sampleInfo;
                }
                
                msg += _T("\r\n总计: %d 张图像");
                msg.Format(msg, totalImages);
                AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
            }
            else {
                // 没有找到有效样品
                UpdateProgress(100, _T("未找到有效样品和图像文件"));
                return;
            }
        }
        else {
            // 既没有直接图像也没有子目录
            UpdateProgress(100, _T("未找到支持的图像文件"));
            return;
        }

        // 检查是否有样品需要处理
        if (samples.empty()) {
            UpdateProgress(100, _T("未找到支持的图像文件"));
            return;
        }

        // 显示处理信息
        CString info;
        info.Format(_T("开始处理 %d 个样品，共 %d 张图像"), samples.size(), totalImages);
        UpdateProgress(0, info);

        // 处理每个样品
        int sampleIndex = 0;
        for (const auto& sample : samples) {
            if (m_bStopRequested) break;

            // 显示当前处理的样品信息
            CString sampleStatus;
            sampleStatus.Format(_T("处理样品 %d/%d: %s"), sampleIndex + 1, samples.size(), WStringToCString(sample.sampleName));
            UpdateProgress((sampleIndex * 100) / samples.size(), sampleStatus);

            // 为每个样品创建输出目录
            CString sampleOutputPath;
            if (isBatchMode) {
                sampleOutputPath = outputPath + _T("\\") + WStringToCString(sample.sampleName);
                CreateDirectoryRecursive(sampleOutputPath);
            }
            else {
                sampleOutputPath = outputPath;
            }

            // 处理样品中的每个图像
            for (const auto& w_imagePath : sample.imageFiles) {
                if (m_bStopRequested) break;

                cv::Mat image;
                try {
                    // 尝试多种方式读取图像
                    image = cv::imread(cv::String(w_imagePath.begin(), w_imagePath.end()));

                    if (image.empty()) {
                        std::ifstream file(w_imagePath, std::ios::binary);
                        if (file) {
                            file.seekg(0, std::ios::end);
                            std::streamsize size = file.tellg();
                            file.seekg(0, std::ios::beg);

                            std::vector<char> buffer(size);
                            if (file.read(buffer.data(), size)) {
                                image = cv::imdecode(cv::Mat(1, size, CV_8UC1, buffer.data()), cv::IMREAD_COLOR);
                            }
                        }
                    }
                }
                catch (...) {
                    image = cv::Mat();
                }

                if (image.empty()) {
                    processed++;
                    continue;
                }

                // 更新预览
                CString imagePath = WStringToCString(w_imagePath);
                CString* pImagePath = new CString(imagePath);
                PostMessage(WM_USER + 103, 0, (LPARAM)pImagePath);

                // 分类预测
                auto start = std::chrono::high_resolution_clock::now();
                ClassificationResult result = m_classifier->classify(image);
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start);

                // 处理分类结果
                if (result.classId != -1 && result.confidence > 0.0f) {
                    std::string className = result.className;

                    // 创建类别目录
                    if (actualCategories.find(className) == actualCategories.end()) {
                        actualCategories.insert(className);
                        CString classFolder = sampleOutputPath + _T("\\") + UTF82CT(className);
                        CreateDirectoryRecursive(classFolder);
                    }

                    // 保存图像
                    std::wstring w_fileName = fs::path(w_imagePath).filename().wstring();
                    CString classFolder = sampleOutputPath + _T("\\") + UTF82CT(className);
                    std::wstring w_outputDir = CStringToWString(classFolder);
                    std::wstring w_outputFile = w_outputDir + L"\\" + w_fileName;

                    try {
                        std::vector<int> compression_params;
                        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
                        compression_params.push_back(95);

                        bool saveResult = cv::imwrite(
                            cv::String(w_outputFile.begin(), w_outputFile.end()),
                            image, compression_params
                        );

                        if (!saveResult) {
                            std::vector<uchar> buffer;
                            if (cv::imencode(".jpg", image, buffer, compression_params)) {
                                std::ofstream file(w_outputFile, std::ios::binary);
                                if (file) {
                                    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
                                }
                            }
                        }
                    }
                    catch (...) {
                        // 保存失败，继续处理
                    }

                    // 复制Diff图像
                    CopyDiffImage(imagePath, sampleOutputPath, isBatchMode, className);
                }

                // 标记文件为已处理（无论成功与否，避免重复处理）
                std::wstring w_fileName = fs::path(w_imagePath).filename().wstring();
                CString fileName = WStringToCString(w_fileName);
                MarkFileAsProcessed(fileName);

                // 更新界面
                processed++;
                int progress = (processed * 100) / totalImages;

                CString status;
                if (m_bStopRequested) {
                    status.Format(_T("正在停止... 已处理: %d/%d (%.1f%%)"), processed, totalImages, (float)progress);
                }
                else {
                    status.Format(_T("处理中: %d/%d (%.1f%%)"), processed, totalImages, (float)progress);
                }
                UpdateProgress(progress, status);

                // 添加到结果列表
                CString strFileName = fileName;
                CString strClassName = UTF82CT(result.className);
                CString strConfidence;
                strConfidence.Format(_T("%.2f%%"), result.confidence * 100);
                CString strTime;
                strTime.Format(_T("%lld ms"), duration.count());

                int nIndex = m_listResults.InsertItem(0, strFileName);
                m_listResults.SetItemText(nIndex, 1, strClassName);
                m_listResults.SetItemText(nIndex, 2, strConfidence);
                m_listResults.SetItemText(nIndex, 3, strTime);

                // 保存结果到CSV文件
                m_csvManager.SaveResultToCSV(strFileName, strClassName, result.confidence, duration.count(), sampleOutputPath);

                // 修改：不再保存到 m_detailedResults，因为现在从CSV读取
                // 只更新内存中的分类图像用于显示统计
                if (result.classId != -1) {
                    m_classifiedImages[result.className].push_back(CT2UTF8(strFileName));
                }
            }

            sampleIndex++;
        }
    }
    catch (const std::exception& e) {
        CString errorMsg;
        errorMsg.Format(_T("处理错误：%s"), CA2T(e.what()));
        UpdateProgress(0, errorMsg);
        return;
    }

    if (m_bStopRequested) {
        int progress = totalImages > 0 ? (processed * 100) / totalImages : 0;
        UpdateProgress(progress, _T("处理已停止"));
    }
    else {
        UpdateProgress(100, _T("处理完成"));
        DisplayStatistics();
    }
}


void CAIImageClassifierDlg::UpdateProgress(int progress, const CString& status)
{
    progress = std::max(0, std::min(100, progress));
    PostMessage(WM_USER + 101, progress, (LPARAM)(new CString(status)));
}

void CAIImageClassifierDlg::DisplayStatistics()
{
    CString stats;
    stats = _T("分类统计结果：\r\n\r\n");

    int totalImages = 0;
    for (const auto& pair : m_classifiedImages) {
        CString line;
        CString categoryName(pair.first.c_str());
        line.Format(_T("%s: %d 张图像\r\n"), categoryName, pair.second.size());
        stats += line;
        totalImages += pair.second.size();
    }

    CString total;
    total.Format(_T("\r\n总计: %d 张图像"), totalImages);
    stats += total;

    m_staticStats.SetWindowText(stats);
}

void CAIImageClassifierDlg::ShowImagePreview(const CString& imagePath)
{
    if (!PathExists(imagePath)) {
        CRect clientRect;
        m_staticPreview.GetClientRect(&clientRect);
        CClientDC dc(&m_staticPreview);
        dc.FillSolidRect(clientRect, RGB(240, 240, 240));
        dc.TextOut(10, 10, _T("图像文件不存在"));
        return;
    }

    cv::Mat image;
    std::wstring w_imagePath = CStringToWString(imagePath);

    try {
        image = cv::imread(cv::String(w_imagePath.begin(), w_imagePath.end()));

        if (image.empty()) {
            std::ifstream file(w_imagePath, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<char> buffer(size);
                if (file.read(buffer.data(), size)) {
                    image = cv::imdecode(cv::Mat(1, size, CV_8UC1, buffer.data()), cv::IMREAD_COLOR);
                }
            }
        }
    }
    catch (...) {
        image = cv::Mat();
    }

    if (image.empty()) {
        CRect clientRect;
        m_staticPreview.GetClientRect(&clientRect);
        CClientDC dc(&m_staticPreview);
        dc.FillSolidRect(clientRect, RGB(240, 240, 240));
        dc.TextOut(10, 10, _T("无法加载图像"));
        return;
    }

    // 图像显示逻辑（保持原有代码）
    CRect rect;
    m_staticPreview.GetClientRect(&rect);

    double scaleX = static_cast<double>(rect.Width()) / image.cols;
    double scaleY = static_cast<double>(rect.Height()) / image.rows;
    double scale = std::min(scaleX, scaleY);

    cv::Size newSize(cvRound(image.cols * scale), cvRound(image.rows * scale));
    cv::Mat resized;
    cv::resize(image, resized, newSize, 0, 0, cv::INTER_AREA);

    cv::Mat rgb;
    if (resized.channels() == 3) {
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    }
    else if (resized.channels() == 4) {
        cv::cvtColor(resized, rgb, cv::COLOR_BGRA2RGB);
    }
    else {
        cv::cvtColor(resized, rgb, cv::COLOR_GRAY2RGB);
    }

    if (!rgb.isContinuous()) {
        rgb = rgb.clone();
    }

    int width = rgb.cols;
    int height = rgb.rows;
    int bytesPerPixel = 3;
    int stride = width * bytesPerPixel;
    int padding = (4 - (stride % 4)) % 4;
    int paddedStride = stride + padding;

    std::vector<uchar> paddedData(height * paddedStride);
    uchar* src = rgb.data;
    uchar* dst = paddedData.data();

    for (int y = 0; y < height; ++y) {
        memcpy(dst, src, stride);
        memset(dst + stride, 0, padding);
        src += stride;
        dst += paddedStride;
    }

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = paddedData.size();

    CClientDC dc(&m_staticPreview);

    CRect clientRect;
    m_staticPreview.GetClientRect(&clientRect);
    dc.FillSolidRect(clientRect, RGB(240, 240, 240));

    int x = (rect.Width() - width) / 2;
    int y = (rect.Height() - height) / 2;

    StretchDIBits(dc.GetSafeHdc(),
        x, y, width, height,
        0, 0, width, height,
        paddedData.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CAIImageClassifierDlg::OnProcessingFinished(WPARAM wParam, LPARAM lParam)
{
    // 恢复按钮状态
    m_btnStart.SetWindowText(_T("开始筛选"));
    m_btnStart.EnableWindow(TRUE);
    m_bIsProcessing = false;

    // 关闭CSV文件
    m_csvManager.CloseCSVFile();

    // 修改：从CSV文件加载所有结果进行分析
    std::map<std::string, std::vector<std::pair<std::string, float>>> detailedResults;
    if (m_csvManager.LoadResultsFromCSV(detailedResults))
    {
        // 设置到阈值分析器中
        m_thresholdAnalyzer.SetClassificationResults(detailedResults);
        m_btnApplyThreshold.EnableWindow(TRUE);

        // 应用默认阈值并更新显示
        ApplyThresholdToResults(0.5f);

        // 确保列表控件刷新
        m_listDistribution.Invalidate();
        m_listDistribution.UpdateWindow();

        // 显示分布统计信息 - 基于完整CSV数据
        CString distInfo;
        distInfo.Format(_T("分析完成，共 %d 个类别，%d 张图像"),
            m_thresholdAnalyzer.GetDistribution().size(),
            m_thresholdAnalyzer.GetTotalImageCount());

        // 只在非停止状态下显示完整提示
        if (!m_bStopRequested) {
            AfxMessageBox(distInfo, MB_OK | MB_ICONINFORMATION);
        }
    }
    else
    {
        // 如果没有数据，清空列表并禁用控件
        m_listDistribution.DeleteAllItems();
        m_staticThresholdStats.SetWindowText(_T("符合阈值图像：0/0"));
        m_btnApplyThreshold.EnableWindow(FALSE);

        CString thresholdText;
        thresholdText.Format(_T("阈值：%.2f"), 0.5f);
        m_staticThresholdValue.SetWindowText(thresholdText);
    }

    // 显示完成提示
    CString message;
    if (m_bStopRequested) {
        message = _T("处理任务已停止。\n\n已处理的文件已保存，下次可以从断点继续。");

        // 显示CSV文件位置（如果存在）
        CString csvFilePath = m_csvManager.GetCSVFilePath();
        if (!csvFilePath.IsEmpty() && PathExists(csvFilePath)) {
            CString csvInfo;
            csvInfo.Format(_T("\n\n结果已保存到：%s"), csvFilePath);
            message += csvInfo;
        }
    }
    else {
        CString csvFilePath = m_csvManager.GetCSVFilePath();
        if (!csvFilePath.IsEmpty() && PathExists(csvFilePath)) {
            message.Format(_T("本次分类任务完成！\n结果已保存到预测表格中。\n\n文件位置：%s"), csvFilePath);
        }
        else {
            message = _T("本次分类任务完成！");
        }

        // 任务完成，清理临时目录
        CleanupTempDirectory();
    }

    AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);

    // 更新状态显示
    if (m_bStopRequested) {
        m_staticStatus.SetWindowText(_T("任务已停止"));
    }
    else {
        m_staticStatus.SetWindowText(_T("任务完成"));

        // 显示最终统计信息
        DisplayStatistics();
    }

    m_bStopRequested = false;  // 重置停止标志

    return 0;
}

// 新增：断点续传相关函数实现
bool CAIImageClassifierDlg::InitializeTempDirectory()
{
    CString outputPath;
    m_editOutputPath.GetWindowText(outputPath);

    if (outputPath.IsEmpty()) {
        return false;
    }

    m_tempDir = outputPath + _T("\\.temp_processing");
    return CreateDirectoryRecursive(m_tempDir);
}

bool CAIImageClassifierDlg::IsFileProcessed(const CString& fileName)
{
    // 首先检查内存中的已处理文件集合
    if (m_processedFiles.find(fileName) != m_processedFiles.end()) {
        return true;
    }

    // 然后检查临时目录中是否存在对应的标记文件
    CString tempFile = m_tempDir + _T("\\") + fileName + _T(".processed");
    return PathExists(tempFile);
}

void CAIImageClassifierDlg::MarkFileAsProcessed(const CString& fileName)
{
    // 添加到内存集合
    m_processedFiles.insert(fileName);

    // 创建标记文件
    CString tempFile = m_tempDir + _T("\\") + fileName + _T(".processed");
    HANDLE hFile = CreateFile(tempFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

void CAIImageClassifierDlg::LoadProcessedFiles()
{
    m_processedFiles.clear();

    if (m_tempDir.IsEmpty() || !PathExists(m_tempDir)) {
        return;
    }

    std::wstring w_tempDir = CStringToWString(m_tempDir);

    try {
        for (const auto& entry : fs::directory_iterator(fs::path(w_tempDir))) {
            if (entry.is_regular_file()) {
                std::wstring w_filename = entry.path().filename().wstring();
                CString filename = WStringToCString(w_filename);

                // 检查是否是标记文件（以.processed结尾）
                if (filename.Right(10) == _T(".processed")) {
                    // 提取原始文件名（去掉.processed后缀）
                    CString originalName = filename.Left(filename.GetLength() - 10);
                    m_processedFiles.insert(originalName);
                }
            }
        }
    }
    catch (...) {
        // 忽略目录访问错误
    }
}

void CAIImageClassifierDlg::CleanupTempDirectory()
{
    if (m_tempDir.IsEmpty() || !PathExists(m_tempDir)) {
        return;
    }

    std::wstring w_tempDir = CStringToWString(m_tempDir);

    try {
        // 删除临时目录中的所有文件
        for (const auto& entry : fs::directory_iterator(fs::path(w_tempDir))) {
            if (entry.is_regular_file()) {
                fs::remove(entry.path());
            }
        }

        // 删除临时目录本身
        fs::remove(fs::path(w_tempDir));
    }
    catch (...) {
        // 忽略清理错误
    }

    m_processedFiles.clear();
}

// 析构函数
CAIImageClassifierDlg::~CAIImageClassifierDlg()
{

    // 如果正在处理，尝试停止
    if (m_bIsProcessing) {
        m_bStopRequested = true;
        // 等待一段时间让线程安全退出
        Sleep(1000);
    }
}






LRESULT CAIImageClassifierDlg::OnUpdateProgress(WPARAM wParam, LPARAM lParam)
{
    int progress = (int)wParam;
    CString* pStatus = (CString*)lParam;
    m_progressCtrl.SetPos(progress);
    m_staticStatus.SetWindowText(*pStatus);
    delete pStatus;
    return 0;
}

LRESULT CAIImageClassifierDlg::OnSetProgressRange(WPARAM wParam, LPARAM lParam)
{
    return 0;
}

LRESULT CAIImageClassifierDlg::OnUpdatePreview(WPARAM wParam, LPARAM lParam)
{
    CString* pImagePath = (CString*)lParam;
    ShowImagePreview(*pImagePath);
    delete pImagePath;
    return 0;
}

void CAIImageClassifierDlg::OnTimer(UINT_PTR nIDEvent)
{
    CDialogEx::OnTimer(nIDEvent);
}

void CAIImageClassifierDlg::OnLvnItemchangedListResults(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED)) {
        CString fileName = m_listResults.GetItemText(pNMLV->iItem, 0);

        auto it = m_imagePathMapW.find(fileName);
        if (it != m_imagePathMapW.end()) {
            CString imagePath = WStringToCString(it->second);
            ShowImagePreview(imagePath);
        }
        else {
            CString sourcePath;
            m_editSourcePath.GetWindowText(sourcePath);
            if (!sourcePath.IsEmpty()) {
                CString possiblePath = sourcePath + _T("\\") + fileName;
                if (PathExists(possiblePath)) {
                    ShowImagePreview(possiblePath);
                }
                else {
                    CRect clientRect;
                    m_staticPreview.GetClientRect(&clientRect);
                    CClientDC dc(&m_staticPreview);
                    dc.FillSolidRect(clientRect, RGB(240, 240, 240));
                    dc.TextOut(10, 10, _T("无法找到图像文件"));
                }
            }
        }
    }

    *pResult = 0;
}

void CAIImageClassifierDlg::OnBnClickedButtonOpenResultDir()
{
    CString outputPath;
    m_editOutputPath.GetWindowText(outputPath);

    if (outputPath.IsEmpty()) {
        AfxMessageBox(_T("请先设置输出路径"));
        return;
    }

    if (!PathExists(outputPath)) {
        CString msg;
        msg.Format(_T("输出路径不存在：\n%s"), outputPath);
        AfxMessageBox(msg);
        return;
    }

    HINSTANCE result = ShellExecute(
        NULL, _T("open"), _T("explorer.exe"), outputPath, NULL, SW_SHOWNORMAL
    );

    if ((INT_PTR)result <= 32) {
        CString cmd;
        cmd.Format(_T("explorer \"%s\""), outputPath);

        int systemResult = _tsystem(cmd);
        if (systemResult != 0) {
            CString errorMsg;
            errorMsg.Format(_T("无法打开输出目录：\n%s\n\n错误代码：%d"), outputPath, (INT_PTR)result);
            AfxMessageBox(errorMsg);
        }
    }
}

void CAIImageClassifierDlg::OnBnClickedButtonViewSamples()
{
    CString sourcePath;
    m_editSourcePath.GetWindowText(sourcePath);

    if (sourcePath.IsEmpty()) {
        AfxMessageBox(_T("请先选择源图像文件夹"));
        return;
    }

    if (!PathExists(sourcePath)) {
        AfxMessageBox(_T("源文件夹不存在"));
        return;
    }

    try {
        std::wstring w_sourcePath = CStringToWString(sourcePath);
        std::vector<SampleInfo> samples;
        int totalImages = 0;

        // 检查源目录下是否有直接的图像文件
        bool hasDirectImages = false;
        for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
            if (entry.is_regular_file()) {
                std::wstring w_ext = entry.path().extension().wstring();
                std::transform(w_ext.begin(), w_ext.end(), w_ext.begin(), ::towlower);
                if (w_ext == L".jpg" || w_ext == L".jpeg" || w_ext == L".png" || w_ext == L".bmp") {
                    hasDirectImages = true;
                    break;
                }
            }
        }

        // 检查源目录下是否有子目录
        bool hasSubdirectories = false;
        for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
            if (entry.is_directory()) {
                hasSubdirectories = true;
                break;
            }
        }

        // 分析目录结构
        if (hasDirectImages && !hasSubdirectories) {
            // 单样品模式：源目录直接包含图像文件
            SampleInfo singleSample;
            singleSample.sampleName = L"单样品";
            singleSample.samplePath = w_sourcePath;
            
            // 收集图像文件
            for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
                if (entry.is_regular_file()) {
                    std::wstring w_ext = entry.path().extension().wstring();
                    std::transform(w_ext.begin(), w_ext.end(), w_ext.begin(), ::towlower);
                    if (w_ext == L".jpg" || w_ext == L".jpeg" || w_ext == L".png" || w_ext == L".bmp") {
                        singleSample.imageFiles.push_back(entry.path().wstring());
                    }
                }
            }
            singleSample.imageCount = singleSample.imageFiles.size();
            samples.push_back(singleSample);
            totalImages = singleSample.imageCount;
            
            // 显示提示
            CString msg;
            msg.Format(_T("检测到单样品模式，包含 %d 张图像。"), singleSample.imageCount);
            AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
        }
        else if (hasSubdirectories) {
            // 批量样品模式：源目录包含多个子目录，每个子目录是一个样品
            for (const auto& entry : fs::directory_iterator(fs::path(w_sourcePath))) {
                if (entry.is_directory()) {
                    SampleInfo sample;
                    sample.sampleName = entry.path().filename().wstring();
                    sample.samplePath = entry.path().wstring();
                    
                    // 收集该样品目录下的图像文件
                    for (const auto& imageEntry : fs::directory_iterator(entry.path())) {
                        if (imageEntry.is_regular_file()) {
                            std::wstring w_ext = imageEntry.path().extension().wstring();
                            std::transform(w_ext.begin(), w_ext.end(), w_ext.begin(), ::towlower);
                            if (w_ext == L".jpg" || w_ext == L".jpeg" || w_ext == L".png" || w_ext == L".bmp") {
                                sample.imageFiles.push_back(imageEntry.path().wstring());
                            }
                        }
                    }
                    sample.imageCount = sample.imageFiles.size();
                    
                    if (sample.imageCount > 0) {
                        samples.push_back(sample);
                        totalImages += sample.imageCount;
                    }
                }
            }
            
            // 显示批量样品信息
            if (!samples.empty()) {
                CString msg;
                msg.Format(_T("检测到批量样品模式，共 %d 个样品：\r\n\r\n"), samples.size());
                
                for (const auto& sample : samples) {
                    CString sampleName = WStringToCString(sample.sampleName);
                    CString sampleInfo;
                    sampleInfo.Format(_T("%s: %d 张图像\r\n"), sampleName, sample.imageCount);
                    msg += sampleInfo;
                }
                
                msg += _T("\r\n总计: %d 张图像");
                msg.Format(msg, totalImages);
                AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
            }
            else {
                // 没有找到有效样品
                AfxMessageBox(_T("未找到有效样品和图像文件"), MB_OK | MB_ICONINFORMATION);
            }
        }
        else {
            // 既没有直接图像也没有子目录
            AfxMessageBox(_T("未找到支持的图像文件"), MB_OK | MB_ICONINFORMATION);
        }
    }
    catch (const std::exception& e) {
        CString errorMsg;
        errorMsg.Format(_T("检查样品时发生错误：%s"), CA2T(e.what()));
        AfxMessageBox(errorMsg, MB_OK | MB_ICONERROR);
    }
}


// 初始化阈值控件
// 初始化阈值控件
void CAIImageClassifierDlg::InitializeThresholdControls()
{
    // 设置滑动条范围 (0-100，对应 0.0-1.0)
    m_sliderThreshold.SetRange(0, 100);
    m_sliderThreshold.SetPos(50); // 默认阈值 0.5
    m_sliderThreshold.SetTicFreq(10);

    // 设置阈值显示
    CString thresholdText;
    thresholdText.Format(_T("阈值：%.2f"), 0.5f);
    m_staticThresholdValue.SetWindowText(thresholdText);

    // 初始化分布列表
    m_listDistribution.DeleteAllItems();
    m_listDistribution.InsertColumn(0, _T("类别名称"), LVCFMT_LEFT, 120);
    m_listDistribution.InsertColumn(1, _T("图像数量"), LVCFMT_LEFT, 80);
    m_listDistribution.InsertColumn(2, _T("百分比"), LVCFMT_LEFT, 80);
    m_listDistribution.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // 初始化统计信息
    m_staticThresholdStats.SetWindowText(_T("符合阈值图像：0/0"));

    // 初始禁用应用按钮
    m_btnApplyThreshold.EnableWindow(FALSE);
}

// 滑动条消息处理
void CAIImageClassifierDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar->GetDlgCtrlID() == IDC_SLIDER_THRESHOLD)
    {
        int pos = m_sliderThreshold.GetPos();
        float threshold = pos / 100.0f;

        // 更新阈值显示
        CString thresholdText;
        thresholdText.Format(_T("阈值：%.2f"), threshold);
        m_staticThresholdValue.SetWindowText(thresholdText);

        // 如果已经有数据，实时更新分布
        if (m_thresholdAnalyzer.GetTotalImageCount() > 0)
        {
            ApplyThresholdToResults(threshold);
        }
    }

    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}
// 应用阈值按钮
void CAIImageClassifierDlg::OnBnClickedButtonApplyThreshold()
{
    int pos = m_sliderThreshold.GetPos();
    float threshold = pos / 100.0f;

    if (m_thresholdAnalyzer.GetTotalImageCount() > 0)
    {
        ApplyThresholdToResults(threshold);

        CString message;
        message.Format(_T("已应用阈值 %.2f，过滤后剩余 %d/%d 张图像"),
            threshold,
            m_thresholdAnalyzer.GetValidImageCount(),
            m_thresholdAnalyzer.GetTotalImageCount());
        AfxMessageBox(message, MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        AfxMessageBox(_T("没有可用的分类数据，请先完成图像分类"), MB_OK | MB_ICONWARNING);
    }
}

// 应用阈值并更新显示
// 应用阈值并更新显示
void CAIImageClassifierDlg::ApplyThresholdToResults(float threshold)
{
    if (m_thresholdAnalyzer.GetTotalImageCount() > 0) {
        m_thresholdAnalyzer.ApplyThreshold(threshold);
        UpdateDistributionList();
        UpdateThresholdStats();
    }
}

// 更新分布列表
// 更新分布列表
void CAIImageClassifierDlg::UpdateDistributionList()
{
    m_listDistribution.DeleteAllItems();

    auto distribution = m_thresholdAnalyzer.GetDistribution();
    int totalValid = m_thresholdAnalyzer.GetValidImageCount();

    int index = 0;
    for (const auto& pair : distribution)
    {
        CString categoryName = UTF82CT(pair.first);
        CString countStr;
        countStr.Format(_T("%d"), pair.second);

        CString percentageStr;
        if (totalValid > 0)
        {
            double percentage = (pair.second * 100.0) / totalValid;
            percentageStr.Format(_T("%.1f%%"), percentage);
        }
        else
        {
            percentageStr = _T("0.0%");
        }

        int nIndex = m_listDistribution.InsertItem(index, categoryName);
        m_listDistribution.SetItemText(nIndex, 1, countStr);
        m_listDistribution.SetItemText(nIndex, 2, percentageStr);
        index++;
    }

    // 确保列宽合适
    m_listDistribution.SetColumnWidth(0, LVSCW_AUTOSIZE);
    m_listDistribution.SetColumnWidth(1, LVSCW_AUTOSIZE);
    m_listDistribution.SetColumnWidth(2, LVSCW_AUTOSIZE);
}


// 更新阈值统计信息
// 更新阈值统计信息
void CAIImageClassifierDlg::UpdateThresholdStats()
{
    CString stats;
    int validCount = m_thresholdAnalyzer.GetValidImageCount();
    int totalCount = m_thresholdAnalyzer.GetTotalImageCount();

    if (totalCount > 0) {
        double percentage = (validCount * 100.0) / totalCount;
        stats.Format(_T("符合阈值图像：%d/%d (%.1f%%)"), validCount, totalCount, percentage);
    }
    else {
        stats = _T("符合阈值图像：0/0 (0.0%)");
    }

    m_staticThresholdStats.SetWindowText(stats);
}

void CAIImageClassifierDlg::OnBnClickedButtonMatchConfig()
{
    CMatchConfigDlg dlg;
    dlg.m_strDiffPath = m_strDiffPath;
    dlg.m_strSavePath = m_strSavePath;

    if (dlg.DoModal() == IDOK) {
        m_strDiffPath = dlg.m_strDiffPath;
        m_strSavePath = dlg.m_strSavePath;
    }
}

bool CAIImageClassifierDlg::HasDGAndZTCategories()
{
    bool hasDG = false;
    bool hasZT = false;

    for (const auto& className : m_classNames) {
        if (className == "DG") {
            hasDG = true;
        }
        if (className == "ZT") {
            hasZT = true;
        }
    }

    return hasDG && hasZT;
}

bool CAIImageClassifierDlg::CopyDiffImage(const CString& sourceImagePath, const CString& sampleOutputPath, bool isBatchMode, const std::string& className)
{
    if (!m_bMatchConfigEnabled || m_strDiffPath.IsEmpty() || m_strSavePath.IsEmpty()) {
        return false;
    }

    if (className != "DG" && className != "ZT") {
        return false;
    }

    try {
        std::wstring w_sourceImagePath = CStringToWString(sourceImagePath);
        std::wstring w_sourceDir;
        std::wstring w_fileName;

        if (isBatchMode) {
            CString sourcePathStr;
            m_editSourcePath.GetWindowText(sourcePathStr);
            std::wstring w_sourcePath = CStringToWString(sourcePathStr);
            fs::path sourcePath(w_sourcePath);
            fs::path imagePath(w_sourceImagePath);

            auto relativePath = fs::relative(imagePath, sourcePath);
            w_fileName = relativePath.filename().wstring();
            w_sourceDir = relativePath.parent_path().wstring();
        } else {
            fs::path imagePath(w_sourceImagePath);
            w_fileName = imagePath.filename().wstring();
            w_sourceDir = L"";
        }

        std::wstring w_diffPath = CStringToWString(m_strDiffPath);
        std::wstring w_savePath = CStringToWString(m_strSavePath);

        fs::path diffFilePath(w_diffPath);
        if (!w_sourceDir.empty()) {
            diffFilePath /= w_sourceDir;
        }
        diffFilePath /= w_fileName;

        if (!fs::exists(diffFilePath)) {
            return false;
        }

        fs::path saveDirPath(w_savePath);
        if (!w_sourceDir.empty()) {
            saveDirPath /= w_sourceDir;
        }
        CString classNameCStr = UTF82CT(className);
        std::wstring w_className = CStringToWString(classNameCStr);
        saveDirPath /= w_className;

        if (!fs::exists(saveDirPath)) {
            fs::create_directories(saveDirPath);
        }

        fs::path saveFilePath = saveDirPath / w_fileName;
        fs::copy_file(diffFilePath, saveFilePath, fs::copy_options::overwrite_existing);

        m_nCopyCount++;
        UpdateCopyCountDisplay();

        return true;
    }
    catch (...) {
        return false;
    }
}

void CAIImageClassifierDlg::UpdateCopyCountDisplay()
{
    CString str;
    str.Format(_T("已复制: %d"), m_nCopyCount);
    m_staticCopyCount.SetWindowText(str);
}