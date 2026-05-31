// MessageNoticerClient.cpp ¡ªConnects to a MessageNoticerServer.
#include "pch.h"
#include "Network.h"
#include "Logger.h"
#include "ClientProcess.h"

int main()
{
	log4cplus::Initializer init;
	Logger logger = GetLogger("main");

	bool isInitReady = false;
	InitNetwork();
	SOCKET sServer = INVALID_SOCKET;
	do {
		if (CreateSocket(sServer, "12306", "127.0.0.1") != 0) {
			LOG_FATAL(logger, "Failed to connect server, retrying in 2s...");
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}
		isInitReady = true;
	} while (!isInitReady);

	for (;;)
	{
		try
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			HandshakeProcess(sServer);
			NormalProcess(sServer);
			break;
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
	}
	CloseSocket(sServer);
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}



