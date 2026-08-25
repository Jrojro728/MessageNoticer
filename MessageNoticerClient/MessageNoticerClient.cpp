// MessageNoticerClient.cpp ¡ª Interactive TCP client.
#include "pch.h"
#include "Network.h"
#include "Logger.h"
#include "ClientProcess.h"
#include "NormalPacket.h"
#include "Console.h"

volatile std::sig_atomic_t gRunning = 1;
extern volatile std::sig_atomic_t gDisconnected;
static void OnSignal(int) { gRunning = 0; }

int main(int argc, char* argv[])
{
	//signal handlers
	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);

	// Enable ANSI escape codes on Windows console
	if (!EnableVirtualTerminal()) {
		std::cerr << "Virtual terminal processing not supported.\n";
		std::cerr << "ANSI escape codes may not work correctly.\n";
	}

	//init logger
	log4cplus::Initializer init;
	Logger logger = GetLogger("main");

	//Random user name
	char *randStr = new char[12];
	srand((unsigned int)time(NULL));
	snprintf(randStr, 12, "USER%d", rand() % 100000);

	// Parse CLI args
	argh::parser cmdl(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
	std::string ServerPort = "12306";
	cmdl({ "-p", "--port" }) >> ServerPort;
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

RESTART:
	//Init interactive console thread
	std::thread consoleThr(ConsoleThread);

	// Connect (with retry)
	while (gRunning)
	{
		if (CreateSocket(sServer, ServerPort.c_str(), ServerAddress.c_str()) == 0)
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

	if (gDisconnected)
	{
		LOG_INFO(logger, CLR_YELLOW "Reconnect Success!" CLR_RESET);
	}
	gDisconnected = 0; // Reset disconnected flag
	LOG_INFO(logger, "Interactive mode. Type /help for commands.");

	while (gRunning)
	{
		int result = 0;
		// Poll and dispatch console commands (non-blocking)
		try
		{
			std::string cmdLine = PollCommand();
			if (!cmdLine.empty())
				ProcessCommand(cmdLine, sServer);
		}
		catch (RestartException& e)
		{
			LOG_INFO(logger, "Will attempt to reconnect to the server...");
			ServerAddress = e.ServerIP.value_or(ServerAddress);
			ServerPort = std::to_string(e.ServerPort.value_or(stoi(ServerPort)));
			consoleThr.detach();
			CloseSocket(sServer);
			LOG_INFO(logger, CLR_YELLOW "(Re)connecting to server..." CLR_RESET);
			goto RESTART;
		}

		// Network I/O: select + receive + dispatch
		if (!gDisconnected)
			result = NormalProcess(sServer);
		if (result != 0 && result != -1)
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
