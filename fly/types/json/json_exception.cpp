#include "fly/types/json/json_exception.hpp"

#include "fly/types/json/json.hpp"

namespace fly {

//==================================================================================================
JsonException::JsonException(std::string &&message) noexcept :
    m_message(String::format("JsonException: {}", message))
{
}

//==================================================================================================
JsonException::JsonException(const Json &json, std::string &&message) noexcept :
    m_message(String::format("JsonException: {}: ({})", message, json))
{
}

//==================================================================================================
JsonException::JsonException(const char *class_name, std::string &&message) noexcept :
    m_message(String::format("{}: {}", class_name, message))
{
}

//==================================================================================================
const char *JsonException::what() const noexcept
{
    return m_message.c_str();
}

//==================================================================================================
JsonIteratorException::JsonIteratorException(const Json &json, std::string &&message) noexcept :
    JsonException("JsonIteratorException", String::format("{}: ({})", message, json))
{
}

//==================================================================================================
BadJsonComparisonException::BadJsonComparisonException(
    const Json &json1,
    const Json &json2) noexcept :
    JsonException(
        "BadJsonComparisonException",
        String::format(
            "Cannot compare iterators of different JSON instances: ({}) ({})",
            json1,
            json2))
{
}

//==================================================================================================
NullJsonException::NullJsonException(const Json &json) noexcept :
    JsonException(
        "NullJsonException",
        String::format("Cannot dereference an empty or past-the-end iterator: ({})", json))
{
}

//==================================================================================================
NullJsonException::NullJsonException() noexcept :
    JsonException(
        "NullJsonException",
        String::format("Cannot dereference an empty or past-the-end iterator"))
{
}

//==================================================================================================
OutOfRangeJsonException::OutOfRangeJsonException(const Json &json, std::ptrdiff_t offset) noexcept :
    JsonException(
        "OutOfRangeJsonException",
        String::format("Offset {} is out-of-range: ({})", offset, json)),
    m_offset(offset)
{
}

//==================================================================================================
std::ptrdiff_t OutOfRangeJsonException::offset() const
{
    return m_offset;
}

} // namespace fly
