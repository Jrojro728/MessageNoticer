// MessageNoticerServer.cpp ¡ª TCP server with argh-based interactive console.
// Console input runs in a background thread; commands are processed via a
// thread-safe queue checked in the main loop. This works on both POSIX
// (select on stdin) and Windows (select only accepts sockets).
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "HandshakePacket.h"
#include "ServerProcess.h"
#include "Message.h"
using std::cout, std::endl, std::cerr;

static volatile std::sig_atomic_t gRunning = 1;
static void OnSignal(int) { gRunning = 0; }

// Thread-safe command queue for console input
static std::mutex gCmdMutex;
static std::queue<std::string> gCmdQueue;

static void ConsoleThread();
static void ProcessCommand(const std::string& line,
	std::vector<Client>& ClientList, Logger& logger);

int main(int argc, char* argv[])
{
	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);

	// ---- Parse startup CLI arguments with argh ----
	argh::parser cmdl(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

	std::string port = "12306";
	cmdl({ "-p", "--port" }) >> port;

	std::string serverName = "MessageNoticer";
	cmdl({ "-n", "--name" }) >> serverName;

	std::string version = "1.0";
	cmdl({ "-v", "--version" }) >> version;

	int maxUsers = 64;
	cmdl({ "-m", "--max" }) >> maxUsers;

	// ---- Init ----
	InitNetwork();
	log4cplus::Initializer initializer;
	Logger logger = GetLogger(LOG4CPLUS_TEXT("main"));
	LOG_INFO(logger, serverName << " v" << version
		<< " starting on port " << port);

	SOCKET sListen;
	std::vector<Client> ClientList;

	if (CreateSocket(sListen, port.c_str(), NULL) != 0) {
		LOG_FATAL(logger, "Failed to create listening socket.");
		return 1;
	}

	// Start console input thread
	std::thread consoleThr(ConsoleThread);

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
			ProcessCommand(cmdLine, ClientList, logger);

		// Use a timeout so console commands are handled promptly
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

		for (SOCKET s = 0; s <= maxSock; ++s)
		{
			if (!FD_ISSET(s, &tmpset))
				continue;

			// ---- New connection ----
			if (s == sListen)
			{
				SOCKET c = accept(s, NULL, NULL);
				if (c == INVALID_SOCKET) {
					LOG_ERROR(logger, "accept() failed: " << GetSocketError());
					continue;
				}
				if (ClientList.size() >= (size_t)maxUsers) {
					LOG_ERROR(logger, "max clients (" << maxUsers << ") reached.");
					CloseSocket(c);
					continue;
				}
				FD_SET(c, &readset);
				if (c > maxSock) maxSock = c;
				LOG_INFO(logger, c << " try to login.");
				continue;
			}

			// ---- Client data ----
			try {
				ret = HandshakeProcess(s, ClientList);
				if (ret == 1) continue;
				ret = NormalProcess(s, ClientList);
			}
			catch (ClientSocketClosedException&) {
				LOG_INFO(logger, s << " logged off.");
				CloseSocket(s);
				FD_CLR(s, &readset);
				if (s == maxSock) {
					maxSock = sListen;
					for (auto& cl : ClientList)
						if (cl.GetSocket() > maxSock)
							maxSock = cl.GetSocket();
				}
				if (!ClientList.empty())
					ClientList.erase(
						std::find(ClientList.begin(), ClientList.end(), Client(s)));
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

// ---------------------------------------------------------------------------
// Background thread: reads stdin line by line
// ---------------------------------------------------------------------------
static void ConsoleThread()
{
	while (gRunning)
	{
		char buf[4096];
		if (fgets(buf, sizeof(buf), stdin) == nullptr)
			break;  // EOF

		// Strip trailing newline
		size_t len = strlen(buf);
		while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
			buf[--len] = '\0';

		if (len == 0) continue;

		std::lock_guard<std::mutex> lock(gCmdMutex);
		gCmdQueue.push(buf);
	}

	// stdin closed ¡ª nothing more to do; thread exits
	// The main loop continues servicing clients.
}

// ---------------------------------------------------------------------------
// Command dispatch (uses argh for parsing)
// ---------------------------------------------------------------------------
static void ProcessCommand(const std::string& line,
	std::vector<Client>& ClientList, Logger& logger)
{
	// Split line into tokens
	std::istringstream iss(line);
	std::vector<std::string> tokens;
	std::string tok;
	while (iss >> tok) tokens.push_back(tok);
	if (tokens.empty()) return;

	// Build null-terminated argv for argh
	std::vector<const char*> args = { "console" };
	for (auto& t : tokens) args.push_back(t.c_str());
	args.push_back(nullptr);

	argh::parser cmd;
	cmd.parse(args.data(), argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
	const auto& pos = cmd.pos_args();

	// pos[0] = "console" (placeholder), pos[1] = command
	std::string cmdName = (pos.size() >= 2) ? pos[1] : "";

	if (cmdName == "/help" || cmdName == "/h" || cmdName == "/?") {
		LOG_INFO(logger, "=== Commands ===");
		LOG_INFO(logger, "  /help | /h | /?           Show this help");
		LOG_INFO(logger, "  /list | /ls               List connected clients");
		LOG_INFO(logger, "  /kick <socket>            Disconnect client");
		LOG_INFO(logger, "  /count | /c               Client count");
		LOG_INFO(logger, "  /shutdown | /exit | /quit Graceful shutdown");
	}
	else if (cmdName == "/list" || cmdName == "/ls") {
		LOG_INFO(logger, "Clients (" << ClientList.size() << "):");
		for (auto& cl : ClientList)
			LOG_INFO(logger, "  " << cl.GetSocket()
				<< "  " << cl.GetReadableClientName());
	}
	else if (cmdName == "/count" || cmdName == "/c") {
		LOG_INFO(logger, "Client count: " << ClientList.size());
	}
	else if (cmdName == "/kick") {
		if (pos.size() < 3) {
			LOG_WARN(logger, "Usage: /kick <socket>");
		}
		else {
			SOCKET target = (SOCKET)std::stoi(pos[2]);
			auto it = std::find(ClientList.begin(), ClientList.end(), Client(target));
			if (it == ClientList.end())
				LOG_WARN(logger, "Client " << target << " not found.");
			else {
				CloseSocket(it->GetSocket());
				ClientList.erase(it);
				LOG_INFO(logger, "Kicked client " << target);
			}
		}
	}
	else if (cmdName == "/shutdown" || cmdName == "/exit" || cmdName == "/quit") {
		LOG_INFO(logger, "Shutdown requested.");
		gRunning = 0;
	}
	else {
		LOG_WARN(logger, "Unknown command: \"" << cmdName << "\". Type /help");
	}
}
