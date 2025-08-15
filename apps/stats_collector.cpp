#include <iostream>
#include <string>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include "logging/logger.hpp"

using namespace std::chrono;

struct Stats {
    uint64_t total = 0;
    uint64_t per_level[4] = {0,0,0,0}; // Debug, Info, Warn, Error
    uint64_t last_hour = 0;
    size_t   min_len = SIZE_MAX;
    size_t   max_len = 0;
    uint64_t sum_len = 0;

    bool operator!=(const Stats& o) const {
        return std::memcmp(this, &o, sizeof(Stats)) != 0;
    }
};

static std::atomic<bool> stop_flag{false};
static void on_sigint(int){ stop_flag = true; }

static int level_index_from_message(const std::string& s) {
    // ожидаем формат: "[timestamp][LEVEL] message"
    auto l = s.find('[');
    auto m = s.find(']', l == std::string::npos ? 0 : l+1);
    if (m == std::string::npos) return 1;
    auto l2 = s.find('[', m+1);
    auto m2 = s.find(']', l2 == std::string::npos ? 0 : l2+1);
    if (l2 == std::string::npos || m2 == std::string::npos) return 1;
    std::string level = s.substr(l2+1, m2 - (l2+1));
    if (level == "DEBUG") return 0;
    if (level == "INFO")  return 1;
    if (level == "WARN")  return 2;
    if (level == "ERROR") return 3;
    return 1;
}

static void print_stats(const Stats& st) {
    auto avg = st.total ? double(st.sum_len)/double(st.total) : 0.0;
    std::cout << "=== Stats ===\n"
              << "total: " << st.total << "\n"
              << "by level: debug=" << st.per_level[0]
              << " info=" << st.per_level[1]
              << " warn=" << st.per_level[2]
              << " error=" << st.per_level[3] << "\n"
              << "last hour: " << st.last_hour << "\n"
              << "len: min=" << (st.min_len==SIZE_MAX?0:st.min_len)
              << " max=" << st.max_len
              << " avg=" << avg << "\n";
}

int main(int argc, char** argv) {
    // Аргументы: --bind <IP:PORT> --N <num> --T <sec>
    std::string bind_ip = "0.0.0.0";
    uint16_t bind_port = 9999;
    uint64_t N = 100;
    uint64_t T = 10;

    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if (a == "--bind" && i+1 < argc) {
            std::string s = argv[++i];
            auto p = s.find(':');
            if (p == std::string::npos) { std::cerr << "Use IP:PORT\n"; return 2; }
            bind_ip = s.substr(0,p);
            bind_port = static_cast<uint16_t>(std::stoi(s.substr(p+1)));
        } else if (a == "--N" && i+1 < argc) {
            N = std::stoull(argv[++i]);
        } else if (a == "--T" && i+1 < argc) {
            T = std::stoull(argv[++i]);
        }
    }

    std::signal(SIGINT, on_sigint);

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { std::cerr << "socket() failed\n"; return 3; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    if (::inet_aton(bind_ip.c_str(), &addr.sin_addr) == 0) {
        std::cerr << "bad bind ip\n"; return 4;
    }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed\n"; return 5;
    }

    Stats st{}, last_printed{};
    std::deque<steady_clock::time_point> last_hour_times;
    auto last_print_tp = steady_clock::now();

    std::cout << "Listening UDP on " << bind_ip << ":" << bind_port
              << " (N=" << N << ", T=" << T << "s). Ctrl+C to stop.\n";

    while (!stop_flag) {
        char buf[2048];
        sockaddr_in src{};
        socklen_t sl = sizeof(src);
        ssize_t n = ::recvfrom(fd, buf, sizeof(buf)-1, 0, reinterpret_cast<sockaddr*>(&src), &sl);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "recvfrom() failed\n"; break;
        }
        buf[n] = '\0';
        std::string msg(buf);

        // печать сообщения
        std::cout << msg << "\n";

        // обновить статистику
        st.total++;
        size_t len = msg.size();
        st.sum_len += len;
        if (len < st.min_len) st.min_len = len;
        if (len > st.max_len) st.max_len = len;
        int idx = level_index_from_message(msg);
        if (idx >=0 && idx < 4) st.per_level[idx]++;

        auto now = steady_clock::now();
        last_hour_times.push_back(now);
        // удалить старые > 1h
        auto cutoff = now - hours(1);
        while (!last_hour_times.empty() && last_hour_times.front() < cutoff)
            last_hour_times.pop_front();
        st.last_hour = last_hour_times.size();

        bool printed = false;
        if (N > 0 && (st.total % N == 0)) {
            print_stats(st);
            last_printed = st;
            last_print_tp = now;
            printed = true;
        }

        auto since = duration_cast<seconds>(now - last_print_tp).count();
        if (!printed && since >= (long long)T && st != last_printed) {
            print_stats(st);
            last_printed = st;
            last_print_tp = now;
        }
    }

    ::close(fd);
    return 0;
}

