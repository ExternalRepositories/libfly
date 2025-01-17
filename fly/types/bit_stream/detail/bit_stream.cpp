#include "fly/types/bit_stream/detail/bit_stream.hpp"

namespace fly::detail {

//==================================================================================================
BitStream::BitStream(std::streambuf *stream_buffer, byte_type starting_position) noexcept :
    m_stream_buffer(stream_buffer),
    m_position(starting_position)
{
}

//==================================================================================================
BitStream::~BitStream() = default;

} // namespace fly::detail
