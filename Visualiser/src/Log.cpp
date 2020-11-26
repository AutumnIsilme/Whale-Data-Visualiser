#include "pch.h"
#include "Log.h"

void Log::Critical(const char* msg)
{
	std::cout << "\033[0;31m[CRITICAL] " << msg << "\033[0m\n";
}

void Log::Error(const char* msg)
{
	std::cout << "\033[1;31m[ERROR]    " << msg << "\033[0m\n";
}

void Log::Warn(const char* msg)
{
	std::cout << "\033[0;33m[WARN]     " << msg << "\033[0m\n";
}

void Log::Info(const char* msg)
{
	std::cout << "\033[0;36m[INFO]     " << msg << "\033[0m\n";
}

void Log::Debug(const char* msg)
{
	std::cout << "\033[0;37m[DEBUG]    " << msg << "\033[0m\n";
}

Log::~Log()
{}
