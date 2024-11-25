#pragma once
#include <string>

bool TextHuffmanCompression(const std::string& text, std::string& result);
bool TextHuffmanDecompression(const std::string& huffman, std::string& text);