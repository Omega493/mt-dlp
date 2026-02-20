cmake_minimum_required (VERSION 3.16)

project("mt-dlp")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mt-dlp "src/main.cpp" "src/downloader.cpp")

# curl
find_package(CURL REQUIRED)
target_link_libraries(mt-dlp PRIVATE CURL::libcurl)

# FTXUI
find_package(ftxui CONFIG REQUIRED)
target_link_libraries(mt-dlp PRIVATE ftxui::dom ftxui::screen ftxui::component)

target_include_directories(mt-dlp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS mt-dlp DESTINATION bin)
