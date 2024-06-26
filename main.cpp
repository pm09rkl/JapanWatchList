#include <boost/json.hpp>
#include <boost/date_time.hpp>
#include <boost/system/system_error.hpp>
#include <boost/exception/diagnostic_information.hpp> 
#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <thread>
#include <queue>
#include <filesystem>
#include "NLTemplate.h"
#include "app_settings.h"
#include "diskunion_url_factory.h"
#include "diskunion_item_info.h"
#include "diskunion_item_query.h"
#include "yahoo_auction_info.h"
#include "yahoo_search_query.h"
#include "yahoo_url_factory.h"
#include "json_pretty_print.h"
#include "async_https_downloader.h"
#include "web_engine_downloader.h"
#include "cmd_line_params_parser.h"
#include <boost/algorithm/string/replace.hpp>

using namespace watchList;
using namespace boost;
using namespace NL::Template;

typedef std::map<CYahooSearchQuery, CYahooAuctionInfo::List> YahooSearchQueryResults;
typedef std::map<CDiskunionItemQuery, CDiskunionItemInfo> DiskunionItemQueryResults;

typedef std::vector<std::thread> ThreadList;
typedef std::set<std::string> WatchHistory;

static void validateErrorCode(const system::error_code& errorCode)
{
    if (errorCode)
    {
        throw system::system_error(errorCode);
    }
}

class CThreadList
{
public:
    ~CThreadList();
    
public:
    void run(asio::io_context& ioContext, int numThreads = 4);
    void join();
    
private:
    std::vector<std::thread> _threads;
};

void CThreadList::run(asio::io_context& ioContext, int numThreads)
{
    for (int i = numThreads; i > 0; --i)
    {
        _threads.emplace_back(
                [&ioContext]()
                {
                    ioContext.run();
                });
    }    
}

void CThreadList::join()
{
    for (auto& thread : _threads)
    {
        thread.join();
    }
}

CThreadList::~CThreadList()
{
    for (auto& thread : _threads)
    {
        if (thread.joinable())
        {
            try
            {
                thread.detach();                
            }
            catch (std::system_error)
            {       
            }
        }
    }
}

static void insertYahooAuctions(YahooSearchQueryResults& searchQueryResults, CYahooSearchQuery&& searchQuery, CYahooAuctionInfo::List&& auctions)
{
    if (!auctions.empty())
    {
        searchQueryResults.emplace(std::move(searchQuery), std::move(auctions));
    }
}

static void insertDiskunionItems(DiskunionItemQueryResults& itemQueryResults, CDiskunionItemQuery&& itemQuery,  CDiskunionItemInfo&& itemInfo)
{
    if (itemInfo.getDescription().isProblemItem() || !itemInfo.getUsedItems().empty())
    {
        itemQueryResults.emplace(std::move(itemQuery), std::move(itemInfo));
    }
}

static std::string getWatchHistoryFileName(std::filesystem::path inputFileName, std::string_view watchListName)
{
    std::string watchHistoryFileName;
    if (inputFileName.has_parent_path())
    {
        watchHistoryFileName += inputFileName.parent_path();
    }
    else
    {
        watchHistoryFileName += DATA_DIR_WITHOUT_SLASH;
    }
    watchHistoryFileName += "/watchHistory_";
    watchHistoryFileName += watchListName;
    watchHistoryFileName += ".json";
    return watchHistoryFileName;
}

class CDownloadTask
{
public:
    CDownloadTask(std::string_view url, std::string_view responseName, CWebEngineDownloader& downloader, bool isContinueLastSession);
    
protected:
    bool readResponse(std::stringstream& response);
    
private:
    CWebEngineDownloader::FutureResponseType _responsePath;
    std::string _responsePathReady;    
};

CDownloadTask::CDownloadTask(std::string_view url, std::string_view responseName, CWebEngineDownloader& downloader, bool isContinueLastSession)
{
    std::string responseNameWithExt(responseName);
    //responseNameWithExt += ".html";

    _responsePathReady = DOWNLOADS_DIR + responseNameWithExt;    
    bool isAddDownload = !isContinueLastSession || !std::filesystem::exists(_responsePathReady);

    if (isAddDownload)
    {
        _responsePathReady.clear();
        _responsePath = downloader.addDownload(url, responseName);
    }
}

