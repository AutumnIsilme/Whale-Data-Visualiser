#pragma once

class Log
{
public:
	static void Critical(const char* msg);
	static void Error(const char* msg);
	static void Warn(const char* msg);
	static void Info(const char* msg);
	static void Debug(const char* msg);

private:
	Log();
	~Log();
};