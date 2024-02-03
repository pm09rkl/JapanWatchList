#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <list>
#include <map>
#include <random>
#include <filesystem>
#include "web_engine_downloader.h"

namespace watchList
{
class CWebEngineDownloader::CImpl
{
public:
    CImpl();
    ~CImpl();
    
    static std::shared_ptr<CImpl> create();
    
public:
    FutureResponseType addDownload(std::string_view link, std::string_view responseName);
    void start();
    void setDownloadDir(std::string_view dir);    

private:
    void setDownloadDestination(WebKitDownload* download, std::string_view fileName);
    void startTask();
    void processTask();
    bool hasTask();
    void waitForTask();
    void completeTask(WebKitDownload* download, GError* error = nullptr);    
    void startDownloading(GtkApplication* app);
    
    int getNumStepsBeforeHugeBreak();
    int getSmallBreakTime();
    int getHugeBreakTime();

private:
    static void processTask(gpointer userData);
    static gboolean waitForTask(gpointer userData);
    static void onActivateApp(GtkApplication* app, gpointer userData);
    static gboolean onDecideDestination(WebKitDownload* download, gchar* suggestedFilename, gpointer userData);    
    static void onDownloadFinished(WebKitDownload* download, gpointer userData);
    static void onDownloadFailed(WebKitDownload* download, GError* error, gpointer userData);
    
private:
    struct DownloadTask
    {
        typedef std::list<DownloadTask> List;
        
        DownloadTask(std::string_view link, std::string_view responseName);
        
        std::string link;
        std::string responseName;
        std::string targetResponsePath;
        std::promise<std::string> responsePath;
    };
    
    typedef std::map<WebKitDownload*, DownloadTask::List::iterator> DownloadTasksInProcess;
    
private:
    std::default_random_engine _randomEngine;
    std::uniform_int_distribution<int> _smallBreakDist;
    std::uniform_int_distribution<int> _hugeBreakDist;
    std::uniform_int_distribution<int> _numStepsUntilHugeBreakDist;
    
    std::string _downloadDir;
    GtkApplication* _app;
    GtkWidget* _webView;
    DownloadTask::List _tasks;
    DownloadTask::List::iterator _nextTask;
    DownloadTasksInProcess _tasksInProcess;
    int _numStepsUntilHugeBreak;

