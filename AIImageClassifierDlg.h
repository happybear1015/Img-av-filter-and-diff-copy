// AIImageClassifierDlg.h
#pragma once
#include "YOLO11CLASS.hpp"
#include "ThresholdAnalysis.h"
#include "CSVManager.h"
#include <memory>
#include <map>
#include <vector>
#include <set>
#include <fstream>

class CAIImageClassifierDlg : public CDialogEx
{
public:
    CAIImageClassifierDlg(CWnd* pParent = nullptr);

    enum { IDD = IDD_AIIMAGECLASSIFIER_DIALOG };
    virtual ~CAIImageClassifierDlg();

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedButtonBrowsesource();
    afx_msg void OnBnClickedButtonBrowseoutput();
    afx_msg void OnBnClickedButtonBrowsemodel();
    afx_msg void OnBnClickedButtonLoadmodel();
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnBnClickedButtonOpenResultDir();
    afx_msg void OnBnClickedButtonViewSamples();
    afx_msg void OnLvnItemchangedListResults(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedButtonMatchConfig();

    // 自定义消息处理函数
    afx_msg LRESULT OnProcessingFinished(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSetProgressRange(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUpdatePreview(WPARAM wParam, LPARAM lParam);

    // 消息处理函数
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBnClickedButtonApplyThreshold();

private:
    // 控件变量
    CEdit m_editSourcePath;
    CEdit m_editOutputPath;
    CEdit m_editModelPath;
    CButton m_btnBrowseSource;
    CButton m_btnBrowseOutput;
    CButton m_btnBrowseModel;
    CButton m_btnLoadModel;
    CButton m_btnStart;
    CButton m_btnOpenResultDir;
    CButton m_btnViewSamples;
    CListCtrl m_listCategories;
    CListCtrl m_listResults;
    CListCtrl m_listClassImages;
    CProgressCtrl m_progressCtrl;
    CStatic m_staticPreview;
    CStatic m_staticStatus;
    CStatic m_staticStats;
    // 阈值分析相关
    CThresholdAnalysis m_thresholdAnalyzer;
    CSliderCtrl m_sliderThreshold;
    CStatic m_staticThresholdValue;
    CListCtrl m_listDistribution;
    CButton m_btnApplyThreshold;
    CStatic m_staticThresholdStats;

    // 匹配配置相关控件
    CButton m_btnMatchConfig;
    CStatic m_staticCopyCount;

    // 数据处理变量
    std::unique_ptr<YOLO11Classifier> m_classifier;
    std::vector<std::string> m_classNames;
    std::map<std::string, std::vector<std::string>> m_classifiedImages;
    std::map<std::string, std::string> m_imagePathMap;
    std::map<CString, std::wstring> m_imagePathMapW;
    bool m_bIsProcessing;
    bool m_bStopRequested;  // 新增：停止请求标志


    // 存储每个图像的置信度信息
    std::map<std::string, std::vector<std::pair<std::string, float>>> m_detailedResults;

    // 添加CSV管理器
    CCSVManager m_csvManager;

    // 新增：断点续传相关
    CString m_tempDir;  // 临时目录路径
    std::set<CString> m_processedFiles;  // 已处理的文件集合

    // 字符编码转换函数
    std::string CT2UTF8(const CString& str);
    CString UTF82CT(const std::string& utf8Str);
    std::wstring CStringToWString(const CString& str);
    CString WStringToCString(const std::wstring& wstr);

    // 文件路径处理函数
    bool CreateDirectoryRecursive(const CString& path);
    bool PathExists(const CString& path);
    CString GetFolderPath();

    // 图像处理相关函数
    void LoadClassNames(const CString& labelsPath);
    void ProcessImages();
    void UpdateProgress(int progress, const CString& status);
    void DisplayStatistics();
    void ShowImagePreview(const CString& imagePath);

    // 阈值相关函数
    void InitializeThresholdControls();
    void UpdateDistributionList();
    void UpdateThresholdStats();
    void ApplyThresholdToResults(float threshold);


    // 新增：断点续传相关函数
    bool InitializeTempDirectory();
    bool IsFileProcessed(const CString& fileName);
    void MarkFileAsProcessed(const CString& fileName);
    void LoadProcessedFiles();
    void CleanupTempDirectory();

    // 线程处理函数
    static UINT ProcessingThread(LPVOID pParam);


    // 匹配配置相关变量
    CString m_strDiffPath;
    CString m_strSavePath;
    int m_nCopyCount;
    bool m_bMatchConfigEnabled;

    // 检查是否包含DG和ZT类别
    bool HasDGAndZTCategories();
    // 复制Diff图像函数
    bool CopyDiffImage(const CString& sourceImagePath, const CString& sampleOutputPath, bool isBatchMode, const std::string& className);
    // 更新复制计数显示
    void UpdateCopyCountDisplay();

    // 线程参数结构体
    struct ThreadParams {
        CAIImageClassifierDlg* pDlg;
        CString sourcePath;
        CString outputPath;
    };
};