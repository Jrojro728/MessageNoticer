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

// ── Shared between ConsoleThread and the main loop ──────────────────
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

// ═══════════════════════════════════════════════════════════════════════
//  Console input (used by main's ConsoleThread)
// ═══════════════════════════════════════════════════════════════════════

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
/// Redraw from the given column to the end of the line, then place the
/// cursor at the specified column.
/// </summary>
/// <param name="line">Text to display on the input line.</param>
/// <param name="from">Column index to start redrawing from.</param>
/// <param name="to">Final cursor column after redrawing.</param>
static void RedrawRange(const std::string& line, size_t from, size_t to)
{
	fputs("\033[K", stdout);               // 清除光标到行尾
	fputs(line.c_str() + from, stdout);    // 输出 from 之后的剩余文本
	size_t back = line.size() - to;        // 从行尾回退到 to
	if (back > 0)
		fprintf(stdout, "\033[%dD", (int)back);
	fflush(stdout);
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
///   Left arrow  → move cursor left
///   Right arrow → move cursor right
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
	size_t cursorPos = 0;   // The cursor's index in currentLine

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
		// Backspace
		case '\b':
		{
			if (cursorPos > 0)
			{
				fputs("\033[D", stdout);   // ── 先左移一格，光标对准待删字符 ──
				--cursorPos;
				currentLine.erase(cursorPos, 1);
				RedrawRange(currentLine, cursorPos, cursorPos);
			}
			break;
		}
		// Delete
		case 0x7f:
		{
			if (cursorPos < currentLine.size())
			{
				currentLine.erase(cursorPos, 1);
				RedrawRange(currentLine, cursorPos, cursorPos);
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
					cursorPos = currentLine.size();   // ── NEW: 光标回到行尾 ──
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
						cursorPos = 0;                // ── NEW ──
						RedrawLine(currentLine);
					}
					else {
						currentLine = s_history[s_historyPos];
						cursorPos = currentLine.size(); // ── NEW ──
						RedrawLine(currentLine);
					}
				}
			}
			else if (c2 == '[' && c3 == 'C') // Right
			{
				if (cursorPos < currentLine.size()) {
					++cursorPos;
					fputs("\033[C", stdout);   // 光标右移一格
					fflush(stdout);
				}
			}
			else if (c2 == '[' && c3 == 'D') // Left
			{
				if (cursorPos > 0) {
					--cursorPos;
					fputs("\033[D", stdout);   // 光标左移一格
					fflush(stdout);
				}
			}
			else if (c2 == '[' && c3 == '3') // Delete (ESC [ 3 ~)
			{
				int c4 = ReadRawChar();
				if (c4 == '~')
				{
					if (cursorPos < currentLine.size())
					{
						currentLine.erase(cursorPos, 1);
						RedrawRange(currentLine, cursorPos, cursorPos);
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

			// 0x48 = Up, 0x50 = Down, 0x4B = Left, 0x4D = Right
			if (c2 == 0x48) // Up
			{
				if (s_history.empty()) break;
				if (s_historyPos < 0)
					s_historyPos = (int)s_history.size() - 1;
				else if (s_historyPos > 0)
					--s_historyPos;
				currentLine = s_history[s_historyPos];
				cursorPos = currentLine.size();
				RedrawLine(currentLine);
			}
			else if (c2 == 0x50) // Down
			{
				if (s_historyPos >= 0) {
					++s_historyPos;
					if (s_historyPos >= (int)s_history.size()) {
						s_historyPos = -1;
						currentLine.clear();
						cursorPos = 0;
						RedrawLine(currentLine);
					}
					else {
						currentLine = s_history[s_historyPos];
						cursorPos = currentLine.size();
						RedrawLine(currentLine);
					}
				}
			}
			else if (c2 == 0x4B) // Left
			{
				if (cursorPos > 0) {
					--cursorPos;
					fputs("\033[D", stdout);
					fflush(stdout);
				}
			}
			else if (c2 == 0x4D) // Right
			{
				if (cursorPos < currentLine.size()) {
					++cursorPos;
					fputs("\033[C", stdout);
					fflush(stdout);
				}
			}
			else if (c2 == 0x53) // Delete
			{
				if (cursorPos < currentLine.size())
				{
					currentLine.erase(cursorPos, 1);
					RedrawRange(currentLine, cursorPos, cursorPos);
				}
			}
			break;
		}
		// ── Printable ASCII characters ──────────────────────────
		default:
		{
			// ── MODIFIED: 在光标处插入 ──
			if (c >= 0x20 && c <= 0x7e && currentLine.size() + 1 < size)
			{
				currentLine.insert(cursorPos, 1, (char)c);
				RedrawRange(currentLine, cursorPos, cursorPos + 1);
				++cursorPos;
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
		// When the line is empty, just draw the prompt
		if (currentLine.empty())
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
		if (t.size() > 1 && t[1] == "usage")
		{
			LOG_WARN(logger, "Usage: /connect <ip> <port>");
			LOG_WARN(logger, "when ip and port isn't offered, try to reconnect");
			LOG_WARN(logger, "default port: 12306");
			return;
		}

		auto it = std::find(t.cbegin(), t.cend(), "-f");
		if (!gDisconnected && it == t.cend())
		{
			LOG_INFO(logger, "Already connected to the server.");
			LOG_INFO(logger, "use -f to force reconnect");
			return;
		}
		else if (!gDisconnected && it != t.cend())
			t.erase(it);

		if (t.size() == 1)
			throw RestartException();
		else if (t.size() == 2)
			throw RestartException(t[1]);
		else if (t.size() == 3)
			throw RestartException(t[1], std::stoi(t[2]));
	};

	// ── Dispatch table ───────────────────────────────────────────
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
