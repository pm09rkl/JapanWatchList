#include "diskunion_url_factory.h"

namespace watchList
{
    std::string_view CDiskunionUrlFactory::createHost()
    {
        return "diskunion.net";
    }

    std::string CDiskunionUrlFactory::createTarget(const CDiskunionItemQuery& itemQuery)
    {
        return std::string("/portal/ct/detail/").append(itemQuery.getCode());
    }
}
