#include "pch.h"
#include "Network.h"
#include "Logger.h"

using std::cerr, std::endl;
// Receive buffer (dynamically grown on demand)
static std::vector<char> TempDataBuffer(65536);
Logger NetworkLogger = GetLogger("Network");

int InitNetwork()
{
#ifdef _WIN32
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "WSAStartup error: " << GetSocketError());
        return 1;
    }
#else
    // SIGPIPE would kill the process when writing to a closed socket.
    // Ignore it and handle EPIPE via send() return value instead.
    signal(SIGPIPE, SIG_IGN);
#endif
    return 0;
}

int CreateSocket(SOCKET& s, const char* port, const char* address)
{
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    memset(&hints, 0, sizeof(hints));
    int connectErrno = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(address, port, &hints, &result))
    {
        LOG_FATAL(NetworkLogger, "getaddrinfo failed: " << GetSocketError());
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (s == INVALID_SOCKET)
        {
            LOG_FATAL(NetworkLogger, "socket error: " << GetSocketError());
#ifdef _WIN32
            WSACleanup();
#endif
            continue;
        }

#ifdef CLIENT_APP
        if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
            int err = errno;  // save before CloseSocket/WSACleanup clear it
            LOG_FATAL(NetworkLogger, "connect error: " << err);
            CloseSocket(s);
#ifdef _WIN32
            WSACleanup();
#endif
            connectErrno = err;
            s = INVALID_SOCKET;
            continue;
        }
        break;
#endif // CLIENT_APP
    }

#ifdef SERVER_APP
    // Allow reusing the address immediately after restart
    int optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "bind error: " << GetSocketError());
        freeaddrinfo(result);
        CloseSocket(s);
#ifdef _WIN32
        WSACleanup();
#endif
        s = INVALID_SOCKET;
        return 1;
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "listen error: " << GetSocketError());
        CloseSocket(s);
#ifdef _WIN32
        WSACleanup();
#endif
        s = INVALID_SOCKET;
        return 1;
    }
#endif // SERVER_APP

    int createErr = (s == INVALID_SOCKET) ? (connectErrno != 0 ? connectErrno : errno) : 0;
    freeaddrinfo(result);
    if (s == INVALID_SOCKET) {
        LOG_FATAL(NetworkLogger, "Unable to create socket! " << createErr);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    return 0;
}

int Recv(SOCKET& s, std::vector<char>& DataBuffer)
{
    static std::vector<char> peekBuffer(65536);

    int result = recv(s, peekBuffer.data(), (int)peekBuffer.size(), MSG_PEEK);

    if (result == SOCKET_ERROR)
    {
#ifdef SERVER_APP
        DataBuffer.clear();
        throw ClientSocketClosedException();
#else
        return SOCKET_ERROR;
#endif
    }

    if (result == 0)
    {
        DataBuffer.clear();
#ifdef SERVER_APP
        throw ClientSocketClosedException();
#else
        return 0;
#endif
    }

    if (result > (int)peekBuffer.size())
        peekBuffer.resize(result + 65536);

    DataBuffer.resize(result);
    result = recv(s, DataBuffer.data(), result, 0);

    if (result == SOCKET_ERROR || result == 0)
    {
        DataBuffer.clear();
#ifdef SERVER_APP
        throw ClientSocketClosedException();
#endif
        return result;
    }

#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "sent: " << strToHexString(DataBuffer.data(), result));
#endif

    return result;
}

int Send(SOCKET& s, const char* DataBuffer, int Size)
{
    send(s, DataBuffer, Size, 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "recv: " << strToHexString(DataBuffer, Size));
#endif
    return 0;
}

int Send(SOCKET& s, const char* DataBuffer)
{
    send(s, DataBuffer, strlen(DataBuffer), 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "recv: " << strToHexString(DataBuffer, strlen(DataBuffer)));
#endif
    return 0;
}

void EndianSwap(char* pData, unsigned int startIndex, unsigned int length)
{
    unsigned int cnt = length / 2;
    unsigned int start = startIndex;
    unsigned int end = startIndex + length - 1;
    for (unsigned int i = 0; i < cnt; i++)
    {
        char tmp = pData[start + i];
        pData[start + i] = pData[end - i];
        pData[end - i] = tmp;
    }
}


