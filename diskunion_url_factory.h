#pragma once

#include "url_factory.h"
#include "diskunion_item_query.h"

namespace watchList
{
    class CDiskunionUrlFactory : public CSecureUrlFactory<CDiskunionUrlFactory>
    {
    public:
        static std::string_view createHost();
        static std::string createTarget(const CDiskunionItemQuery& itemQuery);
    };
}