    std::mutex _tasksMutex;
    std::mutex _downloadDirMutex;
    std::thread _thread;    
};

CWebEngineDownloader::CImpl::DownloadTask::DownloadTask(std::string_view link, std::string_view responseName)
    : link(link)
    , responseName(responseName)
{
}

CWebEngineDownloader::CImpl::CImpl()
    : _smallBreakDist(200, 400)
    , _hugeBreakDist(3000, 5000)
    , _numStepsUntilHugeBreakDist(10, 50)
    , _app(nullptr)
    , _webView(nullptr)
    , _nextTask(_tasks.end())
    , _numStepsUntilHugeBreak(0)
{
    _thread = std::thread([this]() { start(); });
}

CWebEngineDownloader::CImpl::~CImpl()
{
    _thread.detach();
}

CWebEngineDownloader::FutureResponseType CWebEngineDownloader::CImpl::addDownload(std::string_view link, std::string_view responseName)
{
    std::unique_lock lock(_tasksMutex);
    _tasks.emplace(_tasks.end(), link, responseName);
    return _tasks.back().responsePath.get_future();
}

void CWebEngineDownloader::CImpl::start()
{
    _app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(_app, "activate", G_CALLBACK(onActivateApp), this);  
    g_application_run(G_APPLICATION(_app), 0, nullptr);
    g_clear_object(&_app);
}

int CWebEngineDownloader::CImpl::getNumStepsBeforeHugeBreak()
{
    return _numStepsUntilHugeBreakDist(_randomEngine);
}

int CWebEngineDownloader::CImpl::getSmallBreakTime()
{  
    return _smallBreakDist(_randomEngine);
}

int CWebEngineDownloader::CImpl::getHugeBreakTime()
{
    return _hugeBreakDist(_randomEngine);
}

void CWebEngineDownloader::CImpl::setDownloadDestination(WebKitDownload* download, std::string_view fileName)
{
    auto it = _tasksInProcess.find(download);
    if (it != _tasksInProcess.end())
    {
        auto taskIt = it->second;
        taskIt->targetResponsePath.clear();
        {
            std::unique_lock lock(_downloadDirMutex);
            taskIt->targetResponsePath.append(_downloadDir);
        }
        taskIt->targetResponsePath.append(taskIt->responseName.empty() ? fileName : taskIt->responseName);
        webkit_download_set_destination(download, taskIt->targetResponsePath.c_str());
    }
}

void CWebEngineDownloader::CImpl::setDownloadDir(std::string_view dir)
{
    // gtk accepts only absolut path
    std::unique_lock lock(_downloadDirMutex);
    _downloadDir = std::filesystem::canonical(dir).string();
    if (!_downloadDir.empty() && (_downloadDir.back() != '/'))
    {
        _downloadDir.push_back('/');
    }
}

void CWebEngineDownloader::CImpl::startTask()
{
    if (_numStepsUntilHugeBreak <= 0)
    {
        _numStepsUntilHugeBreak = getNumStepsBeforeHugeBreak();
        g_timeout_add_once(getHugeBreakTime(), processTask, this);
    }
    else
    {
        g_timeout_add_once(getSmallBreakTime(), processTask, this);
    }
}

void CWebEngineDownloader::CImpl::processTask()
{
    WebKitDownload* download = webkit_web_view_download_uri(WEBKIT_WEB_VIEW(_webView), _nextTask->link.c_str());
    g_signal_connect(download, "failed", G_CALLBACK(onDownloadFailed), this);
    g_signal_connect(download, "finished", G_CALLBACK(onDownloadFinished), this);
    if (!_downloadDir.empty())
    {
        g_signal_connect(download, "decide-destination", G_CALLBACK(onDecideDestination), this);
    }

    --_numStepsUntilHugeBreak;
    _tasksInProcess[download] = _nextTask;
    
    bool hasOtherTasks = false;
    {
        std::unique_lock lock(_tasksMutex);
        hasOtherTasks = (++_nextTask != _tasks.end());
    }
    if (hasOtherTasks)
    {
        startTask();    
    }
}

bool CWebEngineDownloader::CImpl::hasTask()
{
    {
        std::unique_lock lock(_tasksMutex);
        if (_tasks.empty())
        {
            return false;
        }
        _nextTask = _tasks.begin();
    }
    startTask();
    return true;
}

void CWebEngineDownloader::CImpl::waitForTask()
{
    g_timeout_add_seconds(5, waitForTask, this);
}

void CWebEngineDownloader::CImpl::completeTask(WebKitDownload* download, GError* error)
{
    auto it = _tasksInProcess.find(download);
    if (it != _tasksInProcess.end())
    {
        auto taskIt = it->second;
        if (error != nullptr)
        {
            std::string message = "Error downloading " + taskIt->link + " : " + error->message;
            taskIt->responsePath.set_exception(std::make_exception_ptr(std::runtime_error(message)));
        }
        else
        {
            const gchar* dest = webkit_download_get_destination(download);
            taskIt->responsePath.set_value(dest);
        }
        _tasksInProcess.erase(it);
        bool hasNoMoreTasks = false;
        {
            std::unique_lock lock(_tasksMutex);
            _tasks.erase(taskIt);
            hasNoMoreTasks = _tasks.empty();
        }

        if (hasNoMoreTasks && _tasksInProcess.empty())
        {
            waitForTask();
        }
    }
}

void CWebEngineDownloader::CImpl::startDownloading(GtkApplication* app)
{
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "DummyWindow");
    
    _webView = webkit_web_view_new();
    gtk_window_set_child(GTK_WINDOW(window), _webView);
    
    _numStepsUntilHugeBreak = getNumStepsBeforeHugeBreak();
    _nextTask = _tasks.begin();
    waitForTask();
}

void CWebEngineDownloader::CImpl::onDownloadFinished(WebKitDownload* download, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->completeTask(download);
}

void CWebEngineDownloader::CImpl::onDownloadFailed(WebKitDownload* download, GError* error, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->completeTask(download, error);    
}

gboolean CWebEngineDownloader::CImpl::onDecideDestination(WebKitDownload* download, gchar* suggestedFilename, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    ptrThis->setDownloadDestination(download, suggestedFilename);    
    return true;
}

void CWebEngineDownloader::CImpl::processTask(gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->processTask();
}

gboolean CWebEngineDownloader::CImpl::waitForTask(gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->hasTask() ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
}

void CWebEngineDownloader::CImpl::onActivateApp(GtkApplication* app, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->startDownloading(app);
}

std::shared_ptr<CWebEngineDownloader::CImpl> CWebEngineDownloader::CImpl::create()
{
    return std::make_shared<CImpl>();
}

std::shared_ptr<CWebEngineDownloader::CImpl> CWebEngineDownloader::_pImpl;

void CWebEngineDownloader::checkDownloader()
{
    if (_pImpl == nullptr)
    {
        _pImpl = CImpl::create();
    }    
}

CWebEngineDownloader::FutureResponseType CWebEngineDownloader::addDownload(std::string_view link, std::string_view responseName)
{
    checkDownloader();
    return _pImpl->addDownload(link, responseName);
}

void CWebEngineDownloader::setDownloadDir(std::string_view dir)
{
    checkDownloader();
    _pImpl->setDownloadDir(dir);
}
}
