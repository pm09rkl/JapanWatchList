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
    
    static std::shared_ptr<CImpl> create();
    
public:
    FutureResponseType addDownload(std::string_view link);
    void start();
    void setDownloadDir(std::string_view dir);    

private:
    void setDownloadDestination(WebKitDownload* download, std::string_view fileName) const;
    void processDownloadTask();
    void completeDownloadTask(WebKitDownload* download, GError* error = nullptr);
    void startDownloading(GtkApplication* app);
    
    int getNumStepsBeforeHugeBreak();
    int getSmallBreakTime();
    int getHugeBreakTime();

private:
    static void processDownloadTask(gpointer userData);
    static void onActivateApp(GtkApplication* app, gpointer userData);
    static gboolean onDecideDestination(WebKitDownload* download, gchar* suggestedFilename, gpointer userData);    
    static void onDownloadFinished(WebKitDownload* download, gpointer userData);
    static void onDownloadFailed(WebKitDownload* download, GError* error, gpointer userData);
    
private:
    struct DownloadTask
    {
        typedef std::list<DownloadTask> List;
        
        DownloadTask(std::string_view link);
        
        std::string link;
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
};

CWebEngineDownloader::CImpl::DownloadTask::DownloadTask(std::string_view link)
    : link(link)
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
}

CWebEngineDownloader::FutureResponseType CWebEngineDownloader::CImpl::addDownload(std::string_view link)
{
    _tasks.emplace(_tasks.end(), link);
    return _tasks.back().responsePath.get_future();
}

void CWebEngineDownloader::CImpl::start()
{
    if (!_tasks.empty())
    {
        _app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
        g_signal_connect(_app, "activate", G_CALLBACK(onActivateApp), this);  
        g_application_run(G_APPLICATION(_app), 0, nullptr);
        g_object_unref(_app);
        _app = nullptr;
    }
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

void CWebEngineDownloader::CImpl::setDownloadDestination(WebKitDownload* download, std::string_view fileName) const
{
    auto it = _tasksInProcess.find(download);
    if (it != _tasksInProcess.end())
    {
        auto taskIt = it->second;
        taskIt->targetResponsePath.clear();
        taskIt->targetResponsePath.append(_downloadDir).append(fileName);
        webkit_download_set_destination(download, taskIt->targetResponsePath.c_str());
    }
}

void CWebEngineDownloader::CImpl::setDownloadDir(std::string_view dir)
{
    // gtk accepts only absolut path
    _downloadDir = std::filesystem::canonical(dir).string();
    if (!_downloadDir.empty() && (_downloadDir.back() != '/'))
    {
        _downloadDir.push_back('/');
    }
}

void CWebEngineDownloader::CImpl::processDownloadTask()
{
    if (_nextTask != _tasks.end())
    {
        WebKitDownload* download = webkit_web_view_download_uri(WEBKIT_WEB_VIEW(_webView), _nextTask->link.c_str());
        g_signal_connect(download, "failed", G_CALLBACK(onDownloadFailed), this);
        g_signal_connect(download, "finished", G_CALLBACK(onDownloadFinished), this);
        if (!_downloadDir.empty())
        {
            g_signal_connect(download, "decide-destination", G_CALLBACK(onDecideDestination), this);
        }

        --_numStepsUntilHugeBreak;
        if (_numStepsUntilHugeBreak <= 0)
        {
            _numStepsUntilHugeBreak = getNumStepsBeforeHugeBreak();
            g_timeout_add_once(getHugeBreakTime(), processDownloadTask, this);
        }
        else
        {
            g_timeout_add_once(getSmallBreakTime(), processDownloadTask, this);
        }

        _tasksInProcess[download] = _nextTask;        
        ++_nextTask;
    }
}

void CWebEngineDownloader::CImpl::completeDownloadTask(WebKitDownload* download, GError* error)
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
        _tasks.erase(taskIt);
        if (_tasks.empty() && _tasksInProcess.empty())
        {
            g_application_quit(G_APPLICATION(_app));
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
    g_timeout_add_once(getSmallBreakTime(), processDownloadTask, this);
}

void CWebEngineDownloader::CImpl::onDownloadFinished(WebKitDownload* download, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->completeDownloadTask(download);
}

void CWebEngineDownloader::CImpl::onDownloadFailed(WebKitDownload* download, GError* error, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->completeDownloadTask(download, error);    
}

gboolean CWebEngineDownloader::CImpl::onDecideDestination(WebKitDownload* download, gchar* suggestedFilename, gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    ptrThis->setDownloadDestination(download, suggestedFilename);    
    return true;
}

void CWebEngineDownloader::CImpl::processDownloadTask(gpointer userData)
{
    CImpl* ptrThis = static_cast<CImpl*>(userData);
    return ptrThis->processDownloadTask();
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

CWebEngineDownloader::CWebEngineDownloader()
    : _pImpl(CImpl::create()) 
{
}

CWebEngineDownloader::~CWebEngineDownloader()
{
    if (_thread.joinable())
    {
        try 
        {
            _thread.detach();
        }
        catch (...)
        {
        }
    }
}

void CWebEngineDownloader::waitForCompletion()
{
    // do not allow operations until start method stop working
    if (_thread.joinable())
    {
        _thread.join();
    }    
}

CWebEngineDownloader::FutureResponseType CWebEngineDownloader::addDownload(std::string_view link)
{
    waitForCompletion();
    return _pImpl->addDownload(link);
}

void CWebEngineDownloader::start()
{
    waitForCompletion();
    _thread = std::thread([pImpl = _pImpl](){ pImpl->start(); });
}

void CWebEngineDownloader::setDownloadDir(std::string_view dir)
{
    waitForCompletion();
    _pImpl->setDownloadDir(dir);
}
}
