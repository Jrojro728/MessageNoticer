// MessageNoticerServer.cpp — TCP server with interactive console.
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "ServerProcess.h"
#include "Message.h"
#include "Console.h"
using std::cout, std::endl, std::cerr;

volatile std::sig_atomic_t gRunning = 1;
static void OnSignal(int) { gRunning = 0; }

// Thread-safe command queue
std::mutex gCmdMutex;
std::queue<std::string> gCmdQueue;

// Mutex for thread-safe access to ClientList
std::shared_mutex gClientMutex;

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

	// ---- Parse CLI args ----
	argh::parser cmdl(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
	std::string port = "12306";
	cmdl({ "-p", "--port" }) >> port;
	std::string ServerName = "MessageNoticer";
	cmdl({ "-n", "--name" }) >> ServerName;
	std::string Version = "0.1.0.4";
	if (cmdl({ "-v", "--version" }))
		std::cout << Version;
	int maxUsers = 64;
	cmdl({ "-m", "--max" }) >> maxUsers;

	// ---- Init ----
	log4cplus::Initializer initializer;
	InitNetwork();
	Logger logger = GetLogger(LOG4CPLUS_TEXT("main"));
	LOG_INFO(logger, ServerName << " v" << Version
		<< " starting on port " << port);

	// Start Listen
	SOCKET sListen;
	std::vector<Client> ClientList;
	if (CreateSocket(sListen, port.c_str(), NULL) != 0) {
		LOG_FATAL(logger, "Failed to create listening socket.");
#ifdef _WIN32
		WSACleanup();
#endif
		return 1;
	}

	std::thread consoleThr(ConsoleThread); //Start the Console thread

	fd_set readset{};
	FD_ZERO(&readset);
	FD_SET(sListen, &readset);
	SOCKET maxSock = sListen;

	LOG_INFO(logger, "Ready. Type /help in console for commands.");

	// ---- Main loop ----
	while (gRunning)
	{
		// Process pending console commands (non-blocking)
		std::string cmdLine;
		{
			std::lock_guard<std::mutex> lock(gCmdMutex);
			if (!gCmdQueue.empty()) {
				cmdLine = std::move(gCmdQueue.front());
				gCmdQueue.pop();
			}
		}
		if (!cmdLine.empty())
			ProcessCommand(cmdLine, ClientList, logger, readset, maxSock, sListen);

		// Waiting for the sockets(timeout: 100ms)
		fd_set tmpset = readset;
		struct timeval tv = { 0, 100000 };  // 100 ms
		int ret = select((int)(maxSock + 1), &tmpset, NULL, NULL, &tv);
		if (ret == SOCKET_ERROR) {
			int selErr = GetSocketError();
#ifdef _WIN32
			if (selErr == WSAEINTR) continue;
#else
			if (selErr == EINTR) continue;
#endif
			LOG_ERROR(logger, "select() failed: " << GetSocketError());
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// ---- 1. Handle new connections (main thread) ----
		if (FD_ISSET(sListen, &tmpset))
		{
			SOCKET c = accept(sListen, NULL, NULL);
			if (c == INVALID_SOCKET) {
				LOG_ERROR(logger, "accept() failed: " << GetSocketError());
				continue;
			}
			if (ClientList.size() >= (size_t)maxUsers) {
				LOG_ERROR(logger, "max clients (" << maxUsers << ") reached.");
				CloseSocket(c);
			}
			else {
				FD_SET(c, &readset);
				if (c > maxSock) maxSock = c;
				LOG_INFO(logger, c << " try to login.");
				std::unique_lock<std::shared_mutex> lock(gClientMutex);
				ClientList.push_back(Client(c, uuid::nil_uuid(), "", ClientStatus::Handshaking));
			}
		}

		// ---- 2. Collect ready client sockets (exclude sListen) ----
		std::vector<SOCKET> clientReady;
		for (size_t i = 0; i < readset.fd_count; i++)
		{
			SOCKET s = readset.fd_array[i];
			if (s == sListen) continue;
			if (FD_ISSET(s, &tmpset))
				clientReady.push_back(s);
		}

		// ---- 3. Process client data in parallel ----
		std::vector<std::future<void>> futures;
		std::vector<SOCKET> disconnected;
		std::mutex disconnectedMutex;

		for (SOCKET s : clientReady)
		{
			futures.push_back(std::async(std::launch::async,
				[&, s]()
				{
					try
					{
						uint8_t status = 0;
						{
							std::shared_lock<std::shared_mutex> readLock(gClientMutex);
							auto it = std::find(ClientList.begin(), ClientList.end(), Client(s));
							if (it == ClientList.end()) return;
							status = it->GetClientStatus();
						} // 释放读锁，避免在随后 HandshakeProcess 里获取
						int ret = 0;
						if (status == Handshaking)
							ret = HandshakeProcess(s, ClientList, ServerName, Version);
						if (status == Ready || status == Waiting)
							ret = NormalProcess(s, ClientList);
						// ret == 1 means processed but with non-fatal error (continue)
					}
					catch (SocketClosedException&)
					{
						std::lock_guard<std::mutex> lock(disconnectedMutex);
						disconnected.push_back(s);
					}
					catch (std::exception& e)
					{
						LOG_ERROR(logger, "Unknown exception occurred! reason: " << e.what());
					}
				}));
		}

		// Wait for all parallel processing to complete
		for (auto& f : futures) f.get();

		// ---- 4. Handle disconnections (single-threaded, modifies readset & ClientList) ----
		if (!disconnected.empty())
		{
			std::unique_lock<std::shared_mutex> lock(gClientMutex);
			for (SOCKET s : disconnected)
			{
				LOG_INFO(logger, s << " logged off.");
				CloseSocket(s);
				FD_CLR(s, &readset);
				if (s == maxSock)
					RecalculateMaxSock(maxSock, sListen, ClientList);
				auto it = std::find(ClientList.begin(), ClientList.end(), Client(s));
				if (it != ClientList.end())
					ClientList.erase(it);
			}
		}
	}

	// ---- Shutdown ----
	LOG_INFO(logger, "Shutting down...");
	gRunning = 0;
	consoleThr.join();

	for (auto& cl : ClientList) CloseSocket(cl.GetSocket());
	ClientList.clear();
	CloseSocket(sListen);
#ifdef _WIN32
	WSACleanup();
#endif
	LOG_INFO(logger, "Server stopped.");
	return 0;
}
