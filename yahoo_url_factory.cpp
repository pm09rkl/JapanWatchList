#include <algorithm>
#include "yahoo_url_factory.h"

namespace watchList
{
    static std::string createKeywordForUrl(const CYahooSearchQuery& searchQuery)
    {
        std::string keyword(searchQuery.getKeyword());
        std::replace(keyword.begin(), keyword.end(), ' ', '+');
        return keyword;        
    }

    std::string_view CYahooUrlFactory::createHost()
    {
        return "auctions.yahoo.co.jp";
    }

    std::string CYahooUrlFactory::createTarget(const CYahooSearchQuery& searchQuery)
    {
        std::string keyword = createKeywordForUrl(searchQuery);
        std::string target = "/search/search?p=";
        target += keyword;
        target += "&auccat=";
        target += searchQuery.getCategory();
        target += "&va=";
        target += keyword;
        target += "&is_postage_mode=1&dest_pref_code=13&exflg=1&b=1&n=100&s1=new&o1=d&";
        
        if (searchQuery.getSearchMethod() == EYahooSearchMethod::FUZZY_SEARCH)
        {
            target += "ngram=1";
        }
        else
        {
            target += "f=0x";
            if (searchQuery.getSearchMethod() == EYahooSearchMethod::TITLE)
            {
                target += "2";
            }
            else
            {
                target += "4";
            }
        }
        return target;
    }
    
    std::string CYahooUrlFactory::createTarget(std::string_view auctionId)
    {
        return std::string("/jp/auction/").append(auctionId);
    }
    
    std::string_view CInjapanUrlFactory::createHost()
    {
        return "injapan.ru";
    }

    std::string CInjapanUrlFactory::createTarget(const CYahooSearchQuery& searchQuery)
    {
        std::string keyword = createKeywordForUrl(searchQuery);
        std::string target = "/search/do//currency-JPY/mode-1/page-1/sort-start/order-na.html?query=";
        target += createKeywordForUrl(searchQuery);
        target += "&scope=";
        target += searchQuery.getCategory();
        if (searchQuery.getSearchMethod() == EYahooSearchMethod::TITLE_AND_DESCRIPTION)
        {
            target += "&description=on";
        }
        return target;        
    }

    std::string CInjapanUrlFactory::createTarget(std::string_view auctionId)
    {
        return std::string("/auction/").append(auctionId);
    }
}
