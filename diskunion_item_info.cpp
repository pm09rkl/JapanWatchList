#include "diskunion_item_info.h"

namespace watchList
{
std::string_view CDiskunionItemInfoHtmlParser::LABEL_TAG = "<dt class=\"itemSpecArea__dt\">レーベル</dt>";
std::string_view CDiskunionItemInfoHtmlParser::COUNTRY_TAG = "<dt class=\"itemSpecArea__dt\">国(Country)</dt>";
std::string_view CDiskunionItemInfoHtmlParser::FORMAT_TAG = "<dt class=\"itemSpecArea__dt\">フォーマット</dt>";
std::string_view CDiskunionItemInfoHtmlParser::CATALOG_NUMBER_TAG = "<dt class=\"itemSpecArea__dt\">規格番号</dt>";
std::string_view CDiskunionItemInfoHtmlParser::RELEASE_YEAR_TAG = "<dt class=\"itemSpecArea__dt\">発売日</dt>";
std::string_view CDiskunionItemInfoHtmlParser::BARCODE_TAG = "<dt class=\"itemSpecArea__dt\">EAN</dt>";
std::string_view CDiskunionItemInfoHtmlParser::ITEM_VALUE_OPEN_TAG = "<dd class=\"itemSpecArea__dd\">";
std::string_view CDiskunionItemInfoHtmlParser::ITEM_VALUE_CLOSE_TAG = "</dd>";
std::string_view CDiskunionItemInfoHtmlParser::REF_CLOSE_TAG = "</a>";
std::string_view CDiskunionItemInfoHtmlParser::ITEM_SPEC_AREA_OPEN_TAG = "<div class=\"itemSpecArea\"";
std::string_view CDiskunionItemInfoHtmlParser::USED_AREA_OPEN_TAG = "<div class=\"itemUsedArea__txtArea\">";
std::string_view CDiskunionItemInfoHtmlParser::PRICE_OPEN_TAG = "<p class=\"u-price\">";
std::string_view CDiskunionItemInfoHtmlParser::DATA_ID_ATTRIBUTE = "data-id";
std::string_view CDiskunionItemInfoHtmlParser::DESCRIPTION_OPEN_TAG = "<ul class=\"u-bullet-note add__du__text_wordbreak\">";
std::string_view CDiskunionItemInfoHtmlParser::DESCRIPTION_CLOSE_TAG = "</ul>";
std::string_view CDiskunionItemInfoHtmlParser::LIST_OPEN_TAG = "<li>";
std::string_view CDiskunionItemInfoHtmlParser::LIST_CLOSE_TAG = "</li>";

CDiskunionItemDescription::CDiskunionItemDescription()
{
}

CDiskunionUsedItemInfo::CDiskunionUsedItemInfo()
{
}

CDiskunionItemInfo::CDiskunionItemInfo(const CDiskunionItemInfoHtmlParser& parser)
    : _itemDescription(parser.getItemDescription())
{
}

CDiskunionItemInfoHtmlParser::CDiskunionItemInfoHtmlParser(const std::stringstream& response)
    : _content(response)
{
    _parser = _content.createParser();
    parseItemInfo();
}

std::string_view CDiskunionItemInfoHtmlParser::getLabel() const
{
    std::string_view label = getItemInfoValue(LABEL_TAG);
    CHtmlParser parser(label);
    parser.skipEnding(REF_CLOSE_TAG);
    parser.skipBeginning(">");
    if (parser.hasContent())
    {
        label = parser.getContent();
    }
    return label;
}

std::string_view CDiskunionItemInfoHtmlParser::getReleaseYear() const
{
    std::string_view year = getItemInfoValue(RELEASE_YEAR_TAG);
    if (year.size() > 4)
    {
        year = year.substr(0, 4);
    }
    return year;
}

std::string_view CDiskunionItemInfoHtmlParser::getItemInfoValue(std::string_view tagMarker) const
{
    CHtmlParser parser = _parser;
    parser.skipBeginning(tagMarker);
    parser.skipBeginning(ITEM_VALUE_OPEN_TAG);
    parser.skipEnding(ITEM_VALUE_CLOSE_TAG);
    return parser.getContent();
}

void CDiskunionItemInfoHtmlParser::parseItemInfo()
{
    _parser.skipBeginning(ITEM_SPEC_AREA_OPEN_TAG);
    _itemDescription._label = getLabel();
    _itemDescription._country = getItemInfoValue(COUNTRY_TAG);
    _itemDescription._format = getItemInfoValue(FORMAT_TAG);
    _itemDescription._catalogNumber = getItemInfoValue(CATALOG_NUMBER_TAG);
    _itemDescription._releaseYear = getReleaseYear();
    _itemDescription._barcode = getItemInfoValue(BARCODE_TAG);
}

std::string_view CDiskunionItemInfoHtmlParser::getPrice() const
{
    CHtmlParser parser = _parser;
    parser.skipBeginning(PRICE_OPEN_TAG);
    parser.skipEnding("<");
    return parser.getContent();
}

void CDiskunionItemInfoHtmlParser::setDescription()
{
    CHtmlParser parser = _parser;
    parser.skipBeginning(DESCRIPTION_OPEN_TAG);
    parser.skipEnding(DESCRIPTION_CLOSE_TAG);
    _currentUsedItemInfo._description.clear();
    while (parser.hasContent())
    {
        CHtmlParser parserDescription = parser;
        parserDescription.skipBeginning(LIST_OPEN_TAG);
        parserDescription.skipEnding(LIST_CLOSE_TAG);
        if (parserDescription.hasContent())
        {
            _currentUsedItemInfo._description.emplace_back(parserDescription.getContent());
        }
        parser.skipBeginning(LIST_CLOSE_TAG);
    }
}

bool CDiskunionItemInfoHtmlParser::hasNextUsedItem()
{
    if (_parser.hasContent())
    {
        _parser.skipBeginning(USED_AREA_OPEN_TAG);
        if (_parser.hasContent())
        {
            _currentUsedItemInfo._id = _parser.getAttributeValue(DATA_ID_ATTRIBUTE);
            _currentUsedItemInfo._priceJpy = getPrice();
            setDescription();
            return true;
        }
    }
    return false;
}    
}

