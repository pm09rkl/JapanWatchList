#pragma once

#include "async_https_downloader.h"
#include <string>
#include <vector>
#include <sstream>

namespace watchList
{
    class CHtmlParser
    {
    public:
        CHtmlParser();
        CHtmlParser(std::string_view content);
        
    public:
        bool hasContent() const;
        std::string_view getContent() const;
            
        std::string_view getAttributeValue(std::string_view name) const;            
        
        void skipBeginning(std::string_view endOfBeginning);
        void skipEnding(std::string_view startOfEnding);

    private:
        std::string_view _content;
    };
    
    class CHtmlContent
    {
    public:
        CHtmlContent(const CAsyncHttpsDownloader::ResponseType& response);
        CHtmlContent(const std::stringstream& response);
        
    public:
        CHtmlParser createParser() const;
        
    private:
        std::string _htmlContent;
    }; 
}