bool CDownloadTask::readResponse(std::stringstream& response)
{
    try
    {
        std::ifstream responseFile;
        responseFile.exceptions(std::ios::failbit | std::ios::badbit);
        
        if (_responsePathReady.empty())
        {
            _responsePathReady = _responsePath.get();
        }
        
        responseFile.open(_responsePathReady);

        std::copy(std::istreambuf_iterator<char>(responseFile), std::istreambuf_iterator<char>(), std::ostreambuf_iterator(response));
        
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return false;
    }
}

class CYahooAuctionsTask : public CDownloadTask
{
public:
    typedef std::queue<CYahooAuctionsTask> Queue;
    
public:
    CYahooAuctionsTask(const CYahooSearchQuery& searchQuery, CWebEngineDownloader& downloader, bool isContinueLastSession);
    
    void doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistory, bool isIgnoreHistory);
    
private:
    static int maxNewAuctionsToWatch;    
    
private:
    CYahooSearchQuery _searchQuery;
};

CYahooAuctionsTask::CYahooAuctionsTask(const CYahooSearchQuery& searchQuery, CWebEngineDownloader& downloader, bool isContinueLastSession)
    : CDownloadTask(CYahooUrlFactory::createUrl(searchQuery), searchQuery.createResponseName(), downloader, isContinueLastSession) 
    , _searchQuery(searchQuery)
{
}

int CYahooAuctionsTask::maxNewAuctionsToWatch = 10;

void CYahooAuctionsTask::doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistory, bool isIgnoreHistory)
{
    std::stringstream response;
    if (readResponse(response))
    {
        CYahooAuctionInfo::List newAuctions;
        CYahooAuctionInfoHtmlParser parser(response);
        while (parser.hasNext())
        {
            const CYahooAuctionInfo& auctionInfo = parser.next();
            if (isIgnoreHistory || (watchHistory.count(auctionInfo.getId()) == 0))
            {
                if (!isIgnoreHistory || (newAuctions.size() < maxNewAuctionsToWatch))
                {
                    newAuctions.emplace_back(auctionInfo);
                }
                watchHistory.insert(auctionInfo.getId());
            }      
        }
        insertYahooAuctions(searchQueryResults, std::move(_searchQuery), std::move(newAuctions));
    }
}

static void printWatchHistoryFile(const std::string& watchHistoryFileName, const WatchHistory& watchHistory)
{
    std::ofstream watchHistoryFile;
    watchHistoryFile.exceptions(std::ios::failbit | std::ios::badbit);
    watchHistoryFile.open(watchHistoryFileName);
    json::value watchHistoryValue = json::value_from(watchHistory);
    boost_ext::pretty_print(watchHistoryFile, watchHistoryValue);
}

static std::string createHtmlPath(const std::string& queryFileName)
{
    posix_time::ptime time = posix_time::second_clock::local_time();
    posix_time::time_facet* facet = new posix_time::time_facet("%d-%m-%Y_%H-%M-%S");
    
    
    std::string queryFileStem = std::filesystem::path(queryFileName).stem();
    std::stringstream stream;
    stream.imbue(std::locale(stream.getloc(), facet));
    stream << DATA_DIR << queryFileStem << "_" << time << ".html";

    return stream.str();
}

static void renderHtmlFile(Template& htmlTemplate, const std::string& queryFileName)
{
    std::ofstream htmlFile;
    htmlFile.exceptions(std::ios::failbit | std::ios::badbit);
    htmlFile.open(createHtmlPath(queryFileName));
    if (htmlFile.is_open())
    {
        htmlTemplate.render(htmlFile);
    }
}

