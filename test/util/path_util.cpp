#include "test/util/path_util.hpp"

#include "fly/types/string/string.hpp"

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

namespace fly::test {

//==================================================================================================
PathUtil::ScopedTempDirectory::ScopedTempDirectory() :
    m_directory(std::filesystem::temp_directory_path() / fly::String::generate_random_string(10))
{
    CATCH_REQUIRE(std::filesystem::create_directories(m_directory));
}

//==================================================================================================
PathUtil::ScopedTempDirectory::~ScopedTempDirectory()
{
    CATCH_REQUIRE(std::filesystem::remove_all(m_directory) > 0);
}

//==================================================================================================
std::filesystem::path PathUtil::ScopedTempDirectory::operator()() const
{
    return m_directory;
}

//==================================================================================================
std::filesystem::path PathUtil::ScopedTempDirectory::file() const
{
    return m_directory / (fly::String::generate_random_string(10) + ".txt");
}

//==================================================================================================
bool PathUtil::write_file(const std::filesystem::path &path, const std::string &contents)
{
    std::ofstream stream(path, std::ios::out);

    if (stream.good())
    {
        stream << contents;
    }

    return stream.good();
}

//==================================================================================================
std::string PathUtil::read_file(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::in);
    std::stringstream sstream;

    if (stream.good())
    {
        sstream << stream.rdbuf();
    }

    return sstream.str();
}

//==================================================================================================
bool PathUtil::compare_files(const std::filesystem::path &path1, const std::filesystem::path &path2)
{
    if (std::filesystem::file_size(path1) != std::filesystem::file_size(path2))
    {
        return false;
    }

    std::ifstream file1(path1, std::ios::binary);
    std::ifstream file2(path2, std::ios::binary);

    std::istreambuf_iterator<char> begin1(file1);
    std::istreambuf_iterator<char> begin2(file2);

    return std::equal(begin1, std::istreambuf_iterator<char>(), begin2);
}

} // namespace fly::test
