#pragma once

#include "fly/types/string/detail/string_traits.hpp"
#include "fly/types/string/string_literal.hpp"

namespace fly::detail {

/**
 * Helper class to provide safe alernatives to the STL's <cctype> methods.
 *
 * @author Timothy Flynn (trflynn89@pm.me)
 * @version January 3, 2021
 */
template <typename StringType>
class BasicStringClassifier
{
    using traits = detail::BasicStringTraits<StringType>;
    using char_type = typename traits::char_type;
    using int_type = typename traits::int_type;

public:
    /**
     * Checks if the given character is an alphabetic character as classified by the default C
     * locale.
     *
     * The STL's std::isalpha and std::iswalpha require that the provided character fits into an
     * unsigned char and unsigned wchar_t, respectively. Other values result in undefined behavior.
     * This method has no such restriction.
     *
     * @param ch The character to classify.
     *
     * @return True if the character is an alphabetic character.
     */
    static constexpr bool is_alpha(char_type ch);

    /**
     * Checks if the given character is an upper-case alphabetic character as classified by the
     * default C locale.
     *
     * The STL's std::isupper and std::iswupper require that the provided character fits into an
     * unsigned char and unsigned wchar_t, respectively. Other values result in undefined behavior.
     * This method has no such restriction.
     *
     * @param ch The character to classify.
     *
     * @return True if the character is an alphabetic character.
     */
    static constexpr bool is_upper(char_type ch);

    /**
     * Checks if the given character is a lower-case alphabetic character as classified by the
     * default C locale.
     *
     * The STL's std::islower and std::iswlower require that the provided character fits into an
     * unsigned char and unsigned wchar_t, respectively. Other values result in undefined behavior.
     * This method has no such restriction.
     *
     * @param ch The character to classify.
     *
     * @return True if the character is an alphabetic character.
     */
    static constexpr bool is_lower(char_type ch);

    /**
     * Checks if the given character is a decimal digit character.
     *
     * The STL's std::isdigit and std::iswdigit require that the provided character fits into an
     * unsigned char and unsigned wchar_t, respectively. Other values result in undefined behavior.
     * This method has no such restriction.
     *
     * @param ch The character to classify.
     *
     * @return True if the character is a decimal digit character.
     */
    static constexpr bool is_digit(char_type ch);

    /**
     * Checks if the given character is a hexadecimal digit character.
     *
     * The STL's std::isxdigit and std::iswxdigit require that the provided character fits into an
     * unsigned char and unsigned wchar_t, respectively. Other values result in undefined behavior.
     * This method has no such restriction.
     *
     * @param ch The character to classify.
     *
     * @return True if the character is a hexadecimal digit character.
     */
    static constexpr bool is_x_digit(char_type ch);

private:
    /**
     * Remove the 0x20 bit from the given character, effectively converting the a-z range of
     * characters to the A-Z range.
     *
     * @param ch The character to modify.
     *
     * @return The modified character.
     */
    static constexpr char_type unify_az_characters(char_type ch);

    static constexpr const char_type s_zero = FLY_CHR(char_type, '0');
    static constexpr const char_type s_upper_a = FLY_CHR(char_type, 'A');
    static constexpr const char_type s_upper_z = FLY_CHR(char_type, 'Z');
    static constexpr const char_type s_upper_f = FLY_CHR(char_type, 'F');
    static constexpr const char_type s_lower_a = FLY_CHR(char_type, 'a');
    static constexpr const char_type s_lower_z = FLY_CHR(char_type, 'z');

    static constexpr const int_type s_case_mask = static_cast<int_type>(~0x20);
};

//==================================================================================================
template <typename StringType>
constexpr inline bool BasicStringClassifier<StringType>::is_alpha(char_type ch)
{
    return is_upper(unify_az_characters(ch));
}

//==================================================================================================
template <typename StringType>
constexpr inline bool BasicStringClassifier<StringType>::is_upper(char_type ch)
{
    return (ch >= s_upper_a) && (ch <= s_upper_z);
}

//==================================================================================================
template <typename StringType>
constexpr inline bool BasicStringClassifier<StringType>::is_lower(char_type ch)
{
    return (ch >= s_lower_a) && (ch <= s_lower_z);
}

//==================================================================================================
template <typename StringType>
constexpr inline bool BasicStringClassifier<StringType>::is_digit(char_type ch)
{
    return (ch ^ s_zero) < 10;
}

//==================================================================================================
template <typename StringType>
constexpr inline bool BasicStringClassifier<StringType>::is_x_digit(char_type ch)
{
    const auto alpha = unify_az_characters(ch);
    return is_digit(ch) || ((alpha >= s_upper_a) && (alpha <= s_upper_f));
}

//==================================================================================================
template <typename StringType>
constexpr inline auto BasicStringClassifier<StringType>::unify_az_characters(char_type ch)
    -> char_type
{
    return static_cast<char_type>(static_cast<int_type>(ch) & s_case_mask);
}

} // namespace fly::detail
