#pragma once

#include <string>
#include <fstream>
#include <vector>
#include "yahoo_search_method.h"

namespace watchList
{
    class CYahooSearchQuery
    {
        friend class CYahooKeywordsFileSearchQueryParser;
        
    public:       
        EYahooSearchMethod getSearchMethod() const
            { return _searchMethod; }

        const std::string& getKeyword() const
            { return _keyword; }
            
        const std::string& getCategory() const
            { return _category; }           
            
        const std::string& getCategoryName() const;
        
        std::string createResponseName() const;
        
    public:
        bool operator<(const CYahooSearchQuery& other) const;
        
    public:
        CYahooSearchQuery();

    public:
        std::string _keyword;
        std::string _category;
        EYahooSearchMethod _searchMethod;
    };
    
    class CYahooKeywordsFileSearchQueryParser
    {
    public:
        CYahooKeywordsFileSearchQueryParser(const std::string& keywordsFilePath);
        
    public:
        bool hasNext();

        const CYahooSearchQuery& next() const
            { return _currentSearchQuery; }
            
    private:
        void setCurrentSearchQuery();
            
    private:
        class CSearchParamsParser
        {
        public:
            typedef std::vector<std::string> StringParamList;
            
        public:
            CSearchParamsParser();

        public:
            bool parse(const std::string& line);
            
        public:
            const StringParamList& getCategories() const
                { return _categories; }
                
            EYahooSearchMethod getSearchMehod() const
                { return _searchMethod; }
                
        private:
            static std::string_view PARAM_NAME_CATEGORIES;
            static std::string_view PARAM_NAME_SEARCH_METHOD;

        private:
            static bool isParametersLine(std::string_view line);
            static StringParamList getDefaultCategories();
            static EYahooSearchMethod getDefaultSearchMethod();
            
            static void parse(std::string_view line, std::string_view paramName, StringParamList& strList);
            static void parse(std::string_view line, EYahooSearchMethod& _searchMethod);
            
            static std::string_view getParamValue(std::string_view line, std::string_view paramName);
            
        private:
            StringParamList _categories;
            EYahooSearchMethod _searchMethod;
        };        

    private:
        std::ifstream _keywordsFile;
        std::string _keywordsLine;
        std::size_t _categoryIndex;
        CYahooSearchQuery _currentSearchQuery;
        CSearchParamsParser _paramsParser;
    };
}
