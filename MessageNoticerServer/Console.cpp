// Console.cpp ¡ª¡ª Console related process
#include "pch.h"
#include "Console.h"

// Running flag
extern std::sig_atomic_t gRunning = 1;

// Thread-safe command queue
extern std::mutex gCmdMutex;
extern std::queue<std::string> gCmdQueue;

// Mutex for thread-safe access to ClientList
extern std::mutex gClientMutex;

// ---------------------------------------------------------------------------
// Console input thread
// ---------------------------------------------------------------------------
void ConsoleThread()
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
auto FindClient(std::vector<Client>& ClientList, SOCKET target)
-> decltype(ClientList.end())
{
	return std::find(ClientList.begin(), ClientList.end(), Client(target));
}

// Recalculate maxSock from ClientList
void RecalculateMaxSock(SOCKET& maxSock, SOCKET sListen,
	const std::vector<Client>& ClientList)
{
	maxSock = sListen;
	for (auto& cl : ClientList)
		if (cl.GetSocket() > maxSock)
			maxSock = cl.GetSocket();
}

// Remove client from readset, close socket, erase from list
void RemoveClient(std::vector<Client>& ClientList,
	fd_set& readset, SOCKET& maxSock, SOCKET sListen, SOCKET target)
{
	auto it = FindClient(ClientList, target);
	if (it == ClientList.end()) return;

	CloseSocket(it->GetSocket());
	FD_CLR(it->GetSocket(), &readset);
	if (it->GetSocket() == maxSock)
		RecalculateMaxSock(maxSock, sListen, ClientList);
	ClientList.erase(it);
}


// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------
void ProcessCommand(const std::string& line,
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

	// ©¤©¤ /help ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	auto cmdHelp = [&]() {
		LOG_INFO(logger, CLR_CYAN " === Commands ===" CLR_RESET);
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/help | /h | /?" CLR_RESET "           Show this help");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/list | /ls" CLR_RESET "               List connected clients");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/kick <socket>" CLR_RESET "            Disconnect client");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/count | /c" CLR_RESET "               Client count");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/shutdown | /exit | /quit" CLR_RESET " Graceful shutdown");
	};

	// ©¤©¤ /list ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	auto cmdList = [&]() {
		LOG_INFO(logger, "Clients (" << ClientList.size() << "):");
		for (auto& cl : ClientList)
			LOG_INFO(logger, "  " << cl.GetSocket()
				<< "  " << cl.GetReadableClientName());
	};

	// ©¤©¤ /count ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	auto cmdCount = [&]() {
		LOG_INFO(logger, "Client count: " << ClientList.size());
	};

	// ©¤©¤ /kick ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
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

	// ©¤©¤ /shutdown ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	auto cmdShutdown = [&]() {
		LOG_INFO(logger, "Shutdown requested.");
		gRunning = 0;
	};

	// ©¤©¤ Dispatch table ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	struct Cmd { const char* name; std::function<void()> fn; };
	const std::vector<Cmd> dispatch = {
		{ "/help",     cmdHelp },
		{ "/h",        cmdHelp },
		{ "/?",        cmdHelp },
		{ "/list",     cmdList },
		{ "/ls",       cmdList },
		{ "/count",    cmdCount },
		{ "/c",        cmdCount },
		{ "/kick",     cmdKick },
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
