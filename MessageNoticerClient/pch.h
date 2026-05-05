// pch.h ¡ªPrecompiled header
#pragma once

#ifndef PCH_H
#define PCH_H

#ifndef CLIENT_APP
#define CLIENT_APP
#endif // !CLIENT_APP

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// Standard C++ library
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <memory>
#include <thread>
#include <chrono>
#include <format>
#include <cstring>
#include <cstdint>

// Platform-specific socket headers
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#endif

// Third-party libraries
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <json/json.h>
#include <log4cplus/log4cplus.h>

namespace uuid = ::boost::uuids;
using std::string;
using namespace log4cplus;

#endif //PCH_H

