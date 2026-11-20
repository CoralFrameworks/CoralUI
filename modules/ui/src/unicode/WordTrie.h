#ifndef CORAL_UI_UNICODE_WORDTRIE_H
#define CORAL_UI_UNICODE_WORDTRIE_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace coral::ui::unicode {

struct DecodedWordTrie {
  std::vector<uint32_t> edgeStarts;
  std::vector<uint16_t> edgeCounts;
  std::vector<uint8_t> terminals;
  std::vector<uint32_t> children;
  std::vector<uint8_t> labels;
  uint32_t root = 0;
};

/** Decodes the base64 blob emitted by generate_word_tries.py. */
[[nodiscard]] DecodedWordTrie decodeWordTrie(const std::string_view* chunks, size_t count);

/** C-string overload used by the generated tables (avoids huge constexpr string_views). */
[[nodiscard]] DecodedWordTrie decodeWordTrie(const char* const* chunks, size_t count);

/** Read-only byte trie/DAFSA over UTF-8 dictionary words. */
class WordTrie {
 public:
  constexpr WordTrie(const uint32_t* edgeStarts, const uint16_t* edgeCounts,
      const uint8_t* terminals, const uint32_t* children, const uint8_t* labels,
      const size_t nodeCount, const size_t edgeCount, const uint32_t root)
      : mEdgeStarts(edgeStarts), mEdgeCounts(edgeCounts), mTerminals(terminals),
        mChildren(children), mLabels(labels), mNodeCount(nodeCount), mEdgeCount(edgeCount),
        mRoot(root) {}

  /** @return byte length of the longest dictionary word starting at text[0], or 0. */
  [[nodiscard]] size_t longestPrefix(std::string_view text) const;

 private:
  const uint32_t* mEdgeStarts;
  const uint16_t* mEdgeCounts;
  const uint8_t* mTerminals;
  const uint32_t* mChildren;
  const uint8_t* mLabels;
  size_t mNodeCount;
  size_t mEdgeCount;
  uint32_t mRoot;
};

/** Streams maximal matching boundaries using @p trie. */
[[nodiscard]] std::vector<size_t> scanWordTrie(std::string_view text, const WordTrie& trie);

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_WORDTRIE_H
