#include "diskunion_item_query.h"

namespace watchList
{

CDiskunionItemQuery::CDiskunionItemQuery()
{    
}

bool CDiskunionItemQuery::operator<(const CDiskunionItemQuery& other) const
{
    int cmpResult = _name.compare(other._name);
    if (cmpResult < 0)
    {
        return true;
    }
    else if (cmpResult == 0)
    {
        return _code < other._code;
    }
    return false;
}

CDiskunionFileItemQueryParser::CDiskunionFileItemQueryParser(const std::string& itemsFilePath)
{
    _itemsFile.exceptions(std::ios::failbit | std::ios::badbit);
    _itemsFile.open(itemsFilePath);
}

bool CDiskunionFileItemQueryParser::hasNext()
{
    if (_itemsFile.eof())
    { 
        return false;    
    }
    if (std::getline(_itemsFile, _currentItemQuery._name) && std::getline(_itemsFile, _currentItemQuery._url))
    {
        std::size_t codePos = _currentItemQuery._url.rfind('/');
        if ((codePos != std::string::npos) && (++codePos < _currentItemQuery._url.size()))
        {
            _currentItemQuery._code = _currentItemQuery._url.substr(codePos);
        }
        return true;
    }
    return false;
}

}

