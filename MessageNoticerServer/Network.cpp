#include "pch.h"
#include "Network.h"

using std::cerr, std::endl;
//检测数据暂存区
char* TempDataBuffer = new char[2097152];

int InitNetwork()
{
    // 初始化winsock的环境
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) == SOCKET_ERROR) 
    {
        cerr << "WSAStartup error:" << GetLastError() << endl;
        return 1;
    }
    return 0;
}

int CreateSocket(SOCKET& s, const char* port)
{
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    // 0.解算地址
    if (getaddrinfo(NULL, port, &hints, &result))
    {
        cerr << "getaddrinfo failed:" << GetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // 1.创建监听套接字
    s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET)
    {
        cerr << "socket error:" << GetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // 2.绑定到ip与端口
    if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
    {
        cerr << "bind error:" << GetLastError() << endl;
        freeaddrinfo(result);
        closesocket(s);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);
    return 0;
}

int ListenSocket(SOCKET& sListen)
{
    // 3.监听套接字
    if (listen(sListen, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "listen error:" << GetLastError() << endl;
        closesocket(sListen);
        return 1;
    }
    return 0;
}

int Recv(SOCKET& s, char*& DataBuffer)
{
    int result = 0;
    memset(TempDataBuffer, 0, 2097152);
    result = recv(s, TempDataBuffer, 2097152, MSG_PEEK);
    DataBuffer = new char[result+1];
    memset(DataBuffer, 0, result+1);
    result = recv(s, DataBuffer, result+1, 0);

    return result;
}

int Send(SOCKET& s, const char* DataBuffer)
{
    send(s, DataBuffer, strlen(DataBuffer), 0);
    return 0;
}
