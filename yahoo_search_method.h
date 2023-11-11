#pragma once

#include <string>

namespace watchList
{
    enum class EYahooSearchMethod
    {
        TITLE = 0,
        TITLE_AND_DESCRIPTION,
        FUZZY_SEARCH
    };
    
    EYahooSearchMethod getYahooSearchMethod(std::string_view method);
    std::string_view getYahooSearchMethod(EYahooSearchMethod method);
}
