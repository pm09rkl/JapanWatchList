#include <map>
#include <boost/algorithm/string.hpp>
#include "yahoo_search_query.h"

namespace watchList
{
namespace detail
{       
    typedef std::map<std::string, std::string> YahooCategoryNameMap;
    
    YahooCategoryNameMap createYahooCategoryNameMap()
    {
        YahooCategoryNameMap map;
        map[""] = "Unknown category";
        map["22152"] = "Music";
        map["22192"] = "CD";
        map["22260"] = "Records";
        map["22344"] = "Tapes";
        map["2084007217"] = "Records, Hard Rock";
        map["2084007205"] = "CD, Hard Rock";
        map["2084007218"] = "Records, Punk";
        map["2084007024"] = "Records, Rock";
        map["22196"] = "CD, Rock";
        
        return map;
    }
}
    
CYahooSearchQuery::CYahooSearchQuery()
    :_searchMethod(EYahooSearchMethod::TITLE)
{
}

const std::string& CYahooSearchQuery::getCategoryName() const
{
    static detail::YahooCategoryNameMap categoryName = detail::createYahooCategoryNameMap();
    auto it = categoryName.find(_category);
    return (it == categoryName.end()) ? categoryName.at("") : it->second;
}

std::string CYahooSearchQuery::createResponseName() const
{
    std::string responseName = _keyword + " ";
    responseName += getCategoryName() + " ";
    responseName += getYahooSearchMethod(_searchMethod);
    return responseName;
}

bool CYahooSearchQuery::operator<(const CYahooSearchQuery& other) const
{
    int cmpResult = _keyword.compare(other._keyword);
    if (cmpResult < 0)
    {
        return true;
    }
    else if (cmpResult == 0)
    {
        cmpResult = _category.compare(other._category);
        if (cmpResult < 0)
        {
            return true;
        }
        else if (cmpResult == 0)
        {
            return _searchMethod < other.getSearchMethod();
        }
    }
    return false;
}

std::string_view CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::PARAM_NAME_CATEGORIES = "-categories:";
std::string_view CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::PARAM_NAME_SEARCH_METHOD = "-searchMethod:";

CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::CSearchParamsParser()
    :_categories(getDefaultCategories())
    , _searchMethod(getDefaultSearchMethod())
{
}

std::string_view CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::getParamValue(std::string_view line, std::string_view paramName)
{
    std::string_view value;
    std::size_t paramPos = line.find(paramName);
    if (paramPos != std::string::npos)
    {
        auto first = line.begin() + paramPos + paramName.size();
        auto last = std::find_if(first, line.end(), ::isspace);
        value = std::string_view(first, last);
    }
    return value;
}

void CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::parse(std::string_view line, std::string_view paramName, StringParamList& strList)
{
    std::string_view value = getParamValue(line, paramName);
    if (!value.empty())
    {
        boost::split(strList, value, boost::is_any_of(","));
    }
}

void CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::parse(std::string_view line, EYahooSearchMethod& searchMethod)
{
    std::string_view value = getParamValue(line, PARAM_NAME_SEARCH_METHOD);
    if (!value.empty())
    {
        searchMethod = getYahooSearchMethod(value);
    }    
}

bool CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::parse(const std::string& line)
{
    if (isParametersLine(line))
    {
        parse(line, PARAM_NAME_CATEGORIES, _categories);
        parse(line, _searchMethod);
        return true;
    }
    return false;
}

bool CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::isParametersLine(std::string_view line)
{
    return line.front() == '-';
}

CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::StringParamList CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::getDefaultCategories()
{
    StringParamList catList;
    catList.push_back("22152"); // music category id
    return catList;
}

EYahooSearchMethod CYahooKeywordsFileSearchQueryParser::CSearchParamsParser::getDefaultSearchMethod()
{
    return EYahooSearchMethod::TITLE_AND_DESCRIPTION;
}

CYahooKeywordsFileSearchQueryParser::CYahooKeywordsFileSearchQueryParser(const std::string& keywordsFilePath)
{       
    _keywordsFile.exceptions(std::ios::failbit | std::ios::badbit);
    _keywordsFile.open(keywordsFilePath);
    _categoryIndex = _paramsParser.getCategories().size();
}

void CYahooKeywordsFileSearchQueryParser::setCurrentSearchQuery()
{
    _currentSearchQuery._category = _paramsParser.getCategories()[_categoryIndex];
    _currentSearchQuery._keyword = _keywordsLine;
    _currentSearchQuery._searchMethod = _paramsParser.getSearchMehod();
    _categoryIndex++;
}

bool CYahooKeywordsFileSearchQueryParser::hasNext()
{
    if (!_keywordsFile.eof())
    { 
        if (_categoryIndex >= _paramsParser.getCategories().size())
        {
            bool isKeywordLine = false;
            while (std::getline(_keywordsFile, _keywordsLine))
            {
                if (!_paramsParser.parse(_keywordsLine))
                {
                    isKeywordLine = true;
                    break;
                }
            }
            if (isKeywordLine)
            {
                _categoryIndex = 0;
                setCurrentSearchQuery();
                return true;
            }
        }
        else
        {
            setCurrentSearchQuery();
            return true;
        }
    }
    return false;
}
}
