#pragma once

#include "fly/config/config.hpp"
#include "fly/types/numeric/literals.hpp"

#include <chrono>
#include <cstdint>

namespace fly {

/**
 * Class to hold configuration values related to the logger.
 *
 * @author Timothy Flynn (trflynn89@pm.me)
 * @version July 18, 2016
 */
class LoggerConfig : public Config
{
public:
    static constexpr const char *identifier = "logger";

    /**
     * @return True if log files should be compressed after reaching the max log file size.
     */
    bool compress_log_files() const;

    /**
     * @return Max log file size (in bytes) before rotating the log file.
     */
    std::uintmax_t max_log_file_size() const;

    /**
     * @return Max message size (in bytes) per log.
     */
    std::uint32_t max_message_size() const;

    /**
     * @return Sleep time for logger IO thread.
     */
    std::chrono::milliseconds queue_wait_time() const;

protected:
    bool m_default_compress_log_files {true};
    std::uintmax_t m_default_max_log_file_size {20_u64 << 20};
    std::uint32_t m_default_max_message_size {256};
    std::chrono::milliseconds::rep m_default_queue_wait_time {100};
};

} // namespace fly
