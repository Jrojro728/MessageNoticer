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
