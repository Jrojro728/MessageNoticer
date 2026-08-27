// Logger.cpp — Terminal scroll region, log4cplus setup, interactive ReadLine.
#include "pch.h"
#include "Logger.h"

// Running flag
extern volatile std::sig_atomic_t gRunning;

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
