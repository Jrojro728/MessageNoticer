#pragma once
#include "pch.h"
#include "Network.h"

//Send Handshake request to the server
int HandshakeProcess(SOCKET& sServer);

//The whole normal progress
int NormalProcess(SOCKET& sServer);