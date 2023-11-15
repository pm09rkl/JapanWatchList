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
#include "diskunion_url_factory.h"
#include "diskunion_item_info.h"
#include "diskunion_item_query.h"
#include "yahoo_auction_info.h"
#include "yahoo_search_query.h"
#include "yahoo_url_factory.h"
#include "json_pretty_print.h"
#include "async_https_downloader.h"
#include "web_engine_downloader.h"

using namespace watchList;
using namespace boost;
using namespace NL::Template;

static const std::string DATA_DIR_WITHOUT_SLASH = "./data";
static const std::string DATA_DIR = DATA_DIR_WITHOUT_SLASH + '/';
static const std::string DISKUNION_DOWNLOADS_DIR = "./diskunionTmp/";
static const std::string TEMPLATE_YAHOO_PATH = DATA_DIR + "templateYahooHtml.txt";
static const std::string TEMPLATE_DISKUNION_PATH = DATA_DIR + "templateDiskunionHtml.txt";

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
    if (!itemInfo.getUsedItems().empty())
    {
        itemQueryResults.emplace(std::move(itemQuery), std::move(itemInfo));
    }
}

class CYahooAddAllAuctionsTask
{
public:
    typedef std::queue<CYahooAddAllAuctionsTask> Queue;

public:
    CYahooAddAllAuctionsTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader);
    
    void doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistory);
    
private:
    static int maxNewAuctionsToWatch;

private:
    CYahooSearchQuery _searchQuery;
    CAsyncHttpsDownloader::FutureResponseType _response;
};

int CYahooAddAllAuctionsTask::maxNewAuctionsToWatch = 10;

CYahooAddAllAuctionsTask::CYahooAddAllAuctionsTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader)
    : _searchQuery(searchQuery)
{
    _response = downloader.asyncDownload(CYahooUrlFactory::createTarget(_searchQuery));
}

void CYahooAddAllAuctionsTask::doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistory)
{
    CYahooAuctionInfo::List newAuctions;    
    CAsyncHttpsDownloader::ResponseType response = _response.get();
    CYahooAuctionInfoHtmlParser parser(response);
    while (parser.hasNext())
    {
        const CYahooAuctionInfo& info = parser.next();
        if (newAuctions.size() < maxNewAuctionsToWatch)
        {
            newAuctions.emplace_back(info);
        }
        watchHistory.insert(info.getId());
    }
    insertYahooAuctions(searchQueryResults, std::move(_searchQuery), std::move(newAuctions));
}

class CYahooAddNewAuctionsTask
{
public:
    typedef std::queue<CYahooAddNewAuctionsTask> Queue;

public:
    CYahooAddNewAuctionsTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader);
    
    void doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistory);
    
private:
    CYahooSearchQuery _searchQuery;
    CAsyncHttpsDownloader::FutureResponseType _response;
};

CYahooAddNewAuctionsTask::CYahooAddNewAuctionsTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader)
    : _searchQuery(searchQuery)
{
    _response = downloader.asyncDownload(CYahooUrlFactory::createTarget(_searchQuery));
}

void CYahooAddNewAuctionsTask::doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistory)
{                   
    CYahooAuctionInfo::List newAuctions;
    CAsyncHttpsDownloader::ResponseType response = _response.get();
    CYahooAuctionInfoHtmlParser parser(response);
    while (parser.hasNext())
    {
        const CYahooAuctionInfo& auctionInfo = parser.next();
        auto it = watchHistory.find(auctionInfo.getId());
        if (it == watchHistory.end())
        {
            newAuctions.emplace_back(auctionInfo);
            watchHistory.insert(auctionInfo.getId());
        }
    }
    insertYahooAuctions(searchQueryResults, std::move(_searchQuery), std::move(newAuctions));
}

static void printWatchHistoryFile(const std::string& watchHistoryFileName, const WatchHistory& watchHistory)
{
    std::ofstream watchHistoryFile;
    watchHistoryFile.exceptions(std::ios::failbit | std::ios::badbit);
    watchHistoryFile.open(watchHistoryFileName);
    json::value watchHistoryValue = json::value_from(watchHistory);
    boost_ext::pretty_print(watchHistoryFile, watchHistoryValue);
}

static std::string createHtmlPath(const std::string& queryFileName, std::string_view prefix)
{
    posix_time::ptime time = posix_time::second_clock::local_time();
    posix_time::time_facet* facet = new posix_time::time_facet("%d-%m-%Y_%H-%M-%S");
    
    
    std::string queryFileStem = std::filesystem::path(queryFileName).stem();
    std::stringstream stream;
    stream.imbue(std::locale(stream.getloc(), facet));
    stream << DATA_DIR << prefix << "_" << queryFileStem << "_" << time << ".html";

    return stream.str();
}