static void createYahooHtmlFile(const YahooSearchQueryResults& searchQueryResults, const std::string& keywordsFileName)
{
    if (!searchQueryResults.empty())
    {
        LoaderFile loader;
        Template htmlTemplate(loader);
        htmlTemplate.load(TEMPLATE_YAHOO_PATH);

        std::size_t searchQueryCount = 0;
        Block& searchQueryBlock = htmlTemplate.block("searchQuery");
        searchQueryBlock.repeat(searchQueryResults.size()); 
        for (const auto& [searchQuery, newAuctions] : searchQueryResults)
        {
            Node& searchQueryNode = searchQueryBlock[searchQueryCount];
                
            searchQueryNode.set("injapanSearchLink", CInjapanUrlFactory::createUrl(searchQuery));
            searchQueryNode.set("keyword", searchQuery.getKeyword());
            searchQueryNode.set("category", searchQuery.getCategoryName());
            searchQueryNode.set("numMatches", std::to_string(newAuctions.size()));
                    
            std::size_t newAuctionCount = 0;
            Block& newAuctionBlock = searchQueryNode.block("newAuctions");
            newAuctionBlock.repeat(newAuctions.size());        
            for (const CYahooAuctionInfo& newAuction : newAuctions)
            {
                Node& newAuctionNode = newAuctionBlock[newAuctionCount];
                newAuctionNode.set("imageLink", newAuction.getImageUrl());
                newAuctionNode.set("priceJpy", newAuction.getPriceJpy());
                newAuctionNode.set("injapanAuctionLink", CInjapanUrlFactory::createUrl(newAuction.getId()));
                newAuctionNode.set("auctionTitle", newAuction.getTitle());
                newAuctionNode.set("yahooAuctionLink", CYahooUrlFactory::createUrl(newAuction.getId()));
                newAuctionCount++;
            }
            
            searchQueryCount++;
        }
        
        renderHtmlFile(htmlTemplate, keywordsFileName);
    }
}

template <typename T, typename... Args>
static void doTasks(std::queue<T>& tasks, Args&&... args)
{
    while (!tasks.empty())
    {
        tasks.front().doTask(std::forward<Args>(args)...);
        tasks.pop();
    }    
}

static WatchHistory getWatchHistory(const std::string& watchHistoryFileName)
{
    std::ifstream watchHistoryFileInput;
    watchHistoryFileInput.exceptions(std::ios::failbit | std::ios::badbit);
    watchHistoryFileInput.open(watchHistoryFileName);

    std::istreambuf_iterator<char> first(watchHistoryFileInput);
    std::istreambuf_iterator<char> last;
    std::string content(first, last);

    json::parser parser;
    json::error_code errorCode;
    parser.write(content, errorCode);
    validateErrorCode(errorCode);
    
    return json::value_to<WatchHistory>(parser.release());
}

static void prepareDownloadsDir()
{
    if (std::filesystem::exists(DOWNLOADS_DIR))
    {
        std::filesystem::remove_all(DOWNLOADS_DIR);
    }
    if (!std::filesystem::create_directory(DOWNLOADS_DIR))
    {
        std::string message = "Can't create download dir ";
        message.append(DOWNLOADS_DIR);
        throw std::runtime_error(message);
    }
}

static void prepareDownloader(CWebEngineDownloader& downloader, bool isContinueLastSession)
{
    if (!isContinueLastSession)
    {
        prepareDownloadsDir();
    }
    downloader.setDownloadDir(DOWNLOADS_DIR);
}

static void createYahooAuctionsHtml(const std::string& keywordsFileName, const std::string& watchHistoryFileName, bool isContinueLastSession)
{          
    CYahooAuctionsTask::Queue tasks;
    CWebEngineDownloader downloader;
    CYahooKeywordsFileSearchQueryParser searchQueryParser(keywordsFileName);
    while (searchQueryParser.hasNext())
    {
        tasks.emplace(searchQueryParser.next(), downloader, isContinueLastSession);        
    }
    prepareDownloader(downloader, isContinueLastSession);    
   
    YahooSearchQueryResults searchQueryResults;
    WatchHistory watchHistory;
    if (std::filesystem::exists(watchHistoryFileName))
    {
        watchHistory = getWatchHistory(watchHistoryFileName);
    }
    bool isIgnoreHistory = watchHistory.empty();
    doTasks(tasks, searchQueryResults, watchHistory, isIgnoreHistory);
    printWatchHistoryFile(watchHistoryFileName, watchHistory);
    createYahooHtmlFile(searchQueryResults, keywordsFileName);
}

