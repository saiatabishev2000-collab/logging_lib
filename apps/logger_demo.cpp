#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <vector>
#include <cctype>

#include "logging/logger.hpp"

using logging::ILogger;
using logging::LogLevel;
using logging::Status;

static LogLevel parse_level_token(const std::string& t, bool& ok) {
    std::string s; s.reserve(t.size());
    for (char c : t) s.push_back(std::tolower(static_cast<unsigned char>(c)));

    ok = true;
    if (s == "debug") return LogLevel::Debug;
    if (s == "info")  return LogLevel::Info;
    if (s == "warn" || s == "warning") return LogLevel::Warn;
    if (s == "error" || s == "err")    return LogLevel::Error;
    ok = false;
    return LogLevel::Info;
}

struct Item {
    LogLevel level;
    std::string text;
};

class TSQueue {
public:
    void push(Item it) {
        std::lock_guard<std::mutex> lk(m_);
        q_.push(std::move(it));
        cv_.notify_one();
    }
    bool pop(Item& out) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [&]{ return !q_.empty() || stop_; });
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop();
        return true;
    }
    void stop() {
        std::lock_guard<std::mutex> lk(m_);
        stop_ = true;
        cv_.notify_all();
    }
private:
    std::queue<Item> q_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false;
};

int main(int argc, char** argv) {
    // Аргументы: --file <path> --level <debug|info|warn|error>
    std::string file;
    LogLevel default_lv = LogLevel::Info;

    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if (a == "--file" && i+1 < argc) { file = argv[++i]; }
        else if (a == "--level" && i+1 < argc) {
            bool ok=false; default_lv = parse_level_token(argv[++i], ok);
            if (!ok) { std::cerr << "Bad level\n"; return 2; }
        }
    }
    if (file.empty()) {
        std::cerr << "Usage: " << argv[0] << " --file <log.txt> [--level info]\n";
        return 1;
    }

    ILogger* logger = nullptr;
    auto st = logging::create_file_logger({file, default_lv}, &logger);
    if (!st.ok) {
        std::cerr << "Logger init failed: " << st.message << "\n";
        return 3;
    }

    std::unique_ptr<ILogger> guard(logger);

    TSQueue queue;
    std::atomic<bool> worker_running{true};
    std::thread worker([&]{
        Item it;
        while (queue.pop(it)) {
            auto s = logger->log(it.level, it.text);
            if (!s.ok) {
                std::cerr << "log() error: " << s.message << "\n";
            }
            logger->flush();
        }
        worker_running = false;
    });

    std::cout << "Logger demo. Type messages. Prefix with 'info:/warn:/error:/debug:'.\n";
    std::cout << "Commands: level <lv>, quit\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "exit") break;

        if (line.rfind("level ", 0) == 0) {
            std::string tok = line.substr(6);
            bool ok=false; auto lv = parse_level_token(tok, ok);
            if (!ok) { std::cout << "Unknown level\n"; continue; }
            logger->set_level(lv);
            std::cout << "Default level set to " << tok << "\n";
            continue;
        }

        // Разбор "<lvl>: message"
        LogLevel lv = logger->level();
        std::string msg = line;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string maybe = line.substr(0, colon);
            bool ok=false; auto parsed = parse_level_token(maybe, ok);
            if (ok) {
                lv = parsed;
                msg = line.substr(colon + 1);
                // trim пробел
                while (!msg.empty() && std::isspace(static_cast<unsigned char>(msg.front()))) msg.erase(msg.begin());
            }
        }

        queue.push({lv, std::move(msg)});
    }

    queue.stop();
    if (worker.joinable()) worker.join();

    return 0;
}

