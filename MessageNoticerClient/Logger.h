#pragma once
#include "pch.h"
#include "Colors.h"

// Log levels are color-coded by severity:
//   FATAL: red background   ERROR: red     WARN: yellow
//   INFO:  green            DEBUG: dim
//
// Each macro uses ANSI scroll-region control so that log output scrolls
// in the area above the bottom line, keeping the user's input line fixed at
// the very last row of the terminal window.
//
// gTermRows is set automatically when GetLogger() is first called
// (see Logger.cpp).  Until then (gTermRows == 0) logs go out normally.

extern int gTermRows;   // terminal height — set by first GetLogger() call

#define LOG_INFO(logger, msg) \
do { \
	Logger _l = (logger); \
	if (_l.isEnabledFor(log4cplus::INFO_LOG_LEVEL)) { \
		if (gTermRows > 0) \
			std::printf("\033[%d;1H\033[K", gTermRows - 1); \
		log4cplus::tostringstream _buf; \
		_buf << CLR_GREEN << msg << CLR_RESET; \
		_l.forcedLog(log4cplus::INFO_LOG_LEVEL, _buf.str(), __FILE__, __LINE__); \
		if (gTermRows > 0) { \
			std::printf("\033[%d;1H\033[K> ", gTermRows); \
			std::fflush(stdout); \
		} \
	} \
} while(0)

#define LOG_DEBUG(logger, msg) \
do { \
	Logger _l = (logger); \
	if (_l.isEnabledFor(log4cplus::DEBUG_LOG_LEVEL)) { \
		if (gTermRows > 0) \
			std::printf("\033[%d;1H\033[K", gTermRows - 1); \
		log4cplus::tostringstream _buf; \
		_buf << CLR_DIM << msg << CLR_RESET; \
		_l.forcedLog(log4cplus::DEBUG_LOG_LEVEL, _buf.str(), __FILE__, __LINE__); \
		if (gTermRows > 0) { \
			std::printf("\033[%d;1H\033[K> ", gTermRows); \
			std::fflush(stdout); \
		} \
	} \
} while(0)

#define LOG_WARN(logger, msg) \
do { \
	Logger _l = (logger); \
	if (_l.isEnabledFor(log4cplus::WARN_LOG_LEVEL)) { \
		if (gTermRows > 0) \
			std::printf("\033[%d;1H\033[K", gTermRows - 1); \
		log4cplus::tostringstream _buf; \
		_buf << CLR_YELLOW << msg << CLR_RESET; \
		_l.forcedLog(log4cplus::WARN_LOG_LEVEL, _buf.str(), __FILE__, __LINE__); \
		if (gTermRows > 0) { \
			std::printf("\033[%d;1H\033[K> ", gTermRows); \
			std::fflush(stdout); \
		} \
	} \
} while(0)

#define LOG_ERROR(logger, msg) \
do { \
	Logger _l = (logger); \
	if (_l.isEnabledFor(log4cplus::ERROR_LOG_LEVEL)) { \
		if (gTermRows > 0) \
			std::printf("\033[%d;1H\033[K", gTermRows - 1); \
		log4cplus::tostringstream _buf; \
		_buf << CLR_RED << msg << CLR_RESET; \
		_l.forcedLog(log4cplus::ERROR_LOG_LEVEL, _buf.str(), __FILE__, __LINE__); \
		if (gTermRows > 0) { \
			std::printf("\033[%d;1H\033[K> ", gTermRows); \
			std::fflush(stdout); \
		} \
	} \
} while(0)

#define LOG_FATAL(logger, msg) \
do { \
	Logger _l = (logger); \
	if (_l.isEnabledFor(log4cplus::FATAL_LOG_LEVEL)) { \
		if (gTermRows > 0) \
			std::printf("\033[%d;1H\033[K", gTermRows - 1); \
		log4cplus::tostringstream _buf; \
		_buf << CLR_RED_BG << msg << CLR_RESET; \
		_l.forcedLog(log4cplus::FATAL_LOG_LEVEL, _buf.str(), __FILE__, __LINE__); \
		if (gTermRows > 0) { \
			std::printf("\033[%d;1H\033[K> ", gTermRows); \
			std::fflush(stdout); \
		} \
	} \
} while(0)

Logger GetLogger(tstring name);

// 将字符串（含\0）转换为16进制格式字符串，如 "61 62 00 63"。
std::string strToHexString(const char* data, size_t len);

// ── Interactive input with command history ───────────────────────────
// Read a line from stdin with up/down arrow history support.
// Returns:  1 = line read into buf    0 = EOF (Ctrl+D)   -1 = signal
int ReadLine(char* buf, size_t size);
