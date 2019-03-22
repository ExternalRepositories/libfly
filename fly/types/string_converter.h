#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fly {

/**
 * Helper class to convert a string type to a basic type, e.g. int or bool. This
 * class is only provided so specializations for each basic type can be defined.
 * Outside callers should use BasicString<StringType>::Convert().
 *
 * @author Timothy Flynn (trflynn89@gmail.com)
 * @version March 21, 2019
 */
template <typename StringType, typename T>
struct BasicStringConverter
{
    static T Convert(const StringType &) noexcept(
        std::is_same_v<StringType, std::decay_t<T>>);
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, StringType>
{
    static StringType Convert(const StringType &value) noexcept
    {
        return value;
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, bool>
{
    using value_type = bool;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("bool");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("bool");
        }

        return (result != 0);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, typename StringType::value_type>
{
    using value_type = typename StringType::value_type;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("char");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("char");
        }

        return static_cast<value_type>(result);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::int8_t>
{
    using value_type = std::int8_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::int8_t");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("std::int8_t");
        }

        return static_cast<value_type>(result);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::uint8_t>
{
    using value_type = std::uint8_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::uint8_t");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("std::uint8_t");
        }

        return static_cast<value_type>(result);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::int16_t>
{
    using value_type = std::int16_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::int16_t");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("std::int16_t");
        }

        return static_cast<value_type>(result);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::uint16_t>
{
    using value_type = std::uint16_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::uint16_t");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("std::uint16_t");
        }

        return static_cast<value_type>(result);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::int32_t>
{
    using value_type = std::int32_t;

    static auto Convert(const StringType &value) noexcept(false)
    {
        std::size_t index = 0;
        value_type result = std::stoi(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::int32_t");
        }

        return result;
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::uint32_t>
{
    using value_type = std::uint32_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        static constexpr long long min = std::numeric_limits<value_type>::min();
        static constexpr long long max = std::numeric_limits<value_type>::max();

        std::size_t index = 0;
        long long result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::uint32_t");
        }
        else if ((result < min) || (result > max))
        {
            throw std::out_of_range("std::uint32_t");
        }

        return static_cast<value_type>(result);
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::int64_t>
{
    using value_type = std::int64_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        std::size_t index = 0;
        value_type result = std::stoll(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::int64_t");
        }

        return result;
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, std::uint64_t>
{
    using value_type = std::uint64_t;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        std::size_t index = 0;
        value_type result = std::stoull(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("std::uint64_t");
        }

        return result;
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, float>
{
    using value_type = float;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        std::size_t index = 0;
        value_type result = std::stof(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("float");
        }

        return result;
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, double>
{
    using value_type = double;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        std::size_t index = 0;
        value_type result = std::stod(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("double");
        }

        return result;
    }
};

//==============================================================================
template <typename StringType>
struct BasicStringConverter<StringType, long double>
{
    using value_type = long double;

    static value_type Convert(const StringType &value) noexcept(false)
    {
        std::size_t index = 0;
        value_type result = std::stold(value, &index);

        if (index != value.length())
        {
            throw std::invalid_argument("ldouble");
        }

        return result;
    }
};

} // namespace fly
