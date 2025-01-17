#include "test/util/path_util.hpp"

#include "fly/coders/coder_config.hpp"
#include "fly/coders/huffman/huffman_decoder.hpp"
#include "fly/coders/huffman/huffman_encoder.hpp"
#include "fly/types/bit_stream/bit_stream_writer.hpp"
#include "fly/types/numeric/literals.hpp"
#include "fly/types/string/string.hpp"

#include "catch2/catch_test_macros.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

using namespace fly::literals::numeric_literals;

namespace {

/**
 * Subclass of the coder config to contain invalid values.
 */
class BadCoderConfig : public fly::coders::CoderConfig
{
public:
    BadCoderConfig() noexcept : fly::coders::CoderConfig()
    {
        m_default_huffman_encoder_max_code_length =
            std::numeric_limits<decltype(m_default_huffman_encoder_max_code_length)>::max();
    }
};

/**
 * Subclass of the Huffman coder config to reduce Huffman code lengths.
 */
class SmallCodeLengthConfig : public fly::coders::CoderConfig
{
public:
    SmallCodeLengthConfig() noexcept : fly::coders::CoderConfig()
    {
        m_default_huffman_encoder_max_code_length = 3;
    }
};

/**
 * Create a bitstream with the given bytes and number of remainder bits.
 */
std::string
create_stream_with_remainder(std::vector<fly::byte_type> bytes, fly::byte_type remainder)
{
    std::ostringstream stream(std::ios::out | std::ios::binary);
    fly::BitStreamWriter output(stream);

    for (const fly::byte_type &byte : bytes)
    {
        output.write_byte(byte);
    }
    if (remainder > 0_u8)
    {
        output.write_bits(0_u8, remainder);
    }

    CATCH_REQUIRE(output.finish());
    return stream.str();
}

/**
 * Create a bitstream with the given bytes and no remainder bits.
 */
std::string create_stream(std::vector<fly::byte_type> bytes)
{
    return create_stream_with_remainder(std::move(bytes), 0_u8);
}

} // namespace

