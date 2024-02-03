#pragma once

#include "html_parser.h"
#include "yahoo_search_query.h"
#include <string>
#include <vector>

namespace watchList
{
    class CYahooAuctionInfo
    {
        friend class CYahooAuctionInfoHtmlParser;

    public:
        typedef std::vector<CYahooAuctionInfo> List;

    public:
        const std::string& getId() const
            { return _id; }
            
        const std::string& getTitle() const
            { return _title; }
            
        const std::string& getPriceJpy() const
            { return _priceJpy; }
            
        const std::string& getImageUrl() const
            { return _imageUrl; }
            
    private:
        CYahooAuctionInfo();
        
    private:
        std::string _id;
        std::string _title;
        std::string _priceJpy;
        std::string _imageUrl;
    };

    class CYahooAuctionInfoHtmlParser
    {
    public:
        CYahooAuctionInfoHtmlParser(const std::stringstream& response);
        
    public:
        bool hasNext();

        const CYahooAuctionInfo& next() const
            { return _currentAuctionInfo; }

    private:
        static std::string_view HTML_CLASS_PRODUCT_TITLE;
        static std::string_view HTML_TAG_DATA_AUCTION_ID;
        static std::string_view HTML_TAG_DATA_AUCTION_TITLE;
        static std::string_view HTML_TAG_DATA_AUCTION_IMAGE;
        static std::string_view HTML_TAG_DATA_AUCTION_PRICE;

    private:
        CHtmlContent _content;
        CHtmlParser _parser;
        CYahooAuctionInfo _currentAuctionInfo;
    }; 
}
