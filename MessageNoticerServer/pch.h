// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。
#pragma once

#ifndef PCH_H
#define PCH_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef SERVER_APP
#define SERVER_APP
#endif // !SERVER_APP

#define NOMINMAX

//std libs
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>

//Windows libs
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

//Other libs
#include <boost/uuid.hpp>
#include <json/json.h>

namespace uuid = ::boost::uuids;
using std::string;

#pragma comment(lib, "Ws2_32.lib")

#endif //PCH_H
