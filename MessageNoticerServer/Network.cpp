#include "pch.h"
#include "Network.h"
#include "Logger.h"

using std::cerr, std::endl;
//检测数据暂存区
char* TempDataBuffer = new char[2097152];
Logger NetworkLogger = GetLogger("Network");

int InitNetwork()
{
    // 初始化winsock的环境
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger,"WSAStartup error:" << GetLastError());
        return 1;
    }
    return 0;
}

int CreateSocket(SOCKET& s, const char* port, const char* address)
{
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    // 0.解算地址
    if (getaddrinfo(address, port, &hints, &result))
    {
        LOG_FATAL(NetworkLogger, "getaddrinfo failed:" << GetLastError());
        WSACleanup();
        return 1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        // 1.创建监听套接字
        s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (s == INVALID_SOCKET)
        {
            LOG_FATAL(NetworkLogger, "socket error:" << GetLastError());
            WSACleanup();
            continue;
        }

#ifdef CLIENT_APP
        // 2.连接到服务器
        if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
            LOG_FATAL(NetworkLogger, "connect error:" << GetLastError());
            closesocket(s);
            WSACleanup();
            s = INVALID_SOCKET;
            continue;
        }
        break;
#endif // CLIENT_APP

    }

#ifdef SERVER_APP
    // 2.绑定到ip与端口
    if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "bind error:" << GetLastError());
        freeaddrinfo(result);
        closesocket(s);
        WSACleanup();
        s = INVALID_SOCKET;
        return 1;
    }

    // 3.监听套接字
    if (listen(s, SOMAXCONN) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "listen error:" << GetLastError());
        closesocket(s);
        WSACleanup();
        s = INVALID_SOCKET;
        return 1;
    }
#endif // SERVER_APP

    freeaddrinfo(result);
    if (s == INVALID_SOCKET) {
        LOG_FATAL(NetworkLogger, "Unable to create socket!" << WSAGetLastError());
        WSACleanup();
        return 1;
    }
    return 0;
}

int Recv(SOCKET& s, char*& DataBuffer)
{
    int result = 0;
    memset(TempDataBuffer, 0, 2097152);
    result = recv(s, TempDataBuffer, 2097152, MSG_PEEK);
    DataBuffer = new char[result + 1];
    memset(DataBuffer, 0, result + 1);
    result = recv(s, DataBuffer, result + 1, 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "sent: " << DataBuffer);
#endif // _DEBUG

#ifdef SERVER_APP
    if (result == SOCKET_ERROR || result == 0)
		throw ClientSocketClosedException();
#endif
    return result;
}

int Send(SOCKET& s, const char* DataBuffer, int Size)
{
    send(s, DataBuffer, Size, 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "recv: " << DataBuffer);
#endif // _DEBUG

    return 0;
}

int Send(SOCKET& s, const char* DataBuffer)
{
    send(s, DataBuffer, strlen(DataBuffer), 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "recv: " << DataBuffer);
#endif // _DEBUG

    return 0;
}

void EndianSwap(char* pData, unsigned int startIndex, unsigned int length)
{
    unsigned int i, cnt, end, start;
    cnt = length / 2;
    start = startIndex;
    end = startIndex + length - 1;
    char tmp;
    for (i = 0; i < cnt; i++)
    {
        tmp = pData[start + i];
        pData[start + i] = pData[end - i];
        pData[end - i] = tmp;
    }
}
