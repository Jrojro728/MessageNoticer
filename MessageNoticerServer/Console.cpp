// Console.cpp —— Console related process
#include "pch.h"
#include "Console.h"

// Running flag
extern volatile std::sig_atomic_t gRunning;

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
		if (ret <= 0) raise(SIGINT);

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

	// ── /help ──────────────────────────────────────────────────────────
	auto cmdHelp = [&]() {
		LOG_INFO(logger, CLR_CYAN " === Commands ===" CLR_RESET);
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

// ═══════════════════════════════════════════════════════════════════════
//  Interactive line reader with up/down-arrow command history
// ═══════════════════════════════════════════════════════════════════════

// Persistent command history (shared across ReadLine calls).
// Up/down arrows navigate this circular buffer; Enter commits the
// current line and appends it to the tail (if not a duplicate).
static std::deque<std::string> s_history;
static const size_t MAX_HISTORY = 100;
static int s_historyPos = -1;   // -1 = brand-new input, 0..n = browsing history

// ── Low-level character read ─────────────────────────────────────────

#ifdef _WIN32
/// <summary>
/// Read a single keystroke from the console (Windows).  Uses _getch()
/// which returns the key code directly without waiting for Enter.
/// </summary>
static int ReadRawChar()
{
	while (gRunning)       // 轮询 gRunning，让外部可以随时停止
	{
		if (_kbhit())      // 非阻塞检查是否有输入
			return _getch();   // 有数据，绝不会阻塞

		// 没有输入，短暂休眠避免 CPU 100% 占用，同时提供快速响应
		Sleep(1);          // 1ms；如果需要更低 CPU 占用可改为 10-20ms
	}
	return -1;            // gRunning == 0，停止
}
#else
/// <summary>
/// Read one byte from stdin (POSIX).  The terminal must already be in
/// raw mode (set up inside ReadLine()).  Returns the byte as an unsigned
/// int, or -1 on EOF / signal interrupt.
/// </summary>
static int ReadRawChar()
{
	char c;
	ssize_t n = read(STDIN_FILENO, &c, 1);
	if (n == 1) return (unsigned char)c;
	return -1;
}
#endif

//Redraw input line after the "> " prompt 

/// <summary>
/// Clear everything on the current line after the prompt, then Redraw input line 
/// after the "> " prompt as the given string.  Used when the user presses Up/Down 
/// to switch to a history entry — the old input is erased and the new text appears.
/// </summary>
/// <param name="line">Text to show on the input line.</param>
static void RedrawLine(const std::string& line)
{
	std::fputs("\r\033[2K> ", stdout); // clear everything after prompt
	fwrite(line.data(), 1, line.size(), stdout);
	std::fflush(stdout);
}

/// <summary>
/// Read one line of interactive input with full line editing and command
/// history.  The terminal is switched to raw mode so that keystrokes are
/// received character-by-character instead of line-buffered.
///
/// Supported keys:
///   Enter       → commit the line, add to history, return 1
///   Backspace   → delete the previous character
///   Up arrow    → navigate backward through history
///   Down arrow  → navigate forward through history / new input
///   Ctrl+C      → abort (return -1)
///   Ctrl+D      → EOF on empty line (return 0)
///   printable   → append to current input, echo to terminal
///
/// The raw-mode terminal attributes are restored before the function
/// returns, regardless of the exit path.
/// </summary>
/// <param name="buf">Output buffer for the committed line.</param>
/// <param name="size">Capacity of the output buffer.</param>
/// <returns>1 on success (line committed), 0 on EOF, -1 on signal.</returns>
int ReadLine(char* buf, size_t size)
{
#ifdef _WIN32
	// _getch() works without explicit raw-mode setup
#else
	// ─ Save terminal attributes and enable raw mode ─
	// Disable: ICANON (line buffering), ECHO (automatic echo),
	//          ISIG (signal generation on Ctrl+C), IXON (flow control),
	//          ICRNL (CR→NL translation), OPOST (output processing).
	// VMIN = 1, VTIME = 0 → read() returns as soon as one byte arrives.
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_iflag &= ~(IXON | ICRNL | INLCR | ISTRIP);
	newt.c_oflag &= ~OPOST;
	newt.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	newt.c_cc[VMIN] = 1;
	newt.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &newt);
#endif

	static std::string currentLine;
	currentLine.clear();
	s_historyPos = -1;      // start with a fresh (non-history) line

	int result = 1;         // assume success until an error occurs
	bool done = false;

	while (!done && gRunning)
	{
		int c = ReadRawChar();

		// EOF or signal interrupt
		if (c < 0)
		{
			result = -1;
			break;
		}

		switch (c)
		{
			// Enter 
		case '\n':
		case '\r':
		{
			fputc('\n', stdout);
			fflush(stdout);
			done = true;
			break;
		}
		// Backspace / DEL
		case '\b':
		case 0x7f:
		{
			if (!currentLine.empty())
			{
				currentLine.pop_back();
				fputs("\b \b", stdout); // move left, clear, move left again
				fflush(stdout);
			}
			break;
		}
		case 0x1b: // Escape sequences for linux (arrows, etc.)
		{
#ifdef _WIN32
			// On Windows, _getch() returns 0xE0 or 0x00 as a prefix
			// so we can't handle it here because it will just don't be sent to this case.  
			// The arrow keys are handled in the 0xE0 case below.
#else
			// POSIX: arrow keys send ESC [ A (up), ESC [ B (down).
			int c2 = ReadRawChar();
			int c3 = 0;
			if (c2 == '[')
				c3 = ReadRawChar();

			if (c2 == '[' && c3 == 'A')      // Up
			{
				if (!s_history.empty()) {
					if (s_historyPos < 0)
						s_historyPos = (int)s_history.size() - 1;
					else if (s_historyPos > 0)
						--s_historyPos;
					currentLine = s_history[s_historyPos];
					RedrawLine(currentLine);
				}
			}
			else if (c2 == '[' && c3 == 'B') // Down
			{
				if (s_historyPos >= 0) {
					++s_historyPos;
					if (s_historyPos >= (int)s_history.size()) {
						s_historyPos = -1;
						currentLine.clear();
						RedrawLine(currentLine);
					}
					else {
						currentLine = s_history[s_historyPos];
						RedrawLine(currentLine);
					}
				}
			}
#endif
			break;
		}
		case 0x03:// Ctrl+C (literal ETX character)
		{
			result = -1;
			done = true;
			break;
		}
		case 0x04: // Ctrl+D (EOF — only when the line is empty)
		{
			if (currentLine.empty()) {
				result = 0;
				done = true;
			}
			// If there's text, Ctrl+D is ignored (matches common
			// shell behaviour where it's a delete-character key).
			break;
		}

		case 0xE0: // Windows extended key prefix
		{
			int c2 = ReadRawChar();

			// 0x48 = Up arrow, 0x50 = Down arrow (scancodes)
			if (c2 == 0x48)
			{
				if (s_history.empty()) break;
				if (s_historyPos < 0)
					s_historyPos = (int)s_history.size() - 1;
				else if (s_historyPos > 0)
					--s_historyPos;
				currentLine = s_history[s_historyPos];
				RedrawLine(currentLine);
			}
			else if (c2 == 0x50)
			{
				if (s_historyPos >= 0) {
					++s_historyPos;
					if (s_historyPos >= (int)s_history.size()) {
						s_historyPos = -1;
						currentLine.clear();
						RedrawLine(currentLine);
					}
					else {
						currentLine = s_history[s_historyPos];
						RedrawLine(currentLine);
					}
				}
			}
			break;
		}
		// ── Printable ASCII characters ──────────────────────────
		default:
		{
			if (c >= 0x20 && c <= 0x7e && currentLine.size() + 1 < size)
			{
				currentLine.push_back((char)c);
				fputc(c, stdout);  // echo the character
				fflush(stdout);
			}
			break;
		}
		}
	}

	// ── Restore terminal to original settings ────────────────────
#ifndef _WIN32
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
#pragma warning(disable : 4996)

	// ── On success: copy the line into the output buffer ─────────
	if (result == 1)
	{
		//When the line is empty, just draw the prompt
		if (currentLine == "")
			RedrawLine("");
		// Append to history (skip when it matches the previous entry)
		if (!currentLine.empty() &&
			(s_history.empty() || s_history.back() != currentLine))
		{
			if (s_history.size() >= MAX_HISTORY)
				s_history.pop_front();
			s_history.push_back(currentLine);
		}

		strncpy(buf, currentLine.c_str(), size - 1);
		buf[size - 1] = '\0';
	}

	return result;
}

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