CATCH_TEST_CASE("Huffman", "[coders]")
{
    auto config = std::make_shared<fly::coders::CoderConfig>();

    fly::coders::HuffmanEncoder encoder(config);
    fly::coders::HuffmanDecoder decoder;

    CATCH_SECTION("Cannot encode stream using an invalid configuration")
    {
        const std::string raw;
        std::string enc;

        config = std::make_shared<BadCoderConfig>();
        fly::coders::HuffmanEncoder bad_encoder(config);

        CATCH_CHECK_FALSE(bad_encoder.encode_string(raw, enc));
    }

    CATCH_SECTION("Cannot decode stream missing the encoder's version")
    {
        const std::string enc;
        std::string dec;

        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with an invalid encoder version")
    {
        std::vector<fly::byte_type> bytes = {
            0_u8, // Version
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream missing the encoder's configured chunk size")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with an invalid encoder chunk size")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            0_u8, // Chunk size KB (low)
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream missing the encoder's configured maximum code length")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with an encoder maximum code length that is too small")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            0_u8, // Maximum Huffman code length
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with an encoder maximum code length that is too large")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            255_u8, // Maximum Huffman code length
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream missing the encoder's code length count")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
        };

        const std::string enc = create_stream_with_remainder(std::move(bytes), 1_u8);
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with an encoder code length count that is too small")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
            0_u8, // Number of code length counts
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with an encoder code length count that is too large")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
            8_u8, // Number of code length counts
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with fewer code lengths than the code length count")
    {
        fly::byte_type number_of_code_length_counts = 5_u8;

        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
            number_of_code_length_counts, // Number of code length counts
        };

        for (fly::byte_type i = 0; i < number_of_code_length_counts; ++i)
        {
            const std::string enc = create_stream(bytes);
            std::string dec;

            CATCH_CHECK_FALSE(enc.empty());
            CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));

            bytes.push_back(0_u8);
            bytes.push_back(1_u8);
        }
    }

    CATCH_SECTION("Cannot decode stream missing the encoder's symbols")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
            2_u8, // Number of code length counts
            0_u8, // Code length count 1 (high)
            0_u8, // Code length count 1 (low)
            0_u8, // Code length count 2 (high)
            1_u8, // Code length count 2 (low)
        };

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with too many encoded Huffman codes")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
            2_u8, // Number of code length counts
            0_u8, // Code length count 1 (high)
            0_u8, // Code length count 1 (low)
            std::numeric_limits<std::uint8_t>::max(), // Code length count 2 (high)
            std::numeric_limits<std::uint8_t>::max(), // Code length count 2 (low)
        };

        for (auto i = 0; i < std::numeric_limits<std::uint16_t>::max(); ++i)
        {
            bytes.push_back(1_u8);
        }

        const std::string enc = create_stream(std::move(bytes));
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Cannot decode stream with too few encoded symbols")
    {
        std::vector<fly::byte_type> bytes = {
            1_u8, // Version
            0_u8, // Chunk size KB (high)
            1_u8, // Chunk size KB (low)
            4_u8, // Maximum Huffman code length
            1_u8, // Number of code length counts
            0_u8, // Code length count 1 (high)
            1_u8, // Code length count 1 (low),
            0x41, // Single symbol (A)
        };

        const std::string enc = create_stream_with_remainder(std::move(bytes), 1);
        std::string dec;

        CATCH_CHECK_FALSE(enc.empty());
        CATCH_CHECK_FALSE(decoder.decode_string(enc, dec));
    }

    CATCH_SECTION("Encode and decode empty stream")
    {
        const std::string raw;
        std::string enc, dec;

        CATCH_REQUIRE(encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw == dec);
    }

    CATCH_SECTION("Encode and decode a stream with a single symbol")
    {
        const std::string raw = "a";
        std::string enc, dec;

        CATCH_REQUIRE(encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw == dec);
    }

    CATCH_SECTION("Encode and decode a stream with a single symbol repeated")
    {
        const std::string raw = "aaaaaaaaaa";
        std::string enc, dec;

        CATCH_REQUIRE(encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw == dec);
    }

    CATCH_SECTION("Encode and decode a small stream")
    {
        const std::string raw = "abcdefabcbbb";
        std::string enc, dec;

        CATCH_REQUIRE(encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw == dec);
    }

    CATCH_SECTION("Encode and decode a large stream")
    {
        const std::string raw = fly::String::generate_random_string(100 << 10);
        std::string enc, dec;

        CATCH_REQUIRE(encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw.size() > enc.size());
        CATCH_CHECK(raw == dec);
    }

    CATCH_SECTION("Limit code lengths to a small value and validate the Kraft-McMillan inequality")
    {
        const std::string raw = "abcdefabcbbb";
        std::string enc, dec;

        config = std::make_shared<SmallCodeLengthConfig>();
        fly::coders::HuffmanEncoder limted_encoder(config);

        CATCH_REQUIRE(limted_encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw == dec);

        const auto max_allowed_kraft = (1_u16 << config->huffman_encoder_max_code_length()) - 1;
        CATCH_CHECK(decoder.compute_kraft_mcmillan_constant() <= max_allowed_kraft);
    }

    CATCH_SECTION("Encode and decode a stream with non-ASCII Unicode characters")
    {
        std::string raw = "🍕א😅😅🍕❤️א🍕";
        std::string enc, dec;

        for (int i = 0; i < 10; ++i)
        {
            raw += raw;
        }

        CATCH_REQUIRE(encoder.encode_string(raw, enc));
        CATCH_REQUIRE(decoder.decode_string(enc, dec));

        CATCH_CHECK(raw.size() > enc.size());
        CATCH_CHECK(raw == dec);
    }

    CATCH_SECTION("File tests")
    {
        fly::test::PathUtil::ScopedTempDirectory path;
        std::filesystem::path encoded_file = path.file();
        std::filesystem::path decoded_file = path.file();

        CATCH_SECTION("Encode and decode a large file containing only ASCII symbols")
        {
            // Generated with:
            // tr -dc '[:graph:]' </dev/urandom | head -c 4194304 > test.txt
            const auto here = std::filesystem::path(__FILE__).parent_path();
            const auto raw = here / "data" / "test.txt";

            CATCH_REQUIRE(encoder.encode_file(raw, encoded_file));
            CATCH_REQUIRE(decoder.decode_file(encoded_file, decoded_file));

            CATCH_CHECK(std::filesystem::file_size(raw) > std::filesystem::file_size(encoded_file));
            CATCH_CHECK(fly::test::PathUtil::compare_files(raw, decoded_file));
        }

        CATCH_SECTION("Encode and decode a large file containing ASCII and non-ASCII symbols")
        {
            // Generated with:
            // dd if=/dev/urandom of=test.bin count=1 bs=4194304
            const auto here = std::filesystem::path(__FILE__).parent_path();
            const auto raw = here / "data" / "test.bin";

            CATCH_REQUIRE(encoder.encode_file(raw, encoded_file));
            CATCH_REQUIRE(decoder.decode_file(encoded_file, decoded_file));

            CATCH_CHECK(fly::test::PathUtil::compare_files(raw, decoded_file));
        }
    }
}