static void renderHtmlFile(Template& htmlTemplate, const std::string& queryFileName, std::string_view prefix)
{
    std::ofstream htmlFile;
    htmlFile.exceptions(std::ios::failbit | std::ios::badbit);
    htmlFile.open(createHtmlPath(queryFileName, prefix));
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
        
        renderHtmlFile(htmlTemplate, keywordsFileName, "yahooAuctions");
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

static void createYahooAllAuctionsHtml(const std::string& keywordsFileName, const std::string& watchHistoryFileName)
{
    asio::io_context ioContext;
    CAsyncHttpsDownloader downloader(ioContext, CYahooUrlFactory::createHost(), CYahooUrlFactory::createPort());
    CYahooAddAllAuctionsTask::Queue tasks;
    CYahooKeywordsFileSearchQueryParser parser(keywordsFileName);
    while (parser.hasNext())
    {
        tasks.emplace(parser.next(), downloader);
    }

    CThreadList threads;
    threads.run(ioContext);
    
    YahooSearchQueryResults searchQueryResults;
    WatchHistory watchHistoryNow;

    doTasks(tasks, searchQueryResults, watchHistoryNow);
    threads.join();

    printWatchHistoryFile(watchHistoryFileName, watchHistoryNow);   
    createYahooHtmlFile(searchQueryResults, keywordsFileName);
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

static void createYahooNewAuctionsHtml(const std::string& keywordsFileName, const std::string& watchHistoryFileName)
{          
    asio::io_context ioContext;
    CAsyncHttpsDownloader downloader(ioContext, CYahooUrlFactory::createHost(), CYahooUrlFactory::createPort());
    CYahooAddNewAuctionsTask::Queue tasks;
    CYahooKeywordsFileSearchQueryParser searchQueryParser(keywordsFileName);
    while (searchQueryParser.hasNext())
    {
        tasks.emplace(searchQueryParser.next(), downloader);        
    }    
    
    CThreadList threads;
    threads.run(ioContext);
    
    YahooSearchQueryResults searchQueryResults;
    WatchHistory watchHistory = getWatchHistory(watchHistoryFileName);
   
    doTasks(tasks, searchQueryResults, watchHistory);    
    threads.join();

    printWatchHistoryFile(watchHistoryFileName, watchHistory);
    createYahooHtmlFile(searchQueryResults, keywordsFileName);
}

static bool parseFileName(std::string_view argName, std::string_view arg, std::string& fileName)
{
    if (arg.rfind(argName, 0) == 0)
    {
        std::string_view argFileName = arg.substr(argName.size());
        std::filesystem::path argFilePath(argFileName);
        fileName.clear();
        if (!argFilePath.has_parent_path())
        {
            fileName += DATA_DIR;
        }
        fileName += argFileName;
        return true;
    }
    return false;
}

static void getInputFileNames(int argCount, char** argValues, std::string& yahooKeywordsFileName, std::string& diskunionItemsFileName)
{
    bool hasYahooKeywordsFileName = false;
    bool hasDiskunionItemsFileName = false;
    
    for (int i = 1; i < argCount; ++i)
    {
        hasYahooKeywordsFileName = hasYahooKeywordsFileName || parseFileName("-yahoo:", argValues[i], yahooKeywordsFileName);
        hasDiskunionItemsFileName = hasDiskunionItemsFileName || parseFileName("-diskunion:", argValues[i], diskunionItemsFileName);
    }
    
    if (!hasYahooKeywordsFileName)
    {
        yahooKeywordsFileName += DATA_DIR;
        yahooKeywordsFileName += "keywords.txt";
    }

    if (!hasDiskunionItemsFileName)
    {
        diskunionItemsFileName += DATA_DIR;
        diskunionItemsFileName += "diskunion_items.txt";
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

static void watchYahooAuctions(const std::string& keywordsFileName, const std::string& watchHistoryFileName)
{
    if (std::filesystem::exists(watchHistoryFileName))
    {
        createYahooNewAuctionsHtml(keywordsFileName, watchHistoryFileName);
    }
    else
    {
        createYahooAllAuctionsHtml(keywordsFileName, watchHistoryFileName);
    }
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
            
            itemQueryNode.set("itemImageLink", itemDescription.getImageUrl());
            itemQueryNode.set("itemLink", CDiskunionUrlFactory::createUrl(itemQuery));
            itemQueryNode.set("itemQueryDescription", itemQuery.getName());
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

        renderHtmlFile(htmlTemplate, itemsFileName, "diskunionItems");
    }
}

class CDiskunionAddAllItemsTask
{
public:
    typedef std::queue<CDiskunionAddAllItemsTask> Queue;

public:
    CDiskunionAddAllItemsTask(const CDiskunionItemQuery& itemQuery, CWebEngineDownloader& downloader);
    
    void doTask(DiskunionItemQueryResults& itemQueryResults, WatchHistory& watchHistory);
    
private:
    CDiskunionItemQuery _itemQuery;
    CWebEngineDownloader::FutureResponseType _responsePath;
};

CDiskunionAddAllItemsTask::CDiskunionAddAllItemsTask(const CDiskunionItemQuery& itemQuery, CWebEngineDownloader& downloader)
    : _itemQuery(itemQuery)
{
    _responsePath = downloader.addDownload(CDiskunionUrlFactory::createUrl(itemQuery));
}

void CDiskunionAddAllItemsTask::doTask(DiskunionItemQueryResults& itemQueryResults, WatchHistory& watchHistory)
{
    std::ifstream responseFile;
    responseFile.exceptions(std::ios::failbit | std::ios::badbit);
    responseFile.open(_responsePath.get());
    
    std::stringstream response;
    std::copy(std::istreambuf_iterator<char>(responseFile), std::istreambuf_iterator<char>(), std::ostreambuf_iterator(response));
    
    CDiskunionItemInfoHtmlParser parser(response);
    CDiskunionItemInfo itemInfo(parser);
    while (parser.hasNextUsedItem())
    {
        const CDiskunionUsedItemInfo& usedItemInfo = parser.nextUsedItem();
        itemInfo.getUsedItems().emplace_back(usedItemInfo);
        watchHistory.insert(usedItemInfo.getId());
    }
    insertDiskunionItems(itemQueryResults, std::move(_itemQuery), std::move(itemInfo));
}

static void prepareDiskunionDownloadsDir()
{
    if (std::filesystem::exists(DISKUNION_DOWNLOADS_DIR))
    {
        std::filesystem::remove_all(DISKUNION_DOWNLOADS_DIR);
    }
    if (!std::filesystem::create_directory(DISKUNION_DOWNLOADS_DIR))
    {
        std::string message = "Can't create download dir ";
        message.append(DISKUNION_DOWNLOADS_DIR);
        throw std::runtime_error(message);
    }
}

static void createAllDiskunionItemsHtml(const std::string& itemsFileName, const std::string& watchHistoryFileName)
{
    CDiskunionAddAllItemsTask::Queue tasks;
    CWebEngineDownloader downloader;
    CDiskunionFileItemQueryParser parser(itemsFileName);
    while (parser.hasNext())
    {
        tasks.emplace(parser.next(), downloader);
    }
    prepareDiskunionDownloadsDir();
    downloader.setDownloadDir(DISKUNION_DOWNLOADS_DIR);
    downloader.start();

    DiskunionItemQueryResults itemQueryResults;
    WatchHistory watchHistory;    
    doTasks(tasks, itemQueryResults, watchHistory);
    printWatchHistoryFile(watchHistoryFileName, watchHistory);
    createDiskunionHtmlFile(itemQueryResults, itemsFileName);    
}

static void watchDiskunionItems(const std::string& keywordsFileName, const std::string& watchHistoryFileName)
{
    createAllDiskunionItemsHtml(keywordsFileName, watchHistoryFileName);
}

static void testDiskunion()
{
    std::ifstream stream("./XAT-1245284991.html");
    std::stringstream response;
    
    std::copy(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>(), std::ostreambuf_iterator(response));
    
    CDiskunionItemInfoHtmlParser parser(response);
    CDiskunionItemInfo itemInfo(parser);
    while (parser.hasNextUsedItem())
    {
        const CDiskunionUsedItemInfo& usedItemInfo = parser.nextUsedItem();
        itemInfo.getUsedItems().emplace_back(usedItemInfo);
    }
    
    std::ofstream streamOut("./diskunion_out.txt");
    streamOut << "EAN - " << itemInfo.getDescription().getBarcode() << std::endl;
    streamOut << "catalog number - " << itemInfo.getDescription().getCatalogNumber() << std::endl;
    streamOut << "country - " << itemInfo.getDescription().getCountry() << std::endl;
    streamOut << "format - " << itemInfo.getDescription().getFormat() << std::endl;
    streamOut << "label - " << itemInfo.getDescription().getLabel() << std::endl;
    streamOut << "release year - " << itemInfo.getDescription().getReleaseYear() << std::endl;
    streamOut << std::endl;
    
    for (const CDiskunionUsedItemInfo& usedItemInfo : itemInfo.getUsedItems())
    {
        streamOut << "id - " << usedItemInfo.getId() << std::endl;
        streamOut << "price - " << usedItemInfo.getPriceJpy() << std::endl;
        for (const std::string& description : usedItemInfo.getDescription())
        {
            streamOut << description << std::endl;
        }
        streamOut << std::endl;
    }
}

static void mergeWatchHistories()
{
    WatchHistory watchHistory1 = getWatchHistory("./data/watchHistory_keywords.json");
    WatchHistory watchHistory2 = getWatchHistory("./data/watchHistory_keywords_music.json");
    
    watchHistory1.insert(watchHistory2.begin(), watchHistory2.end());
    
    printWatchHistoryFile("./data/watchHistory_yahoo.json", watchHistory1);
}

int main(int argCount, char** argValues)
{    
    try
    {
        std::string yahooKeywordsFileName;
        std::string diskunionItemsFileName;
        getInputFileNames(argCount, argValues, yahooKeywordsFileName, diskunionItemsFileName);
        std::string watchHistoryYahooFileName = getWatchHistoryFileName(yahooKeywordsFileName, "yahoo");
        std::string watchHistoryDiskunionFileName = getWatchHistoryFileName(diskunionItemsFileName, "diskunion");
        
        watchYahooAuctions(yahooKeywordsFileName, watchHistoryYahooFileName);
        //watchDiskunionItems(diskunionItemsFileName, watchHistoryDiskunionFileName);
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
