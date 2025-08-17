#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace logging {

enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3
};

// Унифицированный статус без исключений
struct Status {
    bool ok = true;
    int  code = 0;                // 0 = OK
    std::string message{};
    static Status Ok() { return {}; }
    static Status Err(int c, std::string msg) { return Status{false, c, std::move(msg)}; }
};

// Базовый интерфейс
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual Status log(LogLevel level, std::string_view message) = 0;
    virtual void   set_level(LogLevel level) = 0;
    virtual LogLevel level() const = 0;
    virtual void   flush() = 0;
    virtual void   close() = 0;
};

// Фабрики. Интерфейс логирования идентичен для файла и сокета.
struct FileInit {
    std::string file_path;
    LogLevel default_level = LogLevel::Info;
};

struct UdpInit {
    std::string host; // напр. "127.0.0.1"
    uint16_t    port = 9999;
    LogLevel default_level = LogLevel::Info;
};

Status create_file_logger(const FileInit& init, ILogger** out_logger); // владение у вызывающего, delete через delete logger
Status create_udp_logger (const UdpInit&  init, ILogger** out_logger);

} 