static void watchYahooAuctions(const std::string& keywordsFileName, bool isContinueLastSession)
{
    std::string watchHistoryFileName = getWatchHistoryFileName(keywordsFileName, "yahoo");
    createYahooAuctionsHtml(keywordsFileName, watchHistoryFileName, isContinueLastSession);
}

static void createDiskunionHtmlFile(const DiskunionItemQueryResults& itemQueryResults, const std::string& itemsFileName)
{
    if (!itemQueryResults.empty())
    {
        LoaderFile loader;
        Template htmlTemplate(loader);
        htmlTemplate.load(TEMPLATE_DISKUNION_PATH);

        std::size_t itemQueryCount = 0;
        Block& itemQueryBlock = htmlTemplate.block("itemQuery");
        itemQueryBlock.repeat(itemQueryResults.size()); 
        for (const auto& [itemQuery, item] : itemQueryResults)
        {
            Node& itemQueryNode = itemQueryBlock[itemQueryCount];
            const CDiskunionItemDescription& itemDescription = item.getDescription();
            const CDiskunionUsedItemInfo::List& usedItems = item.getUsedItems();
            
            std::string itemQueryDescription = itemQuery.getName();
            if (itemDescription.isProblemItem())
            {
                itemQueryDescription = "!!! PROBLEM ITEM !!! " + itemQueryDescription;
            }
            
            itemQueryNode.set("itemImageLink", itemDescription.getImageUrl());
            itemQueryNode.set("itemLink", itemQuery.getUrl());
            itemQueryNode.set("itemQueryDescription", itemQueryDescription);
            itemQueryNode.set("numUsedItems", std::to_string(usedItems.size()));
            itemQueryNode.set("itemLabel", itemDescription.getLabel());
            itemQueryNode.set("itemCountry", itemDescription.getCountry());
            itemQueryNode.set("itemFormat", itemDescription.getFormat());
            itemQueryNode.set("itemCatalogNumber", itemDescription.getCatalogNumber());
            itemQueryNode.set("itemReleaseYear", itemDescription.getReleaseYear());
            itemQueryNode.set("itemBarcode", itemDescription.getBarcode());
                    
            std::size_t usedItemsCount = 0;
            Block& usedItemsBlock = itemQueryNode.block("usedItems");
            usedItemsBlock.repeat(usedItems.size());

            for (const CDiskunionUsedItemInfo& usedItem : usedItems)
            {
                Node& usedItemNode = usedItemsBlock[usedItemsCount];
                usedItemNode.set("priceJpy", usedItem.getPriceJpy());

                const CDiskunionUsedItemInfo::Description& desc = usedItem.getDescription();
                std::size_t usedItemDescLinesCount = 0;
                Block& usedItemDescBlock = usedItemNode.block("usedItem");
                usedItemDescBlock.repeat(desc.size());
                for (const std::string& usedItemDescLine : desc)
                {
                    Node& usedItemDescNode = usedItemDescBlock[usedItemDescLinesCount];
                    usedItemDescNode.set("usedItemDescription", usedItemDescLine);
                    usedItemDescLinesCount++;
                }

                usedItemsCount++;
            }
            
            itemQueryCount++;
        }

        renderHtmlFile(htmlTemplate, itemsFileName);
    }
}

class CDiskunionAddAllItemsTask : public CDownloadTask
{
public:
    typedef std::queue<CDiskunionAddAllItemsTask> Queue;

public:
    CDiskunionAddAllItemsTask(const CDiskunionItemQuery& itemQuery, CWebEngineDownloader& downloader, bool isContinueLastSession);
    
    void doTask(DiskunionItemQueryResults& itemQueryResults, WatchHistory& watchHistory, bool isIgnoreHistory);
    
private:
    CDiskunionItemQuery _itemQuery;
    CWebEngineDownloader::FutureResponseType _responsePath;
    std::string _responsePathReady;
};

CDiskunionAddAllItemsTask::CDiskunionAddAllItemsTask(const CDiskunionItemQuery& itemQuery, CWebEngineDownloader& downloader, bool isContinueLastSession)
    : CDownloadTask(itemQuery.getUrl(), itemQuery.getCode(), downloader, isContinueLastSession)
    , _itemQuery(itemQuery)
{
}

