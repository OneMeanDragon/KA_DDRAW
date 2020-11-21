#ifndef LOG_H
#define LOG_H

namespace emu {

enum MsgType {
    MSG_ERROR,
    MSG_INFORM,
    MSG_VERBOSE,
    MSG_ULTRA_VERBOSE,
};

#ifdef _DEBUG
const MsgType MAXIMAL_MSG_LEVEL = MSG_VERBOSE;
#else
const MsgType MAXIMAL_MSG_LEVEL = MSG_INFORM;
#endif

void log_init(const char * const file_name);
void log_line(const MsgType type, const size_t level, const char * const format, ...);
void log_raw_line(const char * const content);
void log_close(void);

/**
 * @brief Wrapper which ensures that the logKA functions which are above the current logKA level are optimized out.
 */
#define logKA(type, level, ...) {if (type <= ::emu::MAXIMAL_MSG_LEVEL) ::emu::log_line(type, level, __VA_ARGS__);}

} // namespace emu

#endif // LOG_H

// EOF //
