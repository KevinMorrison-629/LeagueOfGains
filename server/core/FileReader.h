#pragma once

#include <fstream>
#include <string>

namespace Core::Utils
{
    /// @brief Reads the entire content of a file into a string.
    /// @param filepath The relative or absolute path to the file.
    /// @return A string containing the contents of the file.
    /// @throw std::runtime_error if the file cannot be opened.
    std::string ReadFile(const std::string &filepath)
    {
        std::ifstream inputFileStream(filepath);

        if (!inputFileStream.is_open())
        {
            throw std::runtime_error("Could not open file: " + filepath);
        }

        // Read the file into a string using stream iterators
        return std::string(std::istreambuf_iterator<char>(inputFileStream), std::istreambuf_iterator<char>());
    }
} // namespace Core::Utils