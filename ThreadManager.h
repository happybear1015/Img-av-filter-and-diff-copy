// ThreadManager.h
#pragma once
#include "ImageClassifier.h"
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>

struct ProcessingStatistics {
    int totalFiles;
    int processedFiles;
    int skippedFiles;
    int errorFiles;
    double totalTime;
    double averageTime;
    std::map<std::string, int> categoryCounts;
};

class ThreadManager {
public:
    using ProgressCallback = std::function<void(int, const CString&)>;
    using CompletionCallback = std::function<void(bool, const CString&)>;
    using ErrorCallback = std::function<void(const CString&)>;
    using PreviewCallback = std::function<void(const CString&)>;

    ThreadManager();
    ~ThreadManager();

    bool StartProcessing(const CString& sourcePath, const CString& outputPath,
        std::unique_ptr<ImageClassifier> classifier);
    void StopProcessing(bool forceStop);
    bool IsProcessing() const;
    ProcessingStatistics GetStatistics() const;

    void SetProgressCallback(ProgressCallback callback);
    void SetCompletionCallback(CompletionCallback callback);
    void SetErrorCallback(ErrorCallback callback);
    void SetPreviewCallback(PreviewCallback callback);
    void SetBatchSize(int size);
    void SetMaxThreads(int threads);
    void EnableResume(bool enable);

private:
    void ProcessingThread();
    void ProcessImageBatch(const std::vector<CString>& imageBatch);
    bool ShouldStop() const;
    void UpdateProgress(int current, int total, const CString& status);
    void NotifyCompletion(bool success, const CString& message);

    std::unique_ptr<ImageClassifier> m_classifier;
    std::vector<std::thread> m_workerThreads;
    std::atomic<bool> m_bStopRequested;
    std::atomic<bool> m_bProcessing;

    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    ErrorCallback m_errorCallback;
    PreviewCallback m_previewCallback;

    mutable std::mutex m_statsMutex;
    ProcessingStatistics m_stats;
    int m_batchSize;
    int m_maxThreads;
    bool m_enableResume;
};