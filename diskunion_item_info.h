#pragma once

#include "html_parser.h"
#include <string>
#include <vector>

namespace watchList
{
    class CDiskunionItemDescription
    {
        friend class CDiskunionItemInfoHtmlParser;

    public:
        const std::string& getLabel() const
            { return _label; }
            
        const std::string& getCountry() const
            { return _country; }
            
        const std::string& getFormat() const
            { return _format; }

        const std::string& getCatalogNumber() const
            { return _catalogNumber; }
            
        const std::string& getReleaseYear() const
            { return _releaseYear; }
            
        const std::string& getBarcode() const
            { return _barcode; }
            
        const std::string& getImageUrl() const
            { return _imageUrl; }

        bool isProblemItem() const
            { return _isProblemItem; }

    private:
        CDiskunionItemDescription();
        
    private:
        std::string _label;
        std::string _country;
        std::string _format;
        std::string _catalogNumber;
        std::string _releaseYear;        
        std::string _barcode;
        std::string _imageUrl;
        bool _isProblemItem;
    };
    
    class CDiskunionUsedItemInfo
    {
        friend class CDiskunionItemInfoHtmlParser;
        
    public:
        typedef std::vector<std::string> Description;
        typedef std::vector<CDiskunionUsedItemInfo> List;

    public:
        const std::string& getPriceJpy() const
            { return _priceJpy; }
        
        const std::string& getId() const
            { return _id; }
        
        const Description& getDescription() const
            { return _description; }
            
    private:
        CDiskunionUsedItemInfo();

    private:
        std::string _priceJpy;
        std::string _id;
        Description _description;
    };
    
    class CDiskunionItemInfoHtmlParser;
    
    class CDiskunionItemInfo
    {
    public:
        CDiskunionItemInfo(const CDiskunionItemInfoHtmlParser& parser);
        
    public:
        const CDiskunionItemDescription& getDescription() const
            { return _itemDescription; }
            
        const CDiskunionUsedItemInfo::List& getUsedItems() const
            { return _usedItems;}
            
        CDiskunionUsedItemInfo::List& getUsedItems()
            { return _usedItems;}            

    private:
        CDiskunionItemDescription _itemDescription;
        CDiskunionUsedItemInfo::List _usedItems;
    };

    class CDiskunionItemInfoHtmlParser
    {
    public:
        CDiskunionItemInfoHtmlParser(const std::stringstream& response);
        
    public:
        bool hasNextUsedItem();

        const CDiskunionUsedItemInfo& nextUsedItem() const
            { return _currentUsedItemInfo; }
            
        const CDiskunionItemDescription& getItemDescription() const
            { return _itemDescription; }
            
    private:
        void parseItemInfo();
        
        std::string_view getLabel() const;
        std::string_view getReleaseYear() const;
        std::string_view getItemInfoValue(std::string_view tagMarker) const;
        std::string_view getPrice() const;
        void setDescription();
        
    private:
        static std::string_view LABEL_TAG;
        static std::string_view COUNTRY_TAG;
        static std::string_view FORMAT_TAG;
        static std::string_view CATALOG_NUMBER_TAG;
        static std::string_view RELEASE_YEAR_TAG;
        static std::string_view BARCODE_TAG;
        static std::string_view ITEM_VALUE_OPEN_TAG;
        static std::string_view ITEM_VALUE_CLOSE_TAG;
        static std::string_view REF_CLOSE_TAG;
        static std::string_view USED_AREA_OPEN_TAG;
        static std::string_view ITEM_SPEC_AREA_OPEN_TAG;        
        static std::string_view PRICE_OPEN_TAG;
        static std::string_view PRICE_CLOSE_TAG;
        static std::string_view DATA_ID_ATTRIBUTE;
        static std::string_view DESCRIPTION_OPEN_TAG;
        static std::string_view DESCRIPTION_CLOSE_TAG;
        static std::string_view LIST_OPEN_TAG;
        static std::string_view LIST_CLOSE_TAG;
        static std::string_view META_IMAGE_PROPERTY_TAG;
        static std::string_view CONTENT_ATTRIBUTE;
            
    private:
        CDiskunionItemDescription _itemDescription;
        CDiskunionUsedItemInfo _currentUsedItemInfo;
        CHtmlContent _content;
        CHtmlParser _parser;
    };
    
}

