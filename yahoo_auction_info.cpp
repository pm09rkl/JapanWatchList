#include "yahoo_auction_info.h"

namespace watchList
{
std::string_view CYahooAuctionInfoHtmlParser::HTML_CLASS_PRODUCT_TITLE = "class=\"Product__title\"";
std::string_view CYahooAuctionInfoHtmlParser::HTML_TAG_DATA_AUCTION_ID = "data-auction-id";
std::string_view CYahooAuctionInfoHtmlParser::HTML_TAG_DATA_AUCTION_TITLE = "data-auction-title";
std::string_view CYahooAuctionInfoHtmlParser::HTML_TAG_DATA_AUCTION_IMAGE = "data-auction-img";
std::string_view CYahooAuctionInfoHtmlParser::HTML_TAG_DATA_AUCTION_PRICE = "data-auction-price";

CYahooAuctionInfo::CYahooAuctionInfo()
{
}

CYahooAuctionInfoHtmlParser::CYahooAuctionInfoHtmlParser(const std::stringstream& response)
    : _content(response)
{
    _parser = _content.createParser();
}

bool CYahooAuctionInfoHtmlParser::hasNext()
{
    if (_parser.hasContent())
    {
        _parser.skipBeginning(HTML_CLASS_PRODUCT_TITLE);
        if (_parser.hasContent())
        {
            _currentAuctionInfo._id = _parser.getAttributeValue(HTML_TAG_DATA_AUCTION_ID);
            _currentAuctionInfo._imageUrl = _parser.getAttributeValue(HTML_TAG_DATA_AUCTION_IMAGE);
            _currentAuctionInfo._priceJpy = _parser.getAttributeValue(HTML_TAG_DATA_AUCTION_PRICE);
            _currentAuctionInfo._title = _parser.getAttributeValue(HTML_TAG_DATA_AUCTION_TITLE);
            return true;
        }
    }
    return false;
}    
}
