#pragma once
#include <mutex>
#include <fstream>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logging/logger.hpp"

namespace logging {

inline const char* level_to_cstr(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

inline std::string now_iso8601_local() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{};
    // thread-safe локализация времени для Linux
    localtime_r(&tt, &tm);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

class FileLogger final : public ILogger {
public:
    explicit FileLogger(FileInit init) : init_(std::move(init)), level_(init_.default_level) {}

    Status open() {
        std::lock_guard<std::mutex> lk(m_);
        ofs_.open(init_.file_path, std::ios::out | std::ios::app);
        if (!ofs_.is_open()) {
            return Status::Err(1, "failed to open log file: " + init_.file_path);
        }
        opened_ = true;
        return Status::Ok();
    }

    Status log(LogLevel lv, std::string_view msg) override {
        if (lv < level_.load(std::memory_order_relaxed)) return Status::Ok();
        if (!opened_) return Status::Err(2, "logger not opened");

        std::lock_guard<std::mutex> lk(m_);
        if (!ofs_.good()) return Status::Err(3, "stream is not good");
        ofs_ << '[' << now_iso8601_local() << "]"
             << '[' << level_to_cstr(lv) << "] "
             << msg << '\n';
        return Status::Ok();
    }

    void set_level(LogLevel lv) override { level_.store(lv, std::memory_order_relaxed); }
    LogLevel level() const override { return level_.load(std::memory_order_relaxed); }
    void flush() override {
        std::lock_guard<std::mutex> lk(m_);
        if (opened_) ofs_.flush();
    }
    void close() override {
        std::lock_guard<std::mutex> lk(m_);
        if (opened_) {
            ofs_.flush();
            ofs_.close();
            opened_ = false;
        }
    }

    ~FileLogger() override { close(); }

private:
    FileInit init_;
    std::ofstream ofs_;
    std::mutex m_;
    std::atomic<LogLevel> level_;
    bool opened_ = false;
};

class UdpLogger final : public ILogger {
public:
    explicit UdpLogger(UdpInit init) : init_(std::move(init)), level_(init_.default_level) {}

    Status open() {
        std::lock_guard<std::mutex> lk(m_);
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return Status::Err(10, "socket() failed");

        std::memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port   = htons(init_.port);
        if (::inet_aton(init_.host.c_str(), &addr_.sin_addr) == 0) {
            ::close(fd_);
            fd_ = -1;
            return Status::Err(11, "invalid host");
        }
        opened_ = true;
        return Status::Ok();
    }

    Status log(LogLevel lv, std::string_view msg) override {
        if (lv < level_.load(std::memory_order_relaxed)) return Status::Ok();
        if (!opened_) return Status::Err(12, "logger not opened");

        std::ostringstream oss;
        oss << '[' << now_iso8601_local() << "]"
            << '[' << level_to_cstr(lv) << "] "
            << msg;
        auto s = oss.str();

        std::lock_guard<std::mutex> lk(m_);
        ssize_t n = ::sendto(fd_, s.data(), s.size(), 0,
                             reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
        if (n < 0 || static_cast<size_t>(n) != s.size()) {
            return Status::Err(13, "sendto() failed");
        }
        return Status::Ok();
    }

    void set_level(LogLevel lv) override { level_.store(lv, std::memory_order_relaxed); }
    LogLevel level() const override { return level_.load(std::memory_order_relaxed); }
    void flush() override {/* UDP — ничего */}
    void close() override {
        std::lock_guard<std::mutex> lk(m_);
        if (opened_) {
            ::close(fd_);
            fd_ = -1;
            opened_ = false;
        }
    }

    ~UdpLogger() override { close(); }

private:
    UdpInit init_;
    int fd_ = -1;
    sockaddr_in addr_{};
    std::mutex m_;
    std::atomic<LogLevel> level_;
    bool opened_ = false;
};

} // namespace logging

