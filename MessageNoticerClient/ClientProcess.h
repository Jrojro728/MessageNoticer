#pragma once
#include "pch.h"
#include "Network.h"

// Global running flag, defined in MessageNoticerClient.cpp
extern volatile std::sig_atomic_t gRunning;

// ── Handshake ───────────────────────────────────────────────

/// <summary>
/// Perform the handshake with the server.
/// Returns 0 on success, 1 on failure.
/// </summary>
/// <param name="sServer">Socket of the server</param>
/// <param name="ClientName">Client's name</param>
/// <returns></returns>
int HandshakeProcess(SOCKET& sServer, string ClientName);

// ── Interactive console ────────────────────────────────────
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

// ── select-based packet handler ────────────────────────────
/// <summary>
/// One round of select() + packet receive + dispatch.
/// Returns 0 on success (or timeout), 1 on error/disconnect.
/// Called from the main event loop.
/// </summary>
int NormalProcess(SOCKET& sServer);
