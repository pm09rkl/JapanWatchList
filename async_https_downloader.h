#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <string>
#include <memory>
#include <future>

namespace watchList
{
    class CAsyncHttpsDownloader
    {   
    public:
        typedef boost::beast::http::response<boost::beast::http::dynamic_body> ResponseType;
        typedef std::future<ResponseType> FutureResponseType;
        
    public:
        CAsyncHttpsDownloader(boost::asio::io_context& ioContext, std::string_view host, std::string_view port, int version = 11);
    
    public:
        FutureResponseType asyncDownload(std::string_view target);
        
    private:
        class CAsyncTask : public std::enable_shared_from_this<CAsyncTask>
        {
        public:
            typedef std::shared_ptr<CAsyncTask> SharedPtr;
            
        public:
            CAsyncTask(CAsyncHttpsDownloader* ptrDownloader, std::string_view target);
            
            static SharedPtr create(CAsyncHttpsDownloader* ptrDownloader, std::string_view target);
            
        public:
            void run(const boost::asio::ip::tcp::resolver::results_type& resolveResults);
            
            std::promise<ResponseType>& getPromise()
                { return _promise; }
                
        private:
            typedef boost::beast::http::request<boost::beast::http::string_body> RequestType;

        private:           
            void onConnect(const boost::system::error_code& errorCode);
            void onHandshake(const boost::system::error_code& errorCode);
            void onWrite(const boost::system::error_code& errorCode);
            void onRead(const boost::system::error_code& errorCode);
            
            bool validateErrorCode(const boost::system::error_code& errorCode);

        private:
            boost::beast::flat_buffer _buffer;
            RequestType _request;
            ResponseType _response;
            std::promise<ResponseType> _promise;
            boost::beast::ssl_stream<boost::beast::tcp_stream> _sslStream;
        };

    private:
        boost::asio::io_context& _ioContext;
        boost::asio::ssl::context _sslContext;
        boost::asio::ip::tcp::resolver::results_type _resolveResults;
        std::string _host;
        std::string _port;
        int _version;
    };
}
