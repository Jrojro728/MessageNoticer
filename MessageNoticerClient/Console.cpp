#include "pch.h"
#include "Console.h"
#include "Logger.h"
#include "HandshakePacket.h"
#include "NormalPacket.h"
#include "Message.h"
#include "Colors.h"

// Global running flag
extern volatile std::sig_atomic_t gRunning;
volatile std::sig_atomic_t gDisconnected = 0;

// ©¤©¤ Shared between ConsoleThread and the main loop ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
static std::mutex gCmdMutex;
static std::deque<std::string> gCmdQueue;

extern Client LocalClient; //The Client itself

// Enable Virtual Terminal Processing on Windows console (for ANSI escape codes).
bool EnableVirtualTerminal() {
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return false;

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode)) return false;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	return SetConsoleMode(hOut, dwMode);
#else
	return true; // POSIX terminals usually support ANSI escape codes by default
#endif // _WIN32
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  Console input (used by main's ConsoleThread)
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T

/// <summary>
/// Background thread: reads stdin via ReadLine() and enqueues lines.
/// Exits when gRunning becomes 0 or stdin closes.
/// </summary>
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
		gCmdQueue.push_back(buf);
	}
}

/// <summary>
/// Non-blocking poll: return the next queued command, or an empty string.
/// </summary>
/// <returns>The next command line, or empty if none.</returns>
std::string PollCommand()
{
	std::lock_guard<std::mutex> lock(gCmdMutex);
	if (gCmdQueue.empty()) return {};
	std::string line = std::move(gCmdQueue.front());
	gCmdQueue.pop_front();
	return line;
}

/// <summary>
/// Tokenise |cmd| and dispatch via a lookup table.
/// </summary>
/// <param name="line">The command line to process (e.g. "/msg 123 Hello")</param>
/// <param name="sServer">Connected server socket, used to send command packets.</param>
void ProcessCommand(const std::string& line, SOCKET& sServer)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("Command"));

	std::istringstream iss(line);
	std::vector<std::string> t;
	std::string tok;
	while (iss >> tok) t.push_back(tok);
	if (t.empty()) return;

	const std::string& cmd = t[0];

	// /help
	auto cmdHelp = [&]() {
		LOG_INFO(logger, CLR_CYAN "=== Commands ===" CLR_RESET);
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/help | /h | /?" CLR_RESET "      Show this help");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/msg <id> <text>" CLR_RESET "     Send message to client <id>");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/broadcast <text>" CLR_RESET "    Send message to all");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/list" CLR_RESET "                List online clients");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/level <0-255>" CLR_RESET "       Set min message level");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/exit | /quit" CLR_RESET "        Disconnect and exit");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/whoami" CLR_RESET "              Show your identity");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/connect <ip> <port>" CLR_RESET " (Re)Connect to the server");
		};

	auto cmdMsgServer = [&]() {
		if (t.size() < 3) {
			LOG_WARN(logger, "Usage: /msg server <text>");
			return;
		}

		std::string text;
		for (size_t i = 2; i < t.size(); ++i) {
			if (i > 2) text += " ";
			text += t[i];
		}

		Message msg("", TextContent(text), LocalClient, ServerClient);
		SendAMessagePacket(msg, 1).Send(sServer);
		LOG_INFO(logger, CLR_GREEN "Message sent to server." CLR_RESET);
		};

	// /msg
	auto cmdMsg = [&]() {
		if (t[1] == "server")
		{
			cmdMsgServer();
			return;
		}

		if (t.size() < 3) {
			LOG_WARN(logger, "Usage: /msg <receiver_id> <text>");
			return;
		}

		SOCKET receiverId = INVALID_SOCKET;
		try { receiverId = (SOCKET)std::stoi(t[1]); }
		catch (...) { LOG_WARN(logger, "Invalid receiver ID: " << t[1]); return; }

		std::string text;
		for (size_t i = 2; i < t.size(); ++i) {
			if (i > 2) text += " ";
			text += t[i];
		}

		Message msg("", TextContent(text), LocalClient, Client(receiverId));
		SendAMessagePacket(msg, 1).Send(sServer);
		LOG_INFO(logger, CLR_GREEN "Message sent to [" << receiverId << "]" CLR_RESET);
		};

	// /broadcast
	auto cmdBroadcast = [&]() {
		if (t.size() < 2) {
			LOG_WARN(logger, "Usage: /broadcast <text>");
			return;
		}
		std::string text;
		for (size_t i = 1; i < t.size(); ++i) {
			if (i > 1) text += " ";
			text += t[i];
		}
		Message msg("", TextContent(text), LocalClient, BroadcastClient);
		SendAMessagePacket(msg, 1).Send(sServer);
		LOG_INFO(logger, CLR_GREEN "Broadcast sent." CLR_RESET);
		};

	// /list
	auto cmdList = [&]() {
		GetClientListPacket(MessagePriority::Low, 0).Send(sServer);
		};

	// /level
	auto cmdLevel = [&]() {
		uint8_t level = 0;
		if (t.size() >= 2) {
			try {
				int l = std::stoi(t[1]);
				if (l < 0) l = 0;
				if (l > 255) l = 255;
				level = (uint8_t)l;
			}
			catch (...) {
				LOG_WARN(logger, "Invalid level: " << t[1] << ". Using 0.");
			}
		}
		WaitingMessagePacket(level).Send(sServer);
		LOG_INFO(logger, "Set message level to " CLR_BOLD << (int)level << CLR_RESET);
		};

	auto cmdWhoami = [&]() {
		WhoAmIPacket("User request").Send(sServer);
		};

	// /exit | /quit
	auto cmdExit = [&]() {
		LOG_INFO(logger, "Exiting...");
		gRunning = 0;
		};

	auto cmdConnect = [&]() {
		if (!gDisconnected)
		{
			LOG_INFO(logger, "Already connected to the server.");
			return;
		}
		if (t.size() == 1)
			throw RestartException(std::nullopt, 12306);
		else if (t.size() == 2)
			throw RestartException(t[1], 12306);
		else if (t.size() == 3)
			throw RestartException(t[1], std::stoi(t[2]));

		if (t[1] == "usage")
		{
			LOG_WARN(logger, "Usage: /connect <ip> <port>");
			LOG_WARN(logger, "when ip and port isn't offered, try to reconnect");
			LOG_WARN(logger, "default port: 12306");
			return;
		}

		LOG_INFO(logger, "Will attempt to reconnect to the server...");
		};

	// ©¤©¤ Dispatch table ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	struct Cmd { const char* name; std::function<void()> fn; };
	const Cmd dispatch[] = {
		{ "/help",      cmdHelp },
		{ "/h",         cmdHelp },
		{ "/?",			cmdHelp },
		{ "/msg",       cmdMsg },
		{ "/broadcast", cmdBroadcast },
		{ "/list",      cmdList },
		{ "/level",     cmdLevel },
		{ "/exit",      cmdExit },
		{ "/quit",      cmdExit },
		{ "/whoami",    cmdWhoami },
		{ "/connect",   cmdConnect },
	};

	for (auto& entry : dispatch) {
		if (cmd == entry.name) {
			entry.fn();
			return;
		}
	}

	LOG_WARN(logger, "Unknown command: \"" << cmd << "\". Type /help");
}
