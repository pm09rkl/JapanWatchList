#include <boost/beast/core/buffers_to_string.hpp>
#include "html_parser.h"

using namespace boost;

namespace watchList
{
CHtmlParser::CHtmlParser()
{  
}

CHtmlParser::CHtmlParser(std::string_view content)
    : _content(content)
{
}

bool CHtmlParser::hasContent() const
{
    return !_content.empty();
}

std::string_view CHtmlParser::getContent() const
{
    return _content;
}
    
void CHtmlParser::skipBeginning(std::string_view endOfBeginning)
{
    std::string_view content;
    std::size_t pos = _content.find(endOfBeginning);
    if (pos != std::string::npos)
    {
        pos += endOfBeginning.size();
        if (pos < _content.size())
        {
            content = _content.substr(pos);
        }
    }
    _content = content;
}

void CHtmlParser::skipEnding(std::string_view startOfEnding)
{
    std::string_view content;
    std::size_t pos = _content.find(startOfEnding);
    if (pos != std::string::npos)
    {
        content = _content.substr(0, pos);
    }
    _content = content;
}

std::string_view CHtmlParser::getAttributeValue(std::string_view name) const
{
    std::string_view value;
    std::size_t pos = _content.find(name);
    if (pos != std::string::npos)
    {
        pos += name.size();
        std::size_t openQuotePos = _content.find('\"', pos);
        if (openQuotePos != std::string::npos)
        {
            std::size_t valuePos = openQuotePos + 1;
            std::size_t closeQuotePos = _content.find('\"', valuePos);
            std::size_t valueSize = (closeQuotePos == std::string::npos) ? std::string::npos : (closeQuotePos - valuePos); 
            value = _content.substr(valuePos, valueSize);
        }
    }
    return value;
}    
    
CHtmlContent::CHtmlContent(const CAsyncHttpsDownloader::ResponseType& response)
    : _htmlContent(buffers_to_string(response.body().data()))
{
}

CHtmlContent::CHtmlContent(const std::stringstream& response)
    : _htmlContent(response.str())
{
}

CHtmlParser CHtmlContent::createParser() const
{
    return CHtmlParser(_htmlContent);
}

}
