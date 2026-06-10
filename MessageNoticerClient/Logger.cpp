// Logger.cpp — Terminal scroll region, log4cplus setup, interactive ReadLine.
#include "pch.h"
#include "Logger.h"

// ── Terminal height for scroll-region input-line preservation ─────────
// gTermRows stores the number of rows in the terminal window, queried once
// on the first call to GetLogger().  A value > 0 enables the scroll-region
// behaviour in the LOG_* macros (Logger.h).
int gTermRows = 0;

/// <summary>
/// Query the terminal window height via ioctl (POSIX) or console API (Win32).
/// Returns 24 as a safe fallback when the information is unavailable.
/// </summary>
static int GetTerminalRows()
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
		return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	return 24;
#else
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
		return (int)ws.ws_row;
	return 24;
#endif
}

/// <summary>
/// Set up the terminal scroll region so that only rows 1 .. (rows-1) scroll
/// when log output is printed.  The very last row is reserved for the user's
/// input prompt ("> ") and never moves.
/// Called exactly once from the first GetLogger() call.
/// </summary>
static void SetupScrollRegion()
{
	gTermRows = GetTerminalRows();
	if (gTermRows < 2) gTermRows = 2;
	// ANSI escape: set scroll region to [1, gTermRows-1]
	std::printf("\033[1;%dr", gTermRows - 1);
	// Move cursor to bottom line and show initial prompt
	std::printf("\033[%d;1H> ", gTermRows);
	std::fflush(stdout);
}

// ── Reset scroll region (used in signal handler + atexit) ──────────
/// <summary>
/// Restore full-terminal scrolling, clear the screen, and move the cursor
/// to the top-left corner.  Called from the signal handler (Ctrl+C), the
/// atexit() hook, and on normal shutdown.
/// </summary>
static void ResetScrollRegion()
{
	std::fputs("\033[r\033[2J\033[H", stdout);
	std::fflush(stdout);
}

// ── Signal handler: reset terminal before chaining to main's handler ──

#ifdef _WIN32
/// <summary>
/// Win32 console control handler.  Resets the scroll region on Ctrl+C or
/// Ctrl+Break, then returns FALSE so other registered handlers (including
/// main's signal()) can also process the event.
/// </summary>
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT)
		ResetScrollRegion();
	return FALSE;
}
#else

// Saved copies of the original signal actions installed by main().
// Our handler chains into them after resetting the terminal.
static struct sigaction g_oldSigInt = {};
static struct sigaction g_oldSigTerm = {};

/// <summary>
/// SIGINT handler (Ctrl+C).  Resets the scroll region immediately, then
/// chains to whatever handler main() registered (typically OnSignal which
/// sets gRunning = 0).  If the old action was SIG_DFL, reinstates the
/// default and re-raises the signal.
/// </summary>
static void sigIntHandler(int sig)
{
	ResetScrollRegion();
	if (g_oldSigInt.sa_handler == SIG_DFL) {
		signal(sig, SIG_DFL);
		raise(sig);
	}
	else if (g_oldSigInt.sa_handler != SIG_IGN) {
		g_oldSigInt.sa_handler(sig);
	}
}

/// <summary>
/// SIGTERM handler (kill/terminate).  Same logic as sigIntHandler.
/// </summary>
static void sigTermHandler(int sig)
{
	ResetScrollRegion();
	if (g_oldSigTerm.sa_handler == SIG_DFL) {
		signal(sig, SIG_DFL);
		raise(sig);
	}
	else if (g_oldSigTerm.sa_handler != SIG_IGN) {
		g_oldSigTerm.sa_handler(sig);
	}
}
#endif

/// <summary>
/// Install our signal hooks that reset the scroll region before the
/// caller's handler runs.  On POSIX, uses sigaction() to chain; on
/// Windows, uses SetConsoleCtrlHandler() which is additive.
/// sa_flags = 0 (no SA_RESTART) so that blocking system calls like
/// fgets/read are interrupted by the signal.
/// Called once from the first GetLogger() call.
/// </summary>
static void InstallSignalHooks()
{
#ifdef _WIN32
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigIntHandler;
	sigaction(SIGINT, &sa, &g_oldSigInt);
	sa.sa_handler = sigTermHandler;
	sigaction(SIGTERM, &sa, &g_oldSigTerm);
#endif
}

