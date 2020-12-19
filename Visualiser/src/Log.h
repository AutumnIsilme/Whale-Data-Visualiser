#pragma once

#include "Common.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

class  Log
{
public:
	static void Init();
	inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

private:
	static std::shared_ptr<spdlog::logger> s_Logger;
};

#ifdef _DEBUG
// Core log macros
#define LOG_TRACE(...)    ::Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::Log::GetLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::Log::GetLogger()->critical(__VA_ARGS__)

#else
//Core log macros
#define SN_CORE_TRACE(...)
#define SN_CORE_INFO(...)     
#define SN_CORE_WARN(...)     
#define SN_CORE_ERROR(...)    
#define SN_CORE_CRITICAL(...) 

// Client log macros			
#define SN_TRACE(...)         
#define SN_INFO(...)          
#define SN_WARN(...)          
#define SN_ERROR(...)         
#define SN_CRITICAL(...)      
#endif
