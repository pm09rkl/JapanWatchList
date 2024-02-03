#pragma once

#include <thread>
#include <future>
#include <memory>
#include <string>

namespace watchList
{
class CWebEngineDownloader
{
public:
    typedef std::string ResponseType;
    typedef std::future<ResponseType> FutureResponseType;
    
public:  
    CWebEngineDownloader();
    ~CWebEngineDownloader();
    
public:
    FutureResponseType addDownload(std::string_view link, std::string_view responseName = std::string_view());
    void start();
    void setDownloadDir(std::string_view dir);
    
private:
    void waitForCompletion();
    
private:
    class CImpl;
    
private:
    std::thread _thread;
    std::shared_ptr<CImpl> _pImpl;
};
}
