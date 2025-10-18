#pragma once
#include "pch.h"

#define LOG_INFO(logger, msg) LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT(msg))
#define LOG_DEBUG(logger, msg) LOG4CPLUS_DEBUG(logger, LOG4CPLUS_TEXT(msg))
#define LOG_WARN(logger, msg) LOG4CPLUS_WARN(logger, LOG4CPLUS_TEXT(msg))
#define LOG_ERROR(logger, msg) LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT(msg))
#define LOG_FATAL(logger, msg) LOG4CPLUS_FATAL(logger, LOG4CPLUS_TEXT(msg))

Logger GetLogger(tstring name);