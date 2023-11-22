#pragma once

#include <string>
#include <fstream>
#include <vector>

namespace watchList
{
    class CDiskunionItemQuery
    {
        friend class CDiskunionFileItemQueryParser;
        
    public:       
        const std::string& getName() const
            { return _name; }
            
        const std::string& getUrl() const
            { return _url; }
            
        const std::string& getCode() const
            { return _code; }
            
    public:
        bool operator<(const CDiskunionItemQuery& other) const;
        
    private:
        CDiskunionItemQuery();

    private:
        std::string _name;
        std::string _code;
        std::string _url;
    };
    
    class CDiskunionFileItemQueryParser
    {
    public:
        CDiskunionFileItemQueryParser(const std::string& itemsFilePath);
        
    public:
        bool hasNext();

        const CDiskunionItemQuery& next() const
            { return _currentItemQuery; }

    private:
        std::ifstream _itemsFile;
        CDiskunionItemQuery _currentItemQuery;
    };
}

