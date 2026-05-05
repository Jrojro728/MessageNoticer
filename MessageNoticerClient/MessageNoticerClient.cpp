// MessageNoticerClient.cpp ¡ªConnects to a MessageNoticerServer.
#include "pch.h"
#include "Network.h"
#include "Logger.h"
#include "HandshakePacket.h"

int main()
{
	log4cplus::Initializer init;
	Logger logger = GetLogger("main");

	for (;;)
	{
		InitNetwork();
		SOCKET sServer = INVALID_SOCKET;
		if (CreateSocket(sServer, "12306", "120.79.245.194") != 0) {
			LOG_FATAL(logger, "Failed to connect server, retrying in 2s...");
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}

		Json::Reader Reader;
		Json::Value Root;

		char randStr[8];
		snprintf(randStr, sizeof(randStr), "%d", rand() % 100000);
		HandshakePacket(1, "TestClient").Send(sServer);

		try
		{
			Packet Received = Packet::PacketFromNetworkRecv(sServer);
			if (Received.GetPacketID() == PacketType::HandshakeError)
				LOG_FATAL(logger, "Handshake error from server: " << Received.GetRawData());

			if (!(Reader.parse(Received.GetData(), Root, false)))
				LOG_FATAL(logger, "Invalid handshake response from server.");

			if (Root["info"]["maxuser"].asInt() - Root["info"]["useronline"].asInt() < 1)
				HandshakeAckPacket("no", No).Send(sServer);

			HandshakeAckPacket(randStr, Ok).Send(sServer);

			if (Packet::PacketFromNetworkRecv(sServer).GetPacketID() == HandshakeSuccess)
			{
				LOG_INFO(logger, "Connected to server: "
					<< Root["info"]["name"].asString()
					<< " Version: " << Root["info"]["version"].asString()
					<< " Protocol: " << Root["info"]["protocol"].asUInt());
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));

		}
		catch (const std::exception& e)
		{
			LOG_FATAL(logger, "Error receiving packet: " << e.what());
			if (sServer != INVALID_SOCKET)
				CloseSocket(sServer);
#ifdef _WIN32
			WSACleanup();
#endif
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}

		CloseSocket(sServer);
#ifdef _WIN32
		WSACleanup();
#endif
	}
	return 0;
}



