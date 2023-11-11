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
    if (!itemInfo.getUsedItems().empty())
    {
        itemQueryResults.emplace(std::move(itemQuery), std::move(itemInfo));
    }
}

class CAddNewQueriesTask
{
public:
    typedef std::queue<CAddNewQueriesTask> Queue;

public:
    CAddNewQueriesTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader);
    
    void doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistoryNow);
    
private:
    static int maxNewAuctionsToWatch;

private:
    CYahooSearchQuery _searchQuery;
    CAsyncHttpsDownloader::FutureResponseType _response;
};

int CAddNewQueriesTask::maxNewAuctionsToWatch = 10;

CAddNewQueriesTask::CAddNewQueriesTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader)
    : _searchQuery(searchQuery)
{
    _response = downloader.asyncDownload(CYahooUrlFactory::createTarget(_searchQuery));
}

void CAddNewQueriesTask::doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistoryNow)
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
        watchHistoryNow.insert(info.getId());
    }
    insertYahooAuctions(searchQueryResults, std::move(_searchQuery), std::move(newAuctions));
}

class CWatchNewAuctionsTask
{
public:
    typedef std::queue<CWatchNewAuctionsTask> Queue;

public:
    CWatchNewAuctionsTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader);
    
    void doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistoryNow, const WatchHistory& watchHistory);
    
private:
    CYahooSearchQuery _searchQuery;
    CAsyncHttpsDownloader::FutureResponseType _response;
};

CWatchNewAuctionsTask::CWatchNewAuctionsTask(const CYahooSearchQuery& searchQuery, CAsyncHttpsDownloader& downloader)
    : _searchQuery(searchQuery)
{
    _response = downloader.asyncDownload(CYahooUrlFactory::createTarget(_searchQuery));
}

void CWatchNewAuctionsTask::doTask(YahooSearchQueryResults& searchQueryResults, WatchHistory& watchHistoryNow, const WatchHistory& watchHistory)
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
        }
        watchHistoryNow.insert(auctionInfo.getId());
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

static std::string createHtmlPath(const std::string& keywordsFileName)
{
    posix_time::ptime time = posix_time::second_clock::local_time();
    posix_time::time_facet* facet = new posix_time::time_facet("%d-%m-%Y_%H-%M-%S");
    
    std::string keywordsStem = std::filesystem::path(keywordsFileName).stem();
    std::stringstream stream;
    stream.imbue(std::locale(stream.getloc(), facet));
    stream << "../data/newAuctions_" << keywordsStem << "_" << time << ".html";
    
    return stream.str();
}

static void createHtmlFile(const YahooSearchQueryResults& searchQueryResults, const std::string& keywordsFileName)
{
    if (!searchQueryResults.empty())
    {
        LoaderFile loader;
        Template htmlTemplate(loader);
        htmlTemplate.load("../data/templateHtml.txt");

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
        
        std::ofstream htmlFile;
        htmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        htmlFile.open(createHtmlPath(keywordsFileName));
        if (htmlFile.is_open())
        {
            htmlTemplate.render(htmlFile);
        }
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

static void createWatchHistoryFile(const std::string& keywordsFileName, const std::string& watchHistoryFileName)
{
    asio::io_context ioContext;
    CAsyncHttpsDownloader downloader(ioContext, CYahooUrlFactory::createHost(), CYahooUrlFactory::createPort());
    CAddNewQueriesTask::Queue tasks;
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
    createHtmlFile(searchQueryResults, keywordsFileName);
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

static void createNewAuctionsHtml(const std::string& keywordsFileName, const std::string& watchHistoryFileName)
{          
    asio::io_context ioContext;
    CAsyncHttpsDownloader downloader(ioContext, CYahooUrlFactory::createHost(), CYahooUrlFactory::createPort());
    CWatchNewAuctionsTask::Queue watchNewAuctionsTasks;
    CYahooKeywordsFileSearchQueryParser searchQueryParser(keywordsFileName);
    while (searchQueryParser.hasNext())
    {
        watchNewAuctionsTasks.emplace(searchQueryParser.next(), downloader);        
    }    
    
    CThreadList threads;
    threads.run(ioContext);
    
    YahooSearchQueryResults searchQueryResults;
    WatchHistory watchHistory = getWatchHistory(watchHistoryFileName);
    WatchHistory watchHistoryNow;
   
    doTasks(watchNewAuctionsTasks, searchQueryResults, watchHistoryNow, watchHistory);    
    threads.join();

    printWatchHistoryFile(watchHistoryFileName, watchHistoryNow);
    createHtmlFile(searchQueryResults, keywordsFileName);
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
            fileName += "../data/";
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
        yahooKeywordsFileName += "../data/keywords.txt";
    }

    if (!hasDiskunionItemsFileName)
    {
        diskunionItemsFileName += "../data/diskunion_items.txt";
    }
}

static std::string getWatchHistoryFileName(std::filesystem::path inputFileName)
{
    std::string watchHistoryFileName;
    if (inputFileName.has_parent_path())
    {
        watchHistoryFileName += inputFileName.parent_path();
    }
    else
    {
        watchHistoryFileName += "../data";
    }
    watchHistoryFileName += "/watchHistory";
    if (inputFileName.has_stem())
    {
        watchHistoryFileName += "_";
        watchHistoryFileName += inputFileName.stem();
    }
    watchHistoryFileName += ".json";
    return watchHistoryFileName;
}

static void convertWatchHistoryFile(const std::string& watchHistoryFileName)
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
    
    json::value watchHistoryValue = parser.release();
    const json::array& watchHistory = watchHistoryValue.as_array();
    
    WatchHistory watchHistoryNow;
    
    for (const json::value& searchHistoryValue : watchHistory)
    {
        const json::object& searchHistory = searchHistoryValue.as_object();
        const json::array& auctionIds = searchHistory.at("onFirstPage").as_array();
        for (const json::value& auctionId : auctionIds)
        {
            watchHistoryNow.insert(json::value_to<std::string>(auctionId));
        }
    }
    watchHistoryFileInput.close();
    
    printWatchHistoryFile(watchHistoryFileName, watchHistoryNow);
}

static void createDiskunionWatchHistoryFile(const std::string& itemsFileName, const std::string& watchHistoryFileName)
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

int main(int argCount, char** argValues)
{    
    try
    {
        std::string yahooKeywordsFileName;
        std::string diskunionItemsFileName;
        getInputFileNames(argCount, argValues, yahooKeywordsFileName, diskunionItemsFileName);
        std::string watchHistoryYahooFileName = getWatchHistoryFileName(yahooKeywordsFileName);
        std::string watchHistoryDiskunionFileName = getWatchHistoryFileName(diskunionItemsFileName);
        
        //createDiskunionWatchHistoryFile(diskunionItemsFileName, watchHistoryDiskunionFileName);
        
        if (std::filesystem::exists(watchHistoryYahooFileName))
        {
            createNewAuctionsHtml(yahooKeywordsFileName, watchHistoryYahooFileName);
        }
        else
        {
            createWatchHistoryFile(yahooKeywordsFileName, watchHistoryYahooFileName);
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
