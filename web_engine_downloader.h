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
    FutureResponseType addDownload(std::string_view link, std::string_view responseName = std::string_view());
    void setDownloadDir(std::string_view dir);
    
private:
    void checkDownloader();
    
private:
    class CImpl;
    
private:
    // WebKitWebView allow creation only from one thread
    static std::shared_ptr<CImpl> _pImpl;
};
}