void CDiskunionAddAllItemsTask::doTask(DiskunionItemQueryResults& itemQueryResults, WatchHistory& watchHistory, bool isIgnoreHistory)
{
    std::stringstream response;
    readResponse(response);

    CDiskunionItemInfoHtmlParser parser(response);
    CDiskunionItemInfo itemInfo(parser);
    while (parser.hasNextUsedItem())
    {
        const CDiskunionUsedItemInfo& usedItemInfo = parser.nextUsedItem();
        if (isIgnoreHistory || (watchHistory.count(usedItemInfo.getId()) == 0))
        {
            itemInfo.getUsedItems().emplace_back(usedItemInfo);
            watchHistory.insert(usedItemInfo.getId());            
        }
    }
    insertDiskunionItems(itemQueryResults, std::move(_itemQuery), std::move(itemInfo));
}

static void createDiskunionItemsHtml(const std::string& itemsFileName, const std::string& watchHistoryFileName, bool isContinueLastSession)
{
    CDiskunionAddAllItemsTask::Queue tasks;
    CWebEngineDownloader downloader;
    CDiskunionFileItemQueryParser parser(itemsFileName);
    while (parser.hasNext())
    {
        tasks.emplace(parser.next(), downloader, isContinueLastSession);
    }
    prepareDownloader(downloader, isContinueLastSession);

    DiskunionItemQueryResults itemQueryResults;
    WatchHistory watchHistory;
    if (std::filesystem::exists(watchHistoryFileName))
    {
        watchHistory = getWatchHistory(watchHistoryFileName);
    }
    bool isIgnoreHistory = watchHistory.empty();
    doTasks(tasks, itemQueryResults, watchHistory, isIgnoreHistory);
    printWatchHistoryFile(watchHistoryFileName, watchHistory);
    createDiskunionHtmlFile(itemQueryResults, itemsFileName);    
}

static void watchDiskunionItems(const std::string& diskunionItemsFileName, bool isContinueLastSession)
{
    std::string watchHistoryFileName = getWatchHistoryFileName(diskunionItemsFileName, "diskunion");
    createDiskunionItemsHtml(diskunionItemsFileName, watchHistoryFileName, isContinueLastSession);
}

static void bookmarksToDiskunionItems()
{
    std::ifstream stream("./bookmarks.html");
    std::ofstream streamItems("./diskunion_items.txt");
    std::stringstream response;
    
    std::copy(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>(), std::ostreambuf_iterator(response));
    
    CHtmlContent content(response);
    CHtmlParser parser = content.createParser();
    parser.skipBeginning("DiskUnion Items</H3>");
    parser.skipBeginning("<A");
    while (parser.hasContent())
    {
        std::string_view linkView = parser.getAttributeValue("HREF");
        CHtmlParser nameParser = parser;
        nameParser.skipBeginning(">");
        nameParser.skipEnding("<");
        std::string_view nameView = nameParser.getContent();
        
        std::string link(linkView.begin(), linkView.end());
        std::string name(nameView.begin(), nameView.end());
        
        boost::replace_all(name, std::string("&#39;"), std::string("'"));
        boost::replace_all(name, std::string("&quot;"), std::string("\""));
        boost::replace_all(name, std::string("&amp;"), std::string("&"));
        
        //boost::replace_all(link, std::string("https://diskunion.net/portal/ct/detail/"), std::string());
        
        streamItems << name << std::endl;
        streamItems << link << std::endl;
        parser.skipBeginning("<A");
    }
}

int main(int argCount, char** argValues)
{    
    try
    {
        CCmdLineParamsParser parser(argCount, argValues);
        if (parser.isWatchYahoo())
        {
            watchYahooAuctions(parser.getYahooKeywordsFilePath(), parser.isContinueLastSession());
        }
        if (parser.isWatchDiskunion())
        {
            watchDiskunionItems(parser.getDiskunionItemsFilePath(), parser.isContinueLastSession());
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << current_exception_diagnostic_information() << std::endl;
        return 1;
    }
    return 0;
}
