#include "yahoo_search_method.h"

namespace watchList
{
    static std::string_view TITLE_SEARCH_METHOD_STR = "title";
    static std::string_view TITLE_AND_DESCRIPTION_SEARCH_METHOD_STR = "titleAndDescription";
    static std::string_view FUZZY_SEARCH_METHOD_STR = "fuzzySearch";
    
    EYahooSearchMethod getYahooSearchMethod(std::string_view method)
    {
        if (method == TITLE_SEARCH_METHOD_STR)
        {
            return EYahooSearchMethod::TITLE;
        }
        else if (method == FUZZY_SEARCH_METHOD_STR)
        {
            return EYahooSearchMethod::FUZZY_SEARCH;
        }
        return EYahooSearchMethod::TITLE_AND_DESCRIPTION;
    }
    
    std::string_view getYahooSearchMethod(EYahooSearchMethod method)
    {
        switch (method)
        {
            case EYahooSearchMethod::TITLE:
                return TITLE_SEARCH_METHOD_STR;
            case EYahooSearchMethod::TITLE_AND_DESCRIPTION:
                return TITLE_AND_DESCRIPTION_SEARCH_METHOD_STR;
            case EYahooSearchMethod::FUZZY_SEARCH:
                return FUZZY_SEARCH_METHOD_STR;
        }
        return TITLE_AND_DESCRIPTION_SEARCH_METHOD_STR;
    }
}
