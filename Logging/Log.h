//
// Created by whaim on 2016/3/31.
//

#ifndef XESTCORE_LOG_H
#define XESTCORE_LOG_H

#include <memory>
#include <unordered_map>
#include <boost/asio.hpp>
#include "Define.h"
#include "Appender.h"
#include "cppformat/format.h"
#include "Logger.h"

#define LOGGER_ROOT "root"

class TC_COMMON_API Log
{
    typedef std::unordered_map<std::string, Logger> LoggerMap;
private:
    Log();
    ~Log();

public:
    static Log* instance();
    void Initialize(boost::asio::io_service* ioService);
    void SetSynchronous();  // Not threadsafe - should only be called from main() after all threads are joined
    void LoadFromConfig();
    void Close();
    bool ShouldLog(std::string const& type, LogLevel level) const;
    bool SetLogLevel(std::string const& name, char const* level, bool isLogger = true);

    template<typename Format, typename... Args>
    inline void outMessage(std::string const& filter, LogLevel const level, Format&& fmt, Args&&... args)
    {
        write(std::unique_ptr<LogMessage>(new LogMessage(level, filter,
                                               fmt::sprintf(std::forward<Format>(fmt), std::forward<Args>(args)...))));

    }

    template<typename Format, typename... Args>
    void outCommand(uint32 account, Format&& fmt, Args&&... args)
    {
        if (!ShouldLog("commands.gm", LOG_LEVEL_INFO))
            return;

        std::unique_ptr<LogMessage> msg =
                std::unique_ptr<LogMessage>(new LogMessage(LOG_LEVEL_INFO, "commands.gm",
                                                 fmt::sprintf(std::forward<Format>(fmt), std::forward<Args>(args)...)));

        msg->param1 = std::to_string(account);

        write(std::move(msg));
    }

    void outCharDump(char const* str, uint32 account_id, uint64 guid, char const* name);

    void SetRealmId(uint32 id);

    template<class AppenderImpl>
    void RegisterAppender()
    {
        using Index = typename AppenderImpl::TypeIndex;
        auto itr = appenderFactory.find(Index::value);
        ASSERT(itr == appenderFactory.end());
        appenderFactory[Index::value] = &CreateAppender<AppenderImpl>;
    }

    std::string const& GetLogsDir() const { return m_logsDir; }
    std::string const& GetLogsTimestamp() const { return m_logsTimestamp; }

private:
    static std::string GetTimestampStr();
    void write(std::unique_ptr<LogMessage>&& msg) const;

    Logger const* GetLoggerByType(std::string const& type) const;
    Appender* GetAppenderByName(std::string const& name);
    uint8 NextAppenderId();
    void CreateAppenderFromConfig(std::string const& name);
    void CreateLoggerFromConfig(std::string const& name);
    void ReadAppendersFromConfig();
    void ReadLoggersFromConfig();

    AppenderCreatorMap appenderFactory;
    AppenderMap appenders;
    LoggerMap loggers;
    uint8 AppenderId;
    LogLevel lowestLogLevel;

    std::string m_logsDir;
    std::string m_logsTimestamp;

    boost::asio::io_service* _ioService;
    boost::asio::strand* _strand; //strand提供串行执行, 能够保证线程安全
};

inline Logger const* Log::GetLoggerByType(std::string const& type) const
{
    LoggerMap::const_iterator it = loggers.find(type);
    if (it != loggers.end())
        return &(it->second);

    if (type == LOGGER_ROOT)
        return NULL;

    std::string parentLogger = LOGGER_ROOT;
    size_t found = type.find_last_of(".");
    if (found != std::string::npos)
        parentLogger = type.substr(0,found);

    return GetLoggerByType(parentLogger);
}

inline bool Log::ShouldLog(std::string const& type, LogLevel level) const
{
    // TODO: Use cache to store "Type.sub1.sub2": "Type" equivalence, should
    // Speed up in cases where requesting "Type.sub1.sub2" but only configured
    // Logger "Type"

    // Don't even look for a logger if the LogLevel is lower than lowest log levels across all loggers
    if (level < lowestLogLevel)
        return false;

    Logger const* logger = GetLoggerByType(type);
    if (!logger)
        return false;

    LogLevel logLevel = logger->getLogLevel();
    return logLevel != LOG_LEVEL_DISABLED && logLevel <= level;
}

#define sLog Log::instance()

#define LOG_EXCEPTION_FREE(filterType__, level__, ...) \
    { \
        try \
        { \
            sLog->outMessage(filterType__, level__, __VA_ARGS__); \
        } \
        catch (std::exception& e) \
        { \
            sLog->outMessage("server", LOG_LEVEL_ERROR, "Wrong format occurred (%s) at %s:%u.", \
                e.what(), __FILE__, __LINE__); \
        } \
    }

#if PLATFORM != PLATFORM_WINDOWS
void check_args(const char*, ...) ATTR_PRINTF(1, 2);
void check_args(std::string const&, ...);

// This will catch format errors on build time
#define TC_LOG_MESSAGE_BODY(filterType__, level__, ...)                 \
        do {                                                            \
            if (sLog->ShouldLog(filterType__, level__))                 \
            {                                                           \
                if (false)                                              \
                    check_args(__VA_ARGS__);                            \
                                                                        \
                LOG_EXCEPTION_FREE(filterType__, level__, __VA_ARGS__); \
            }                                                           \
        } while (0)
#else
#define TC_LOG_MESSAGE_BODY(filterType__, level__, ...)                 \
        __pragma(warning(push))                                         \
        __pragma(warning(disable:4127))                                 \
        do {                                                            \
            if (sLog->ShouldLog(filterType__, level__))                 \
                LOG_EXCEPTION_FREE(filterType__, level__, __VA_ARGS__); \
        } while (0)                                                     \
        __pragma(warning(pop))
#endif

#define TC_LOG_TRACE(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_TRACE, __VA_ARGS__)

#define TC_LOG_DEBUG(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_DEBUG, __VA_ARGS__)

#define TC_LOG_INFO(filterType__, ...)  \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_INFO, __VA_ARGS__)

#define TC_LOG_WARN(filterType__, ...)  \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_WARN, __VA_ARGS__)

#define TC_LOG_ERROR(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_ERROR, __VA_ARGS__)

#define TC_LOG_FATAL(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_FATAL, __VA_ARGS__)
#endif //XESTCORE_LOG_H
