#pragma once
#include "pch.h"
#include "Client.h"

void ConsoleThread();
void ProcessCommand(const std::string& line,
	std::vector<Client>& ClientList, Logger& logger,
	fd_set& readset, SOCKET& maxSock, SOCKET sListen);

// Recalculate maxSock from ClientList
void RecalculateMaxSock(SOCKET& maxSock, SOCKET sListen,
	const std::vector<Client>& ClientList);

// Read a line from stdin with up/down arrow history support.
// Returns:  1 = line read into buf    0 = EOF (Ctrl+D)   -1 = signal
int ReadLine(char* buf, size_t size);

// Enable Virtual Terminal Processing on Windows console (for ANSI escape codes).
bool EnableVirtualTerminal();