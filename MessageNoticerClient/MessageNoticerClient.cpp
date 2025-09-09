// MessageNoticerClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "Network.h"
#include "HandshakePacket.h"

int main()
{
    start:
    InitNetwork();
    SOCKET sServer;
    CreateSocket(sServer, "12306", "127.0.0.1");
	Json::Reader Reader;
	Json::Value Root;

	HandshakePacket(1, "TestClient").Send(sServer);
    char* cstr = nullptr;
    try
    {
        cstr = const_cast<char*>(Packet::PacketFromNetworkRecv(sServer).GetData());
        if (Reader.parse(cstr, Root, false))
        {
            if (Root["info"]["maxuser"].asInt() - Root["info"]["useronline"].asInt() < 1)
                HandshakeAckPacket("no", No).Send(sServer);
			HandshakeAckPacket("whhhhhh", Ok).Send(sServer);
            if (Packet::PacketFromNetworkRecv(sServer).GetPacketID() == HandshakeSuccess)
            {
                std::cout << "Connected to server: " << Root["info"]["name"].asString() << " Version: " << Root["info"]["version"].asString() << " Protocol: " << Root["info"]["protocol"].asUInt() << std::endl;
            }
        }
        else
        {
            if (Packet(cstr).GetPacketID() == PacketType::HandshakeError)
            {
                std::cerr << "Handshake error from server: " << Packet(cstr).GetData() << std::endl;
            }
            else
				std::cerr << "Invalid handshake response from server." << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error receiving packet: " << e.what() << std::endl;
        closesocket(sServer);
		goto start;
	}
}
