#pragma once

#include "yahoo_search_query.h"
#include "url_factory.h"

namespace watchList
{
    class CYahooUrlFactory : public CSecureUrlFactory<CYahooUrlFactory>
    {
    public:
        static std::string_view createHost();
        static std::string createTarget(const CYahooSearchQuery& searchQuery);
        static std::string createTarget(std::string_view auctionId);
    };
    
    class CInjapanUrlFactory : public CSecureUrlFactory<CInjapanUrlFactory>
    {
    public:
        static std::string_view createHost();
        static std::string createTarget(const CYahooSearchQuery& searchQuery);
        static std::string createTarget(std::string_view auctionId);
    };    
}
