// MessageNoticerClient.cpp ¡ª Interactive TCP client.
#include "pch.h"
#include "Network.h"
#include "Logger.h"
#include "ClientProcess.h"
#include "NormalPacket.h"

volatile std::sig_atomic_t gRunning = 1;
static void OnSignal(int) { gRunning = 0; }

int main(int argc, char* argv[])
{
	//signal handlers
	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);

	//init logger
	log4cplus::Initializer init;
	Logger logger = GetLogger("main");

	//Random user name
	char *randStr = new char[12];
	srand((unsigned int)time(NULL));
	snprintf(randStr, 12, "USER%d", rand() % 100000);

	// Parse CLI args
	argh::parser cmdl(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
	std::string port = "12306";
	cmdl({ "-p", "--port" }) >> port;
	std::string ServerAddress = "127.0.0.1";
	cmdl({ "-a", "--address" }) >> ServerAddress;
	std::string ClientName = randStr;
	cmdl({ "-n", "--name" }) >> ClientName;
	std::string Version = "0.1.0.4";
	if (cmdl({ "-v", "--version" }))
		std::cout << Version;

	//Init network
	InitNetwork();
	SOCKET sServer = INVALID_SOCKET;

	// Connect (with retry)
	while (gRunning)
	{
		if (CreateSocket(sServer, port.c_str(), ServerAddress.c_str()) == 0)
			break;
		LOG_FATAL(logger, "Failed to connect server, retrying in 2s...");
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	if (!gRunning) { CloseSocket(sServer); return 1; }

	// Handshake
	try
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (HandshakeProcess(sServer, ClientName) != 0) {
			LOG_FATAL(logger, "Handshake failed.");
			CloseSocket(sServer);
#ifdef _WIN32
			WSACleanup();
#endif
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		LOG_FATAL(logger, "Handshake error: " << e.what());
		CloseSocket(sServer);
#ifdef _WIN32
		WSACleanup();
#endif
		return 1;
	}

	// Interactive event loop
	// Tell server to send us all messages, get client list
	WaitingMessagePacket(0).Send(sServer);
	GetClientListPacket(MessagePriority::Low, 0).Send(sServer);

	std::thread consoleThr(ConsoleThread);
	LOG_INFO(logger, "Interactive mode. Type /help for commands.");

	while (gRunning)
	{
		// Poll and dispatch console commands (non-blocking)
		std::string cmdLine = PollCommand();
		if (!cmdLine.empty())
			ProcessCommand(cmdLine, sServer);

		// Network I/O: select + receive + dispatch
		if (NormalProcess(sServer) != 0)
			break;  // disconnected or fatal error
	}

	// Cleanup
	gRunning = 0;
	consoleThr.join(); 
	CloseSocket(sServer);
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
