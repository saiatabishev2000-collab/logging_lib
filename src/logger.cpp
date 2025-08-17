#include "logging/logger.hpp"
#include "sinks.hpp"

namespace logging {

Status create_file_logger(const FileInit& init, ILogger** out_logger) {
    if (!out_logger) return Status::Err(100, "out_logger is null");
    auto* l = new FileLogger(init);
    if (auto st = l->open(); !st.ok) { delete l; return st; }
    *out_logger = l;
    return Status::Ok();
}

Status create_udp_logger(const UdpInit& init, ILogger** out_logger) {
    if (!out_logger) return Status::Err(101, "out_logger is null");
    auto* l = new UdpLogger(init);
    if (auto st = l->open(); !st.ok) { delete l; return st; }
    *out_logger = l;
    return Status::Ok();
}

} 
