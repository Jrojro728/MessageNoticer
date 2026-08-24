#pragma once
#include "pch.h"

// Enable Virtual Terminal Processing on Windows console (for ANSI escape codes).
bool EnableVirtualTerminal();

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
