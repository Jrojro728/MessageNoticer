#pragma once
#include "pch.h"
#ifndef WIN32
#include "Network.h"
#endif // !WIN32

// Enable Virtual Terminal Processing on Windows console (for ANSI escape codes).
bool EnableVirtualTerminal();

// ©¤©¤ Interactive input with command history ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
// Read a line from stdin with up/down arrow history support.
// Returns:  1 = line read into buf    0 = EOF (Ctrl+D)   -1 = signal
int ReadLine(char* buf, size_t size);

// ©¤©¤ Interactive console ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
/// <summary>
/// Background thread: reads stdin via ReadLine() and enqueues commands.
/// </summary>
void ConsoleThread();

/// <summary>
/// Non-blocking: pop the next queued command, or return empty string.
/// </summary>
std::string PollCommand();

/// <summary>
/// Parse and execute a console command (/help, /msg, /list, etc.).
/// </summary>
void ProcessCommand(const std::string& line, SOCKET& sServer);
