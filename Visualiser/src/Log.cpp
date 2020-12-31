#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::Init()
{
	std::vector<spdlog::sink_ptr> logSinks;

	#ifdef _DEBUG
	logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	logSinks[logSinks.size() - 1]->set_pattern("%^[%T] %n: %v%$");
	#endif // _DEBUG

	logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Visualiser.log", true));
	logSinks[logSinks.size() - 1]->set_pattern("[%T] [%l] %n: %v");

	s_Logger = std::make_shared<spdlog::logger>("Visualiser", begin(logSinks), end(logSinks));
	s_Logger->set_level(spdlog::level::trace);
	s_Logger->flush_on(spdlog::level::trace);
}
