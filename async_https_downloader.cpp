#include <boost/asio/placeholders.hpp>
#include <boost/bind/bind.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include "yahoo_url_factory.h"
#include "async_https_downloader.h"

using namespace boost::asio;
using namespace boost::beast;
using namespace boost::system;

namespace watchList
{
CAsyncHttpsDownloader::CAsyncTask::CAsyncTask(CAsyncHttpsDownloader* ptrDownloader, std::string_view target)
    : _request(http::verb::get, target, ptrDownloader->_version)
    , _sslStream(ptrDownloader->_ioContext, ptrDownloader->_sslContext)
{
    _request.set(http::field::host, ptrDownloader->_host);
    _request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
}

void CAsyncHttpsDownloader::CAsyncTask::onConnect(const error_code& errorCode)
{
    if (validateErrorCode(errorCode))
    {
        _sslStream.async_handshake(ssl::stream_base::client,
            [this, ptrThis = shared_from_this()](const error_code& errorCode)
            {
                onHandshake(errorCode);
            });
    }
}

void CAsyncHttpsDownloader::CAsyncTask::onHandshake(const error_code& errorCode)
{
    if (validateErrorCode(errorCode))
    {       
        http::async_write(_sslStream, _request,
            [this, ptrThis = shared_from_this()](const error_code& errorCode, std::size_t)
            {
                onWrite(errorCode);
            });
    }
}

void CAsyncHttpsDownloader::CAsyncTask::onWrite(const error_code& errorCode)
{
    if (validateErrorCode(errorCode))
    {
        http::async_read(_sslStream, _buffer, _response,
            [this, ptrThis = shared_from_this()](const error_code& errorCode, std::size_t)
            {
                onRead(errorCode);
            });
    }
}

void CAsyncHttpsDownloader::CAsyncTask::onRead(const error_code& errorCode)
{
    if (validateErrorCode(errorCode))
    {
        _promise.set_value(std::move(_response));
    }
}

bool CAsyncHttpsDownloader::CAsyncTask::validateErrorCode(const error_code& errorCode)
{
    if (errorCode)
    {
        try
        {
            _promise.set_exception(std::make_exception_ptr(system_error(errorCode)));
        }
        catch (...)
        {
        }
        return false;
    }
    return true;
}

void CAsyncHttpsDownloader::CAsyncTask::run(const ip::tcp::resolver::results_type& resolveResults)
{
    get_lowest_layer(_sslStream).async_connect(resolveResults, 
        [this, ptrThis = shared_from_this()](const error_code& errorCode, const ip::tcp::endpoint&)
        {
            onConnect(errorCode);
        });
}

CAsyncHttpsDownloader::CAsyncTask::SharedPtr CAsyncHttpsDownloader::CAsyncTask::create(CAsyncHttpsDownloader* ptrDownloader, std::string_view target)
{
    return std::make_shared<CAsyncHttpsDownloader::CAsyncTask>(ptrDownloader, target);
}

CAsyncHttpsDownloader::CAsyncHttpsDownloader(net::io_context& ioContext, std::string_view host, std::string_view port, int version)
    : _ioContext(ioContext)
    , _sslContext(ssl::context::tlsv12_client)
    , _host(host)
    , _port(port)
    , _version(version)
{
    ip::tcp::resolver resolver(_ioContext);
    _resolveResults = resolver.resolve(host, port);
}

CAsyncHttpsDownloader::FutureResponseType CAsyncHttpsDownloader::asyncDownload(std::string_view target)
{
    CAsyncTask::SharedPtr ptrTask = CAsyncTask::create(this, target);
    ptrTask->run(_resolveResults);
    return ptrTask->getPromise().get_future();
}   
}
