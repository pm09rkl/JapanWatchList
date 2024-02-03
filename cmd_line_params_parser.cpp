#include "app_settings.h"
#include "cmd_line_params_parser.h"
#include <filesystem>

namespace watchList
{
CCmdLineParamsParser::CCmdLineParamsParser(int argCount, char ** argValues)
    : _isWatchDiskunion(false)
    , _isContinueLastSession(false)
    , _isWatchYahoo(true)
    , _yahooKeywordsFilePath(DATA_DIR + YAHOO_KEYWORDS_DEFAULT_FILE_NAME)
    , _diskunionItemsFilePath(DATA_DIR + DISKUNION_ITEMS_DEFAULT_FILE_NAME)
{
    parse(argCount, argValues);
}

bool CCmdLineParamsParser::parseFileName(int argCount, char** argValues, int* argNum, std::string* fileName)
{
    int argNumNext = *argNum + 1;
    if (argNumNext < argCount)
    {
        std::string_view argFileName = argValues[argNumNext];
        if (!argFileName.empty() && (argFileName.front() != '-'))
        {
            std::filesystem::path argFilePath(argFileName);
            fileName->clear();
            if (!argFilePath.has_parent_path())
            {
                *fileName += DATA_DIR;
            }
            *fileName += argFileName;
            *argNum = argNumNext;
            return true;
        }
    }
    return false;
}

void CCmdLineParamsParser::parse(int argCount, char** argValues)
{
    for (int argNum = 1; argNum < argCount; ++argNum)
    {
        std::string_view argValue = argValues[argNum];
        if ((argValue == "-y") or (argValue == "--yahoo"))
        {   
            parseFileName(argCount, argValues, &argNum, &_yahooKeywordsFilePath);
        }
        else 
        {
            bool isParseFileName = true;
            if ((argValue == "-d") or (argValue == "--diskunion"))
            {
                _isWatchDiskunion = true;
            }
            else if (argValue == "--diskunion-only")
            {
                _isWatchDiskunion = true;
                _isWatchYahoo = false;
            }
            else
            {
                isParseFileName = false;
                if (argValue == "--continue")
                {
                    _isContinueLastSession = true;
                }
            }
            if (isParseFileName)
            {
                parseFileName(argCount, argValues, &argNum, &_diskunionItemsFilePath);
            }
        }
    }
}

}
