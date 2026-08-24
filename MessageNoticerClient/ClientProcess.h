#pragma once
#include "pch.h"
#include "Network.h"

// ── Handshake ───────────────────────────────────────────────

/// <summary>
/// Perform the handshake with the server.
/// Returns 0 on success, 1 on failure.
/// </summary>
/// <param name="sServer">Socket of the server</param>
/// <param name="ClientName">Client's name</param>
/// <returns></returns>
int HandshakeProcess(SOCKET& sServer, string ClientName);

// ── select-based packet handler ────────────────────────────
/// <summary>
/// One round of select() + packet receive + dispatch.
/// Called from the main event loop.
/// </summary>
/// <return>0 on success (or timeout), 1 on error, -1 on disconnect.</return>
int NormalProcess(SOCKET& sServer);
