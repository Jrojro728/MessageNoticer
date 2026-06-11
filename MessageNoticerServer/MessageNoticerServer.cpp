// MessageNoticerServer.cpp — TCP server with interactive console.
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "ServerProcess.h"
#include "Message.h"
using std::cout, std::endl, std::cerr;

static volatile std::sig_atomic_t gRunning = 1;
static void OnSignal(int) { gRunning = 0; }

// Thread-safe command queue
static std::mutex gCmdMutex;
static std::queue<std::string> gCmdQueue;

static void ConsoleThread();
static void ProcessCommand(const std::string& line,
	std::vector<Client>& ClientList, Logger& logger,
	fd_set& readset, SOCKET& maxSock, SOCKET sListen);
static void RecalcMaxSock(SOCKET& maxSock, SOCKET sListen,
	const std::vector<Client>& ClientList);

int main(int argc, char* argv[])
{
	signal(SIGINT, OnSignal);
	signal(SIGTERM, OnSignal);

	// ---- Parse CLI args ----
	argh::parser cmdl(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
	std::string port = "12306";
	cmdl({ "-p", "--port" }) >> port;
	std::string ServerName = "MessageNoticer";
	cmdl({ "-n", "--name" }) >> ServerName;
	std::string Version = "0.1.0.4";
	cmdl({ "-v", "--version" }) >> Version;
	int maxUsers = 64;
	cmdl({ "-m", "--max" }) >> maxUsers;

	// ---- Init ----
	log4cplus::Initializer initializer;
	InitNetwork();
	Logger logger = GetLogger(LOG4CPLUS_TEXT("main"));
	LOG_INFO(logger, ServerName << " v" << Version
		<< " starting on port " << port);

	SOCKET sListen;
	std::vector<Client> ClientList;
	if (CreateSocket(sListen, port.c_str(), NULL) != 0) {
		LOG_FATAL(logger, "Failed to create listening socket.");
		return 1;
	}

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
			ProcessCommand(cmdLine, ClientList, logger, readset, maxSock, sListen);

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
				ClientList.push_back(Client(c, uuid::nil_uuid(), "", ClientStatus::Handshaking));
				continue;
			}

			// ---- Client data ----
			try {
				uint8_t status = std::find(ClientList.begin(), ClientList.end(), Client(s))->GetClientStatus();
				if (status == Handshaking)
					ret = HandshakeProcess(s, ClientList, ServerName, Version);
				if (status == Ready || status == Waiting)
					ret = NormalProcess(s, ClientList);
				if (ret == 1) continue;
			}
			catch (ClientSocketClosedException&) {
				LOG_INFO(logger, s << " logged off.");
				CloseSocket(s);
				FD_CLR(s, &readset);
				if (s == maxSock)
				RecalcMaxSock(maxSock, sListen, ClientList);
				if (!ClientList.empty())
					ClientList.erase(std::find(ClientList.begin(), ClientList.end(), Client(s)));
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
// Console input thread
// ---------------------------------------------------------------------------
static void ConsoleThread()
{
	while (gRunning)
	{
		char buf[4096];
		int ret = ReadLine(buf, sizeof(buf));
		if (ret <= 0) break;

		size_t len = strlen(buf);
		if (len == 0) continue;

		std::lock_guard<std::mutex> lock(gCmdMutex);
		gCmdQueue.push(buf);
	}
}

// ---------------------------------------------------------------------------
// Helpers for ProcessCommand
// ---------------------------------------------------------------------------

// Find client by socket, returns iterator or ClientList.end()
static auto FindClient(std::vector<Client>& ClientList, SOCKET target)
	-> decltype(ClientList.end())
{
	return std::find(ClientList.begin(), ClientList.end(), Client(target));
}

// Recalculate maxSock from ClientList
static void RecalcMaxSock(SOCKET& maxSock, SOCKET sListen,
	const std::vector<Client>& ClientList)
{
	maxSock = sListen;
	for (auto& cl : ClientList)
		if (cl.GetSocket() > maxSock)
			maxSock = cl.GetSocket();
}

// Remove client from readset, close socket, erase from list
static void RemoveClient(std::vector<Client>& ClientList,
	fd_set& readset, SOCKET& maxSock, SOCKET sListen, SOCKET target)
{
	auto it = FindClient(ClientList, target);
	if (it == ClientList.end()) return;

	CloseSocket(it->GetSocket());
	FD_CLR(it->GetSocket(), &readset);
	if (it->GetSocket() == maxSock)
		RecalcMaxSock(maxSock, sListen, ClientList);
	ClientList.erase(it);
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------
static void ProcessCommand(const std::string& line,
	std::vector<Client>& ClientList, Logger& logger,
	fd_set& readset, SOCKET& maxSock, SOCKET sListen)
{
	// Tokenize
	std::istringstream iss(line);
	std::vector<std::string> t;
	std::string tok;
	while (iss >> tok) t.push_back(tok);
	if (t.empty()) return;

	const std::string& cmd = t[0];

	// ── /help ──────────────────────────────────────────────────────────
	auto cmdHelp = [&]() {
		LOG_INFO(logger, CLR_CYAN "=== Commands ===" CLR_RESET);
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/help | /h | /?" CLR_RESET "           Show this help");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/list | /ls" CLR_RESET "               List connected clients");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/kick <socket>" CLR_RESET "            Disconnect client");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/count | /c" CLR_RESET "               Client count");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/shutdown | /exit | /quit" CLR_RESET " Graceful shutdown");
	};

	// ── /list ──────────────────────────────────────────────────────────
	auto cmdList = [&]() {
		LOG_INFO(logger, "Clients (" << ClientList.size() << "):");
		for (auto& cl : ClientList)
			LOG_INFO(logger, "  " << cl.GetSocket()
				<< "  " << cl.GetReadableClientName());
	};

	// ── /count ─────────────────────────────────────────────────────────
	auto cmdCount = [&]() {
		LOG_INFO(logger, "Client count: " << ClientList.size());
	};

	// ── /kick ──────────────────────────────────────────────────────────
	auto cmdKick = [&]() -> bool {
		if (t.size() < 2) {
			LOG_WARN(logger, "Usage: /kick <socket>");
			return false;
		}
		SOCKET target;
		try { target = (SOCKET)std::stoi(t[1]); }
		catch (...) {
			LOG_WARN(logger, "Invalid socket: " << t[1]);
			return false;
		}
		if (FindClient(ClientList, target) == ClientList.end()) {
			LOG_WARN(logger, "Client " << target << " not found.");
			return false;
		}
		RemoveClient(ClientList, readset, maxSock, sListen, target);
		LOG_INFO(logger, "Kicked client " << target);
		return true;
	};

	// ── /shutdown ──────────────────────────────────────────────────────
	auto cmdShutdown = [&]() {
		LOG_INFO(logger, "Shutdown requested.");
		gRunning = 0;
	};

	// ── Dispatch table ─────────────────────────────────────────────────
	struct Cmd { const char* name; std::function<void()> fn; };
	const Cmd dispatch[] = {
		{ "/help",     cmdHelp },
		{ "/h",        cmdHelp },
		{ "/?",        cmdHelp },
		{ "/list",     cmdList },
		{ "/ls",       cmdList },
		{ "/count",    cmdCount },
		{ "/c",        cmdCount },
		{ "/kick",     [&]{ cmdKick(); } },
		{ "/shutdown", cmdShutdown },
		{ "/exit",     cmdShutdown },
		{ "/quit",     cmdShutdown },
	};

	for (auto& entry : dispatch) {
		if (cmd == entry.name) {
			entry.fn();
			return;
		}
	}

	LOG_WARN(logger, "Unknown command: \"" << cmd << "\". Type /help");
}
