#ifndef LOG_UTIL_H_NESSAJ_3729857248924
#define LOG_UTIL_H_NESSAJ_3729857248924

#include <stdarg.h>

#include <iostream>
#include <fstream>
#include <atomic>
#include <cstring>

#define ECD_LOG_SIZE 1024
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define ecd_info(format, ...) LOG(__func__, __FILENAME__, __LINE__, "INFO", format, ##__VA_ARGS__)
#define ecd_error(format, ...) LOG(__func__, __FILENAME__, __LINE__, "ERROR", format, ##__VA_ARGS__)

void LOG(const char *func, const char *filename, int line, const char *level, const char *format, ...);

#endif