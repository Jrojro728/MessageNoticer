// pch.h —Precompiled header
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
#include <ctime>
#include <cstring>
#include <cstdint>
#include <csignal>
#include <mutex>
#include <queue>
#include <functional>
#include <deque>
#include <algorithm>

// Platform-specific socket headers
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

// Third-party libraries
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <json/json.h>
#include <log4cplus/log4cplus.h>
#include <argh.h>

namespace uuid = ::boost::uuids;
using std::string;
using namespace log4cplus;

#endif //PCH_H