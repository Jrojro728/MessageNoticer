// Network.h —Platform-agnostic socket abstraction.
#pragma once
#include "pch.h"
#include "Logger.h"

// ---------- Platform abstraction for basic socket types ----------

#ifdef _WIN32
// Windows: use winsock types directly (SOCKET is unsigned)
static inline void CloseSocket(SOCKET s) { closesocket(s); }
static inline int GetSocketError() { return WSAGetLastError(); }
#else
// POSIX: SOCKET is int, close() instead of closesocket()
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
static inline void CloseSocket(SOCKET s) { close(s); }
static inline int GetSocketError() { return errno; }
#endif

// ---------- Exception ----------

class ClientSocketClosedException : public std::exception
{
public:
	const char* what() const noexcept override {
		return "Client socket closed.";
	}
};

// ---------- Function declarations ----------

/// Initialize the network subsystem.
/// On Windows this calls WSAStartup; on POSIX it ignores SIGPIPE.
/// Returns 0 on success, 1 on failure.
int InitNetwork();

/// Create a listening (server) or connecting (client) socket.
/// port: port number as string (e.g. "12306").
/// address: IP to bind/connect to, or NULL for INADDR_ANY (server).
int CreateSocket(SOCKET& s, const char* port, const char* address);

/// Receive data from a socket.
/// Fills DataBuffer (RAII, no manual cleanup needed).
/// Returns bytes received, 0 for graceful close, SOCKET_ERROR on failure.
/// On the server side, throws ClientSocketClosedException on close/error.
int Recv(SOCKET& s, std::vector<char>& DataBuffer);

/// Send a fixed-size buffer.
int Send(SOCKET& s, const char* DataBuffer, int Size);

/// Send a null-terminated string.
int Send(SOCKET& s, const char* DataBuffer);

/// Reverse byte order of a range
void EndianSwap(char* pData, unsigned int startIndex, unsigned int length);
