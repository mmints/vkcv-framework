#pragma once

#include <stdio.h>

namespace vkcv {
	
	enum class LogLevel {
		INFO,
		WARNING,
		ERROR
	};
	
	constexpr auto getLogOutput(LogLevel level) {
		switch (level) {
			case LogLevel::INFO:
				return stdout;
			default:
				return stderr;
		}
	}
	
	constexpr const char* getLogName(LogLevel level) {
		switch (level) {
			case LogLevel::INFO:
				return "INFO";
			case LogLevel::WARNING:
				return "WARNING";
			case LogLevel::ERROR:
				return "ERROR";
			default:
				return "UNKNOWN";
		}
	}
	
#ifndef NDEBUG
#ifndef VKCV_DEBUG_MESSAGE_LEN
#define VKCV_DEBUG_MESSAGE_LEN 1024
#endif

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define vkcv_log(level, ...) {      \
  char output_message [             \
    VKCV_DEBUG_MESSAGE_LEN          \
  ];                                \
  snprintf(                         \
    output_message,                 \
    VKCV_DEBUG_MESSAGE_LEN,         \
    __VA_ARGS__                     \
  );                                \
  fprintf(                          \
    getLogOutput(level),            \
    "[%s]: %s [%s, line %d: %s]\n", \
  	vkcv::getLogName(level),        \
    output_message,                 \
    __FILE__,                       \
    __LINE__,                       \
    __PRETTY_FUNCTION__             \
  );                                \
}

#else
#define vkcv_log(level, ...) {}
#endif

}