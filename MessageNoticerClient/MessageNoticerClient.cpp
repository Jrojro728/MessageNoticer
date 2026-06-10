// MessageNoticerClient.cpp — Interactive TCP client.
#include "pch.h"
#include "Network.h"
#include "Logger.h"
#include "ClientProcess.h"
#include "NormalPacket.h"

volatile std::sig_atomic_t gRunning = 1;
static void OnSignal(int) { gRunning = 0; }

int main()
{
	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);

	log4cplus::Initializer init;
	Logger logger = GetLogger("main");

	InitNetwork();
	SOCKET sServer = INVALID_SOCKET;

	// ── Connect (with retry) ─────────────────────────────────────────
	while (gRunning)
	{
		if (CreateSocket(sServer, "12306", "127.0.0.1") == 0)
			break;
		LOG_FATAL(logger, "Failed to connect server, retrying in 2s...");
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	if (!gRunning) { CloseSocket(sServer); return 1; }

	// ── Handshake ───────────────────────────────────────────────────
	try
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (HandshakeProcess(sServer) != 0) {
			LOG_FATAL(logger, "Handshake failed.");
			CloseSocket(sServer);
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		LOG_FATAL(logger, "Handshake error: " << e.what());
		CloseSocket(sServer);
		return 1;
	}

	// ── Interactive event loop ───────────────────────────────────────
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

	// ── Cleanup ──────────────────────────────────────────────────────
	gRunning = 0;
	consoleThr.join(); 
	CloseSocket(sServer);
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
