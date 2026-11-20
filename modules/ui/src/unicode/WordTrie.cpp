#include "unicode/WordTrie.h"

#include "unicode/GraphemeBreak.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace coral::ui::unicode {

namespace {

std::string decodeBase64(const std::string_view* chunks, const size_t count) {
  std::string encoded;
  for (size_t index = 0; index < count; ++index) encoded += chunks[index];
  std::string data;
  data.reserve(encoded.size() * 3 / 4);
  int bits = 0;
  int accumulator = 0;
  for (const char character: encoded) {
    int value = -1;
    if (character >= 'A' && character <= 'Z') value = character - 'A';
    else if (character >= 'a' && character <= 'z') value = character - 'a' + 26;
    else if (character >= '0' && character <= '9') value = character - '0' + 52;
    else if (character == '+') value = 62;
    else if (character == '/') value = 63;
    else continue;
    accumulator = (accumulator << 6) | value;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      data.push_back(static_cast<char>((accumulator >> bits) & 0xFF));
    }
  }
  return data;
}

} // namespace

DecodedWordTrie decodeWordTrie(const std::string_view* chunks, const size_t count) {
  const std::string data = decodeBase64(chunks, count);
  size_t offset = 0;
  const auto readU8 = [&]() -> uint8_t {
    return offset < data.size() ? static_cast<uint8_t>(data[offset++]) : 0;
  };
  const auto readU16 = [&]() -> uint16_t {
    const uint16_t low = readU8();
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(readU8()) << 8));
  };
  const auto readU32 = [&]() -> uint32_t {
    const uint32_t low = readU16();
    return low | (static_cast<uint32_t>(readU16()) << 16);
  };
  DecodedWordTrie decoded;
  const uint32_t nodes = readU32();
  decoded.edgeStarts.reserve(nodes);
  decoded.edgeCounts.reserve(nodes);
  decoded.terminals.reserve(nodes);
  for (uint32_t index = 0; index < nodes; ++index) {
    decoded.edgeStarts.push_back(readU32());
    decoded.edgeCounts.push_back(readU16());
    decoded.terminals.push_back(readU8());
  }
  const uint32_t edges = readU32();
  decoded.children.reserve(edges);
  decoded.labels.reserve(edges);
  for (uint32_t index = 0; index < edges; ++index) {
    decoded.children.push_back(readU32());
    decoded.labels.push_back(readU8());
  }
  decoded.root = readU32();
  return decoded;
}

DecodedWordTrie decodeWordTrie(const char* const* chunks, const size_t count) {
  std::vector<std::string_view> views;
  views.reserve(count);
  for (size_t index = 0; index < count; ++index) views.emplace_back(chunks[index]);
  return decodeWordTrie(views.data(), views.size());
}

size_t WordTrie::longestPrefix(const std::string_view text) const {
  uint32_t node = mRoot;
  if (node >= mNodeCount) return 0;
  size_t best = 0;
  for (size_t position = 0; position < text.size(); ++position) {
    const auto label = static_cast<unsigned char>(text[position]);
    const uint32_t start = mEdgeStarts[node];
    const uint16_t count = mEdgeCounts[node];
    if (start > mEdgeCount || count > mEdgeCount - start) break;
    const uint8_t* begin = mLabels + start;
    const uint8_t* end = begin + count;
    const uint8_t* found = std::lower_bound(begin, end, label);
    if (found == end || *found != label) break;
    node = mChildren[start + static_cast<size_t>(found - begin)];
    if (node >= mNodeCount) break;
    if (mTerminals[node] != 0) best = position + 1;
  }
  return best;
}

std::vector<size_t> scanWordTrie(const std::string_view text, const WordTrie& trie) {
  // Boundaries stay on grapheme clusters: a dictionary word may end in the
  // middle of a combining sequence in pathological data, and unmatched runs get
  // a fallback boundary at the next cluster so Thai/Lao/Burmese never become
  // one unbreakable line.
  const std::vector<size_t> graphemes = computeGraphemeBreaks(text);
  const auto isGraphemeBoundary = [&](const size_t offset) {
    return std::binary_search(graphemes.begin(), graphemes.end(), offset);
  };
  std::vector<size_t> boundaries;
  size_t offset = 0;
  while (offset < text.size()) {
    const size_t length = trie.longestPrefix(text.substr(offset));
    if (length > 0) {
      const size_t end = offset + length;
      if (end < text.size() && isGraphemeBoundary(end)) boundaries.push_back(end);
      offset = end;
      continue;
    }
    const auto next = std::upper_bound(graphemes.begin(), graphemes.end(), offset);
    if (next == graphemes.end()) break;
    if (*next < text.size()) boundaries.push_back(*next);
    offset = *next;
  }
  return boundaries;
}

} // namespace coral::ui::unicode
