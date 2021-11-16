#include "log_util.h"

const char *log_path = "/root/media/logs/components.log";
std::ofstream lout;
std::atomic_bool log_switch(true);

void LOG(const char *func, const char *filename, int line, const char *level, const char *format, ...)
{
    if (!log_switch.load())
        return;

    char log_buffer[ECD_LOG_SIZE];

    time_t now = time(0);
    strftime(log_buffer, sizeof(log_buffer), "[%Y-%m-%d %H:%M:%S]", localtime(&now));

    sprintf(log_buffer + strlen(log_buffer), "[%s][%s:%d][%s]", level, filename, line, func);

    va_list ap;
    va_start(ap, format);
    vsnprintf(log_buffer + strlen(log_buffer), ECD_LOG_SIZE, format, ap);

    if (!lout.is_open())
        lout.open(log_path, std::ios::app);

    if (lout.is_open())
        lout << log_buffer << std::endl;
}