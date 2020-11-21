#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>

//#ifdef _DEBUG
#define LOG_SUPPORT
//#endif

namespace emu {
namespace {

/**
 * @brief Currently opened logKA file.
 */
FILE * log_file = NULL;

} // anonymous namespace

/**
 * @brief Initializes logKA to specified file name.
 *
 * If logKA is already opened, it is closed.
 */
void log_init(const char * const file_name)
{
#ifdef LOG_SUPPORT

    log_close();
    log_file = fopen(file_name, "a+");

#endif // LOG_SUPPORT
}

/**
 * @brief Writes message to the logKA.
 *
 * Does nothing if the logKA is not opened.
 */
void log_line(const MsgType type, const size_t level, const char * const format, ...)
{
#ifdef LOG_SUPPORT

    if (type > MAXIMAL_MSG_LEVEL) {
        return;
    }

    if (log_file == NULL) {
        return;
    }

    char tmpbuff[1000];

    // Indent.

    const size_t used_level = (level < 100) ? level : 0;

    memset(tmpbuff, ' ', used_level);
    tmpbuff[used_level] = 0;

    // Fixed prefixes.

    if (type == MSG_ERROR) {
        strcat_s(tmpbuff, "ERR:");
    }

    // Real content.

    va_list params;
    va_start(params, format);
    vsprintf_s(tmpbuff + used_level, sizeof(tmpbuff) - used_level, format, params);
    va_end(params);

    // New line formatter.

    strcat_s(tmpbuff, "\n");

    // logKA output.

    fprintf(log_file, "%s", tmpbuff);
    OutputDebugStringA(tmpbuff);

    // Flush the file if necessary.

    if (is_log_flush_enabled()) {
        fflush(log_file);
    }
#endif // LOG_SUPPORT
}

/**
 * @brief Directly writes specified string to the logKA.
 *
 * The line should end with the \n character.
 */
void log_raw_line(const char * const content)
{
#ifdef LOG_SUPPORT
    if (log_file == NULL) {
        return;
    }

    // logKA output.

    fprintf(log_file, "%s", content);
    OutputDebugStringA(content);

    // Flush the file if necessary.

    if (is_log_flush_enabled()) {
        fflush(log_file);
    }

#endif // LOG_SUPPORT

}

/**
 * @brief Closes the logKA.
 */
void log_close(void)
{
#ifdef LOG_SUPPORT

    if (log_file == NULL) {
        return;
    }
    fclose(log_file);
    log_file = NULL;

#endif // LOG_SUPPORT
}

} // namespace emu

// EOF //
