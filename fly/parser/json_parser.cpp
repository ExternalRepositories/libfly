#include "fly/parser/json_parser.hpp"

#include "fly/fly.hpp"
#include "fly/logger/logger.hpp"

namespace fly::parser {

#define JLOG(...)                                                                                  \
    LOGW(                                                                                          \
        "[line {}, column {}]: " FLY_FORMAT_STRING(__VA_ARGS__),                                   \
        line(),                                                                                    \
        column() FLY_FORMAT_ARGS(__VA_ARGS__));

namespace {

    using JsonString = fly::JsonTraits::StringType;

    inline bool
    is_feature_enabled(JsonParser::Features enabled_features, JsonParser::Features feature)
    {
        return (enabled_features & feature) != JsonParser::Features::Strict;
    }

} // namespace

//==================================================================================================
JsonParser::JsonParser(const Features features) noexcept :
    Parser(),
    m_allow_comments(is_feature_enabled(features, Features::AllowComments)),
    m_allow_trailing_comma(is_feature_enabled(features, Features::AllowTrailingComma)),
    m_allow_any_type(is_feature_enabled(features, Features::AllowAnyType))
{
}

//==================================================================================================
std::optional<fly::Json> JsonParser::parse_internal()
{
    std::optional<fly::Json> json;

    try
    {
        json = parse_json();
    }
    catch (const JsonException &ex)
    {
        JLOG("{}", ex.what());
        return std::nullopt;
    }

    if (consume_whitespace_and_comments() == ParseState::Invalid)
    {
        return std::nullopt;
    }
    else if (json)
    {
        if (!eof())
        {
            JLOG("Extraneous symbols found after JSON value: {:#04x}", peek());
            return std::nullopt;
        }
        else if (!json->is_object() && !json->is_array() && !m_allow_any_type)
        {
            JLOG("Parsed non-object/non-array value, but Features::AllowAnyType is not enabled");
            return std::nullopt;
        }
    }

    return json;
}

//==================================================================================================
std::optional<fly::Json> JsonParser::parse_json()
{
    if (consume_whitespace_and_comments() == ParseState::Invalid)
    {
        return std::nullopt;
    }

    switch (peek<Token>())
    {
        case Token::StartBrace:
            return parse_object();

        case Token::StartBracket:
            return parse_array();

        case Token::Quote:
            if (auto value = parse_quoted_string(); value)
            {
                return *std::move(value);
            }

            return std::nullopt;

        default:
            return parse_value();
    }
}

//==================================================================================================
std::optional<fly::Json> JsonParser::parse_object()
{
    static constexpr const Token s_end_token = Token::CloseBrace;

    fly::Json object = fly::JsonTraits::object_type();
    ParseState state;

    // Discard the opening brace, which has already been peeked.
    discard();

    while ((state = state_for_object_or_array(s_end_token)) == ParseState::KeepParsing)
    {
        if (object && ((state = consume_comma(s_end_token)) != ParseState::KeepParsing))
        {
            break;
        }

        std::optional<fly::JsonTraits::string_type> key = parse_quoted_string();

        if (!key || (consume_token(Token::Colon) == ParseState::Invalid))
        {
            return std::nullopt;
        }
        else if (std::optional<fly::Json> value = parse_json(); value)
        {
            object.insert_or_assign(*std::move(key), *std::move(value));
        }
        else
        {
            return std::nullopt;
        }
    }

    return (state == ParseState::Invalid) ? std::nullopt :
                                            std::optional<fly::Json>(std::move(object));
}

//==================================================================================================
std::optional<fly::Json> JsonParser::parse_array()
{
    static constexpr const Token s_end_token = Token::CloseBracket;

    fly::Json array = fly::JsonTraits::array_type();
    ParseState state;

    // Discard the opening bracket, which has already been peeked.
    discard();

    while ((state = state_for_object_or_array(s_end_token)) == ParseState::KeepParsing)
    {
        if (array && ((state = consume_comma(s_end_token)) != ParseState::KeepParsing))
        {
            break;
        }

        if (std::optional<fly::Json> value = parse_json(); value)
        {
            array.push_back(*std::move(value));
        }
        else
        {
            return std::nullopt;
        }
    }

    return (state == ParseState::Invalid) ? std::nullopt :
                                            std::optional<fly::Json>(std::move(array));
}

//==================================================================================================
JsonParser::ParseState JsonParser::state_for_object_or_array(Token end_token)
{
    if (consume_whitespace_and_comments() == ParseState::Invalid)
    {
        return ParseState::Invalid;
    }

    const Token token = peek<Token>();

    if (token == end_token)
    {
        discard();
        return ParseState::StopParsing;
    }
    else if (token == Token::EndOfFile)
    {
        return ParseState::Invalid;
    }

    return ParseState::KeepParsing;
}

//==================================================================================================
std::optional<fly::JsonTraits::string_type> JsonParser::parse_quoted_string()
{
    fly::JsonTraits::string_type value;
    Token token;

    if (consume_token(Token::Quote) == ParseState::Invalid)
    {
        return std::nullopt;
    }

    while ((token = get<Token>()) != Token::Quote)
    {
        value.push_back(static_cast<fly::JsonTraits::char_type>(token));

        if (token == Token::ReverseSolidus)
        {
            // Blindly ignore escaped symbols, the fly::Json class will check whether they are
            // valid. Just read at least one more symbol to prevent breaking out of the loop too
            // early if the next symbol is a quote.
            value.push_back(get<fly::JsonTraits::char_type>());
        }
        else if (token == Token::EndOfFile)
        {
            return std::nullopt;
        }
    }

    return value;
}

//==================================================================================================
std::optional<fly::Json> JsonParser::parse_value()
{
    const fly::JsonTraits::string_type value = consume_value();

    if (value == FLY_JSON_STR("true"))
    {
        return true;
    }
    else if (value == FLY_JSON_STR("false"))
    {
        return false;
    }
    else if (value == FLY_JSON_STR("null"))
    {
        return nullptr;
    }

    switch (validate_number(value))
    {
        case NumberType::SignedInteger:
            if (auto num = JsonString::convert<fly::JsonTraits::signed_type>(value); num)
            {
                return *num;
            }
            break;

        case NumberType::UnsignedInteger:
            if (auto num = JsonString::convert<fly::JsonTraits::unsigned_type>(value); num)
            {
                return *num;
            }
            break;

        case NumberType::FloatingPoint:
            if (auto num = JsonString::convert<fly::JsonTraits::float_type>(value); num)
            {
                return *num;
            }
            break;

        default:
            break;
    }

    JLOG("Could not convert '{}' to a JSON value", value);
    return std::nullopt;
}

//==================================================================================================
JsonParser::ParseState JsonParser::consume_token(Token token)
{
    consume_whitespace();

    if (const Token parsed = get<Token>(); parsed != token)
    {
        JLOG("Unexpected character {}, was expecting {}", parsed, token);
        return ParseState::Invalid;
    }

    return ParseState::KeepParsing;
}

//==================================================================================================
JsonParser::ParseState JsonParser::consume_comma(Token end_token)
{
    if (consume_token(Token::Comma) == ParseState::Invalid)
    {
        return ParseState::Invalid;
    }
    else if (state_for_object_or_array(end_token) == ParseState::StopParsing)
    {
        if (m_allow_trailing_comma)
        {
            return ParseState::StopParsing;
        }

        JLOG("Found trailing comma, but Features::AllowTrailingComma is not enabled");
        return ParseState::Invalid;
    }

    return ParseState::KeepParsing;
}

//==================================================================================================
fly::JsonTraits::string_type JsonParser::consume_value()
{
    fly::JsonTraits::string_type value;

    auto keep_parsing = [this](Token token) -> bool
    {
        switch (token)
        {
            case Token::Comma:
            case Token::Solidus:
            case Token::CloseBracket:
            case Token::CloseBrace:
            case Token::EndOfFile:
                return false;

            default:
                return !is_whitespace(token);
        }
    };

    while (keep_parsing(peek<Token>()))
    {
        value.push_back(get<fly::JsonTraits::char_type>());
    }

    return value;
}

//==================================================================================================
JsonParser::ParseState JsonParser::consume_whitespace_and_comments()
{
    consume_whitespace();

    while (peek<Token>() == Token::Solidus)
    {
        if (consume_comment() == ParseState::Invalid)
        {
            return ParseState::Invalid;
        }

        consume_whitespace();
    }

    return ParseState::KeepParsing;
}

//==================================================================================================
void JsonParser::consume_whitespace()
{
    while (is_whitespace(peek<Token>()))
    {
        discard();
    }
}

//==================================================================================================
JsonParser::ParseState JsonParser::consume_comment()
{
    if (!m_allow_comments)
    {
        JLOG("Found comment, but Features::AllowComments is not enabled");
        return ParseState::Invalid;
    }

    // Discard the opening solidus, which has already been peeked.
    discard();

    Token token;

    switch (token = get<Token>())
    {
        case Token::Solidus:
            do
            {
                token = get<Token>();
            } while ((token != Token::EndOfFile) && (token != Token::NewLine));

            break;

        case Token::Asterisk:
        {
            bool parsing_comment = true;

            do
            {
                token = get<Token>();

                if ((token == Token::Asterisk) && (peek<Token>() == Token::Solidus))
                {
                    parsing_comment = false;
                    discard();
                    break;
                }
            } while (token != Token::EndOfFile);

            if (parsing_comment)
            {
                return ParseState::Invalid;
            }

            break;
        }

        default:
            JLOG("Invalid start sequence for comments: {}", token);
            return ParseState::Invalid;
    }

    return ParseState::KeepParsing;
}

//==================================================================================================
JsonParser::NumberType JsonParser::validate_number(const fly::JsonTraits::string_type &value) const
{
    const JsonString::view_type value_view = value;

    const bool is_signed = !value_view.empty() && (value_view[0] == '-');
    const auto signless = value_view.substr(is_signed ? 1 : 0);

    if (signless.empty())
    {
        return NumberType::Invalid;
    }

    const bool is_octal =
        (signless.size() > 1) && (signless[0] == '0') && JsonString::is_digit(signless[1]);

    if (!JsonString::is_digit(signless[0]) || is_octal)
    {
        return NumberType::Invalid;
    }

    const fly::JsonTraits::string_type::size_type d = signless.find('.');
    const fly::JsonTraits::string_type::size_type e1 = signless.find('e');
    const fly::JsonTraits::string_type::size_type e2 = signless.find('E');

    if (d != fly::JsonTraits::string_type::npos)
    {
        fly::JsonTraits::string_type::size_type end = signless.size();

        if ((e1 != fly::JsonTraits::string_type::npos) ||
            (e2 != fly::JsonTraits::string_type::npos))
        {
            end = std::min(e1, e2);
        }

        if ((d + 1) >= end)
        {
            return NumberType::Invalid;
        }

        return NumberType::FloatingPoint;
    }
    else if (
        (e1 != fly::JsonTraits::string_type::npos) || (e2 != fly::JsonTraits::string_type::npos))
    {
        return NumberType::FloatingPoint;
    }

    return is_signed ? NumberType::SignedInteger : NumberType::UnsignedInteger;
}

//==================================================================================================
bool JsonParser::is_whitespace(Token token) const
{
    switch (token)
    {
        case Token::Tab:
        case Token::NewLine:
        case Token::VerticalTab:
        case Token::CarriageReturn:
        case Token::Space:
            return true;

        default:
            return false;
    }
}

//==================================================================================================
JsonParser::Features operator&(JsonParser::Features a, JsonParser::Features b)
{
    return static_cast<JsonParser::Features>(
        static_cast<std::underlying_type_t<JsonParser::Features>>(a) &
        static_cast<std::underlying_type_t<JsonParser::Features>>(b));
}

//==================================================================================================
JsonParser::Features operator|(JsonParser::Features a, JsonParser::Features b)
{
    return static_cast<JsonParser::Features>(
        static_cast<std::underlying_type_t<JsonParser::Features>>(a) |
        static_cast<std::underlying_type_t<JsonParser::Features>>(b));
}

} // namespace fly::parser
