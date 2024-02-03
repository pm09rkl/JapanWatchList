#pragma once

#include <string>

namespace watchList
{
    class CCmdLineParamsParser
    {
    public:
        CCmdLineParamsParser(int argCount, char** argValues);
        
    public:
        bool isWatchDiskunion() const
            { return _isWatchDiskunion; }
            
        bool isContinueLastSession() const
            { return _isContinueLastSession; }
            
        bool isWatchYahoo() const
            { return _isWatchYahoo; }            
            
        const std::string& getYahooKeywordsFilePath() const
            { return _yahooKeywordsFilePath; }

        const std::string& getDiskunionItemsFilePath() const
            { return _diskunionItemsFilePath; }
            
    private:
        void parse(int argCount, char** argValues);
        bool parseFileName(int argCount, char** argValues, int* argNum, std::string* fileName);

    private:
        bool _isWatchDiskunion;
        bool _isContinueLastSession;
        bool _isWatchYahoo;
        std::string _yahooKeywordsFilePath;
        std::string _diskunionItemsFilePath;
    };
}