/// <summary>
/// Obtain (or create) a log4cplus Logger with the given name.
///
/// On the very first call, the terminal scroll region is set up and signal
/// hooks are installed — this happens once globally, regardless of how many
/// times GetLogger is called or how many different logger names are used.
///
/// Each distinct logger name gets its own ConsoleAppender (with a coloured
/// timestamped layout) attached only once.  The log level defaults to DEBUG
/// in debug builds and INFO in release builds.
/// </summary>
/// <param name="name">Logger name, e.g. "main", "HandshakeProcess"</param>
/// <returns>A ready-to-use Logger instance.</returns>
Logger GetLogger(tstring name)
{
	static bool firstCall = true;
	if (firstCall) {
		firstCall = false;
		SetupScrollRegion();
		InstallSignalHooks();
		std::atexit(ResetScrollRegion);
	}

	Logger logger = log4cplus::Logger::getInstance(name);

	// Only attach an appender if none exists yet for this logger.
	// This prevents duplicate appends when GetLogger is called multiple
	// times for the same name (e.g. "main").
	if (logger.getAllAppenders().size() == 0)
	{
		SharedAppenderPtr appender(new log4cplus::ConsoleAppender());
		appender->setName(LOG4CPLUS_TEXT("console"));

		// Layout pattern:
		//   %D{...}  → date/time with milliseconds
		//   [%t]     → thread ID
		//   %-5p     → log level (left-aligned, 5 chars)
		//   %c       → logger name
		//   %m       → the message itself
		//   [%F:%L]  → source file & line number
		//   %n       → newline
		tstring pattern = LOG4CPLUS_TEXT("%D{%y/%m/%d %H:%M:%S.%q} [%t] %-5p %c - %m [%F:%L]%n");
		appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));

#ifdef _DEBUG
		logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else
		logger.setLogLevel(log4cplus::INFO_LOG_LEVEL);
#endif
		logger.addAppender(appender);
	}

	return logger;
}

/// <summary>
/// Convert an arbitrary byte buffer to a hexadecimal string, with bytes
/// separated by spaces.  Example:
///   strToHexString("abc\0def", 7) → "61 62 63 00 64 65 66"
/// </summary>
/// <param name="data">Pointer to the raw byte buffer.</param>
/// <param name="len">Number of bytes to convert.</param>
/// <returns>Space-separated hex string.</returns>
std::string strToHexString(const char* data, size_t len) {
	std::stringstream ss;
	ss << std::hex << std::setfill('0'); // 16进制，不足2位补0
	for (size_t i = 0; i < len; ++i) {
		// Cast through unsigned char to avoid sign-extension artefacts
		ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(data[i]));
		if (i != len - 1)
			ss << " "; // 字节间用空格分隔
	}
	return ss.str(); // 返回拼接好的16进制字符串
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
static inline int ReadRawChar()
{
	return _getch();
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
/// Clear everything on the current line after the prompt, then write
/// the given string.  Used when the user presses Up/Down to switch to
/// a history entry — the old input is erased and the new text appears.
/// </summary>
/// <param name="line">Text to show on the input line.</param>
static void RedrawLine(const std::string& line)
{
	std::fputs("\033[K", stdout);       // clear everything after prompt
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

	while (!done)
	{
		int c = ReadRawChar();

		// ── EOF or signal interrupt ──────────────────────────────
		if (c < 0)
		{
			result = -1;
			break;
		}

		switch (c)
		{
			// ── Enter ───────────────────────────────────────────────
		case '\n':
		case '\r':
		{
			fputc('\n', stdout);
			fflush(stdout);
			done = true;
			break;
		}

		// ── Backspace / DEL ─────────────────────────────────────
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

		// ── Escape sequences (arrows, etc.) ─────────────────────
		case 0x1b:
		{
#ifdef _WIN32
			// On Windows, _getch() returns 0xE0 or 0x00 as a prefix
			// for extended keys, then a second call gives the scancode.
			int c2 = ReadRawChar();
			int c3 = (c2 == 0xE0 || c2 == 0x00) ? ReadRawChar() : 0;

			// 0x48 = Up arrow, 0x50 = Down arrow (scancodes)
			if ((c2 == 0x48 || c3 == 0x48))
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
			else if ((c2 == 0x50 || c3 == 0x50))
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

		// ── Ctrl+C (literal ETX character, 0x03) ────────────────
		case 0x03:
		{
			result = -1;
			done = true;
			break;
		}

		// ── Ctrl+D (EOF — only when the line is empty) ──────────
		case 0x04:
		{
			if (currentLine.empty()) {
				result = 0;
				done = true;
			}
			// If there's text, Ctrl+D is ignored (matches common
			// shell behaviour where it's a delete-character key).
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
