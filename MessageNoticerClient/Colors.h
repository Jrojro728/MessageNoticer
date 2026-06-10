#pragma once

// ANSI escape codes for colored terminal output.
// Works on Linux, macOS, and modern Windows 10+ terminals.
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_DIM     "\033[2m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"
#define CLR_WHITE   "\033[37m"

// Background + foreground combinations
#define CLR_RED_BG   "\033[41m\033[97m"
#define CLR_GREEN_BG "\033[42m\033[30m"
#define CLR_YELLOW_BG "\033[43m\033[30m"
#define CLR_BLUE_BG  "\033[44m\033[30m"
