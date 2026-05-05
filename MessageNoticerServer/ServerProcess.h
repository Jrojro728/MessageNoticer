#pragma once
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "HandshakePacket.h"

// Process the handshake from a selected socket
int HandshakeProcess(SOCKET& sSelected, std::vector<Client>& ClientList);

// Process normal messages from a selected socket
int NormalProcess(SOCKET& sSelected, std::vector<Client>& ClientList);
