// MessageNoticerServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "HandshakePacket.h"
using std::cout, std::endl, std::cerr;

int main()
{
	// 初始化网络
    InitNetwork();
    SOCKET sListen;
    std::vector<Client> ClientList;
    uuid::random_generator UUIDGenerator;
	Json::Reader Reader;
	Json::Value Root;
    
	// 创建监听套接字
    CreateSocket(sListen, "12306", NULL);
    
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sListen, &readset);
    while (true)
    {
        for (auto& i : ClientList)
            cout << "{ " << i.GetClientID() << "; " << i.GetReadableClientName() << "};" << endl;

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
                if (readset.fd_count > FD_SETSIZE) 
                    cerr << "max 64 clients for now." << endl;

                //往集合中添加客户端套接字
                FD_SET(c, &readset);
                cout << c << "try to login." << endl;
            }
            else {
                //处理已关闭的客户端
                if (ret == SOCKET_ERROR || ret == 0) {
                    closesocket(sSelected);
                    FD_CLR(sSelected, &readset);
                    cout << sSelected << "logged off." << endl;
                    if (ClientList.size() != 0)
                        ClientList.erase(std::find(ClientList.begin(), ClientList.end(), Client(sSelected)));
                }
                // 接收客户端的数据
                char* buf = new char[2048];
                try {
                    if (!Reader.parse(Packet::PacketFromNetworkRecv(sSelected).GetData(), Root, false))
                    {
                        cerr << "Request parse failed!\n";
                        HandshakeErrorPacket("Invalid handshake request.").Send(sSelected);
                        break;
                    }
                    if (strcmp(Root["fastmessage"].asCString(), "Hello from client!"))
                    {
                        cerr << "Request parse failed!\n";
                        HandshakeErrorPacket("Invalid handshake request.").Send(sSelected);
                        break;
                    }

                    // 给客户端发送信息
                    HandshakeInfoPacket("bla", "b1", 64, ClientList.size(), 1, Online).Send(sSelected);
                    // 客户端确认
                    if (!Reader.parse(Packet::PacketFromNetworkRecv(sSelected).GetData(), Root, false))
                    {
                        cerr << "Ack parse failed!\n";
                        HandshakeErrorPacket("Invalid handshake ack.").Send(sSelected);
                        break;
                    }
                    if (!(Root["status"].asUInt() == Ok))
                    {
                        cerr << "Client error!\n";
                        HandshakeErrorPacket().Send(sSelected);
                        break;
                    }
                }
                catch (ClientSocketClosedExpection&) {
                    closesocket(sSelected);
                    FD_CLR(sSelected, &readset);
                    cout << sSelected << "logged off." << endl;
                    if (ClientList.size() != 0)
                        ClientList.erase(std::find(ClientList.begin(), ClientList.end(), Client(sSelected)));
                    break;
				}

				HandshakeSuccessPacket().Send(sSelected);
                ClientList.push_back(Client(sSelected, UUIDGenerator(), Root["name"].asString()));
                delete[] buf;
            }
        }
    }
}