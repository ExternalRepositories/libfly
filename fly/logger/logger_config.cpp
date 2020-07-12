#include "fly/logger/logger_config.hpp"

namespace fly {

//==================================================================================================
bool LoggerConfig::compress_log_files() const
{
    return get_value<bool>("compress_log_files", m_default_compress_log_files);
}

//==================================================================================================
std::uintmax_t LoggerConfig::max_log_file_size() const
{
    return get_value<std::uintmax_t>("max_log_file_size", m_default_max_log_file_size);
}

//==================================================================================================
std::uint32_t LoggerConfig::max_message_size() const
{
    return get_value<std::uint32_t>("max_message_size", m_default_max_message_size);
}

//==================================================================================================
std::chrono::milliseconds LoggerConfig::queue_wait_time() const
{
    return std::chrono::milliseconds(
        get_value<std::chrono::milliseconds::rep>("queue_wait_time", m_default_queue_wait_time));
}

} // namespace fly
