// MessageNoticerServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "Network.h"
using std::cout, std::endl;

int main()
{
    InitNetwork();
    SOCKET sListen;
    CreateSocket(sListen, "12306");
    ListenSocket(sListen);
    
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sListen, &readset);
    while (true)
    {
        fd_set tmpset; // 定义一个临时的集合
        FD_ZERO(&tmpset); // 初始化集合
        tmpset = readset; // 每次循环都是所有的套接字

        // 利用select选择出集合中可以读写的多个套接字，有点像筛选
        int ret = select(0, &tmpset, NULL, NULL, NULL);//最后一个参数为NULL，一直等待，直到有数据过来
        if (ret == SOCKET_ERROR) {
            continue;
        }

        // 成功筛选出来的tmpSet可以发送或者接收的socket
        for (size_t i = 0; i < tmpset.fd_count; ++i) {
            //获取到套接字
            SOCKET sSelected = tmpset.fd_array[i];

            // 接收到客户端的链接
            if (sSelected == sListen) {
                SOCKET c = accept(sSelected, NULL, NULL);
                // fd_set集合最大值为64
                if (readset.fd_count < FD_SETSIZE) {
                    //往集合中添加客户端套接字
                    FD_SET(c, &readset);
                    cout << c << "logged in." << endl;

                    // 给客户端发送欢迎
                    char buf[100];
                    memset(buf, 0, 100);
                    sprintf_s(buf, "hello from server %lld", c);
                    Send(c, buf);
                }
                else {
                    cout << "max 64 clients for now." << endl;
                }
            }
            else {
                // 接收客户端的数据
                char* buf = nullptr;
                ret = Recv(sSelected, buf);
                if (ret == SOCKET_ERROR || ret == 0) {
                    closesocket(sSelected);
                    FD_CLR(sSelected, &readset);
                    cout << sSelected << "logged off." << endl;
                }
                else {
                    cout << sSelected << " sent " << buf << endl;
                }
            }

        }
    }
}