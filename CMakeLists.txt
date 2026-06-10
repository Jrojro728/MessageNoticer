cmake_minimum_required(VERSION 3.16)
project(MessageNoticer VERSION 0.1.0.3 LANGUAGES CXX)

# ---- Output directory ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

# ---- C++ Standard ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Compile definitions ----
# Define _DEBUG when CMAKE_BUILD_TYPE=Debug (MSVC-compatible)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
endif()

# ---- Dependencies ----
set(Boost_NO_SYSTEM_PATHS ON)   # use vcpkg's Boost, not the system one
find_package(Boost REQUIRED COMPONENTS uuid)              # uuid is header-only
find_package(jsoncpp REQUIRED)
find_package(log4cplus REQUIRED)
find_package(argh CONFIG REQUIRED)

# ---- Platform-specific ----
if(WIN32)
    set(PLATFORM_LIBS ws2_32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
else()
    set(PLATFORM_LIBS pthread)
    find_package(Threads REQUIRED)
endif()

# ---- Server target ----
set(SERVER_SOURCES
    MessageNoticerServer/MessageNoticerServer.cpp
    MessageNoticerServer/Logger.cpp
    MessageNoticerServer/Message.cpp
    MessageNoticerServer/Packet.cpp
    MessageNoticerServer/Network.cpp
    MessageNoticerServer/ServerProcess.cpp
)

add_executable(MessageNoticerServer ${SERVER_SOURCES})

target_include_directories(MessageNoticerServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/MessageNoticerServer
    ${Boost_INCLUDE_DIRS}
    ${jsoncpp_INCLUDE_DIRS}
    ${log4cplus_INCLUDE_DIRS}
)

target_link_libraries(MessageNoticerServer PRIVATE
    Boost::uuid
    JsonCpp::JsonCpp
    log4cplus::log4cplus
    argh
    ${PLATFORM_LIBS}
)

target_compile_definitions(MessageNoticerServer PRIVATE SERVER_APP)

if(MSVC)
    target_precompile_headers(MessageNoticerServer  PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageNoticerServer/pch.h
    )
endif()

# ---- Client target ----
set(CLIENT_SOURCES
    MessageNoticerClient/MessageNoticerClient.cpp
    MessageNoticerClient/Logger.cpp
    MessageNoticerClient/Message.cpp
    MessageNoticerClient/Packet.cpp
    MessageNoticerClient/Network.cpp
    MessageNoticerClient/ClientProcess.cpp
)

add_executable(MessageNoticerClient ${CLIENT_SOURCES})

target_include_directories(MessageNoticerClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/MessageNoticerClient
    ${Boost_INCLUDE_DIRS}
    ${jsoncpp_INCLUDE_DIRS}
    ${log4cplus_INCLUDE_DIRS}
)

target_link_libraries(MessageNoticerClient PRIVATE
    Boost::uuid
    JsonCpp::JsonCpp
    log4cplus::log4cplus
    ${PLATFORM_LIBS}
)

target_compile_definitions(MessageNoticerClient PRIVATE CLIENT_APP)

if(MSVC)
    target_precompile_headers(MessageNoticerClient PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageNoticerClient/pch.h
    )
endif()

# ---- Install ----
install(TARGETS MessageNoticerServer MessageNoticerClient RUNTIME DESTINATION bin)
