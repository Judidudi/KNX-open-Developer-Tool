cmake_minimum_required(VERSION 3.21)

include(cmake/version.cmake)
project(KNXOpenDeveloperTool VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Xml Test LinguistTools)

include(FetchContent)

# miniz — ZIP read/write with STORE + DEFLATE support
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        2.2.0
    GIT_SHALLOW    TRUE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(miniz)

add_subdirectory(src)
add_subdirectory(app)

enable_testing()
add_subdirectory(tests)
