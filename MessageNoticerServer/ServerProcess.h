#pragma once
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "HandshakePacket.h"
#include "NormalPacket.h"

// Process the handshake from a selected socket
int HandshakeProcess(SOCKET& sSelected, std::vector<Client>& ClientList, string ServerName, string Version);

// Process normal messages from a selected socket
int NormalProcess(SOCKET& sSelected, std::vector<Client>& ClientList);
