cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)

project(japanwatchlist)

add_subdirectory(NLTemplate)
add_executable(japanwatchlist
    main.cpp
    async_https_downloader.cpp
    diskunion_item_info.cpp
    diskunion_item_query.cpp
    diskunion_url_factory.cpp
    html_parser.cpp
    json_pretty_print.cpp
    web_engine_downloader.cpp
    yahoo_auction_info.cpp
    yahoo_search_method.cpp
    yahoo_search_query.cpp
    yahoo_url_factory.cpp)
target_link_libraries(japanwatchlist
    webkitgtk-6.0
    gobject-2.0
    gio-2.0
    glib-2.0
    gtk-4
    boost_json
    NLTemplate
    pthread
    ssl
    crypto)
target_include_directories(japanwatchlist PUBLIC "${PROJECT_SOURCE_DIR}/NLTemplate")

install(TARGETS japanwatchlist RUNTIME DESTINATION bin)
