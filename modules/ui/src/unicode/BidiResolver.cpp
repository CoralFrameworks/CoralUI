/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BidiResolver.h"

#include "unicode/BidiBrackets.generated.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

namespace coral::ui::unicode {

namespace {

constexpr uint8_t kMaxExplicitLevel = 125;

struct Item {
  BidiClass original = BidiClass::L;
  BidiClass type = BidiClass::L;
  uint8_t level = 0;
  bool removed = false;
};

struct StackEntry {
  uint8_t level = 0;
  BidiClass overrideType = BidiClass::ON; // ON means no override
  bool isolate = false;
};

bool isIsolateInitiator(const BidiClass value) {
  return value == BidiClass::LRI || value == BidiClass::RLI || value == BidiClass::FSI;
}

bool isIsolateFormatting(const BidiClass value) { return isIsolateInitiator(value) || value == BidiClass::PDI; }

bool isNeutral(const BidiClass value) {
  return value == BidiClass::B || value == BidiClass::S || value == BidiClass::WS || value == BidiClass::ON ||
         isIsolateFormatting(value);
}

bool isStrong(const BidiClass value) {
  return value == BidiClass::L || value == BidiClass::R || value == BidiClass::AL;
}

uint8_t nextEvenLevel(const uint8_t level) { return static_cast<uint8_t>((level + 2) & ~1u); }

uint8_t nextOddLevel(const uint8_t level) { return static_cast<uint8_t>(level % 2 == 0 ? level + 1 : level + 2); }

BidiClass directionForLevel(const uint8_t level) { return level % 2 == 0 ? BidiClass::L : BidiClass::R; }

BidiClass strongForNeutral(const BidiClass value) {
  if (value == BidiClass::L) return BidiClass::L;
  if (value == BidiClass::R || value == BidiClass::EN || value == BidiClass::AN) {
    return BidiClass::R;
  }
  return value;
}

uint32_t canonicalBracket(const uint32_t codePoint) {
  // N0 requires canonical equivalence for these two angle-bracket pairs.
  if (codePoint == 0x2329u) return 0x3008u;
  if (codePoint == 0x232Au) return 0x3009u;
  return codePoint;
}

int firstStrongClass(const std::vector<Item>& items, const size_t begin, const size_t end) {
  int depth = 0;
  for (size_t index = begin; index < end; ++index) {
    const BidiClass value = items[index].original;
    if (isIsolateInitiator(value)) {
      ++depth;
    } else if (value == BidiClass::PDI) {
      if (depth > 0) --depth;
    } else if (depth == 0) {
      if (value == BidiClass::L) return static_cast<int>(BidiClass::L);
      if (value == BidiClass::R || value == BidiClass::AL) {
        return static_cast<int>(BidiClass::R);
      }
    }
  }
  return -1;
}

uint8_t chooseBaseLevel(const std::vector<Item>& items, const int paragraphDirection) {
  if (paragraphDirection == 0) return 0;
  if (paragraphDirection == 1) return 1;
  const int strong = firstStrongClass(items, 0, items.size());
  return strong == static_cast<int>(BidiClass::R) ? 1 : 0;
}

void applyWeakRules(std::vector<BidiClass>& types, const BidiClass sos) {
  const size_t count = types.size();
  // W1
  BidiClass previous = sos;
  for (size_t index = 0; index < count; ++index) {
    if (types[index] == BidiClass::NSM) {
      types[index] = isIsolateFormatting(previous) ? BidiClass::ON : previous;
    }
    previous = types[index];
  }

  // W2
  BidiClass lastStrong = sos;
  for (size_t index = 0; index < count; ++index) {
    if (isStrong(types[index])) {
      lastStrong = types[index];
    } else if (types[index] == BidiClass::EN && lastStrong == BidiClass::AL) {
      types[index] = BidiClass::AN;
    }
  }

  // W3
  for (BidiClass& value: types) {
    if (value == BidiClass::AL) value = BidiClass::R;
  }

  // W4
  for (size_t index = 1; index + 1 < count; ++index) {
    if (types[index] == BidiClass::ES && types[index - 1] == BidiClass::EN && types[index + 1] == BidiClass::EN) {
      types[index] = BidiClass::EN;
    } else if (types[index] == BidiClass::CS) {
      if (types[index - 1] == BidiClass::EN && types[index + 1] == BidiClass::EN) {
        types[index] = BidiClass::EN;
      } else if (types[index - 1] == BidiClass::AN && types[index + 1] == BidiClass::AN) {
        types[index] = BidiClass::AN;
      }
    }
  }

  // W5
  for (size_t index = 0; index < count; ++index) {
    if (types[index] != BidiClass::ET) continue;
    size_t end = index;
    while (end < count && types[end] == BidiClass::ET) ++end;
    const bool adjacent =
            (index > 0 && types[index - 1] == BidiClass::EN) || (end < count && types[end] == BidiClass::EN);
    if (adjacent) {
      for (size_t position = index; position < end; ++position) {
        types[position] = BidiClass::EN;
      }
    }
    index = end == 0 ? index : end - 1;
  }

  // W6
  for (BidiClass& value: types) {
    if (value == BidiClass::ES || value == BidiClass::ET || value == BidiClass::CS) {
      value = BidiClass::ON;
    }
  }

  // W7
  lastStrong = sos;
  for (size_t index = 0; index < count; ++index) {
    if (types[index] == BidiClass::L || types[index] == BidiClass::R) {
      lastStrong = types[index];
    } else if (types[index] == BidiClass::EN && lastStrong == BidiClass::L) {
      types[index] = BidiClass::L;
    }
  }
}

void applyBracketRule(std::vector<BidiClass>& types, const std::vector<uint32_t>& codePoints,
        const std::vector<size_t>& indices, const BidiClass sos, const uint8_t level) {
  struct OpenBracket {
    size_t position = 0;
    uint32_t expectedClose = 0;
  };
  std::vector<OpenBracket> stack;
  std::vector<std::pair<size_t, size_t>> pairs;
  bool overflow = false;

  for (size_t position = 0; position < indices.size(); ++position) {
    const uint32_t codePoint = codePoints[indices[position]];
    if (types[position] != BidiClass::ON) continue;
    const BidiBracket* bracket = bidiBracket(codePoint);
    if (bracket == nullptr) continue;
    if (bracket->open) {
      if (stack.size() >= 63) {
        overflow = true;
        break;
      }
      stack.push_back({ position, canonicalBracket(bracket->pair) });
    } else {
      const uint32_t close = canonicalBracket(codePoint);
      for (size_t depth = stack.size(); depth > 0; --depth) {
        if (stack[depth - 1].expectedClose == close) {
          pairs.emplace_back(stack[depth - 1].position, position);
          stack.resize(depth - 1);
          break;
        }
      }
    }
  }
  if (overflow) return;
  std::sort(pairs.begin(), pairs.end());

  const BidiClass embedding = directionForLevel(level);
  const BidiClass opposite = embedding == BidiClass::L ? BidiClass::R : BidiClass::L;
  for (const auto& pair: pairs) {
    bool embeddingInside = false;
    bool oppositeInside = false;
    for (size_t position = pair.first + 1; position < pair.second; ++position) {
      const BidiClass value = types[position];
      if (value == embedding || (embedding == BidiClass::R && (value == BidiClass::EN || value == BidiClass::AN))) {
        embeddingInside = true;
      }
      if (value == opposite || (opposite == BidiClass::R && (value == BidiClass::EN || value == BidiClass::AN))) {
        oppositeInside = true;
      }
    }
    BidiClass direction = BidiClass::ON;
    if (embeddingInside) {
      direction = embedding;
    } else if (oppositeInside) {
      BidiClass preceding = sos;
      for (size_t position = pair.first; position > 0; --position) {
        const BidiClass value = types[position - 1];
        if (value == BidiClass::L || value == BidiClass::R || value == BidiClass::EN || value == BidiClass::AN) {
          preceding = strongForNeutral(value);
          break;
        }
      }
      direction = preceding == opposite ? opposite : embedding;
    }
    if (direction == BidiClass::ON) continue;
    types[pair.first] = direction;
    types[pair.second] = direction;
    // N0: original NSM characters immediately following a bracket that changed
    // under N0 take the bracket's direction.
    for (size_t position = pair.first + 1;
            position < types.size() && bidiClass(codePoints[indices[position]]) == BidiClass::NSM; ++position) {
      types[position] = direction;
    }
    for (size_t position = pair.second + 1;
            position < types.size() && bidiClass(codePoints[indices[position]]) == BidiClass::NSM; ++position) {
      types[position] = direction;
    }
  }
}

void applyNeutralRules(std::vector<BidiClass>& types, const BidiClass sos, const BidiClass eos, const uint8_t level) {
  for (BidiClass& value: types) {
    if (isIsolateFormatting(value)) value = BidiClass::ON;
  }
  const BidiClass embedding = directionForLevel(level);
  size_t index = 0;
  while (index < types.size()) {
    if (!isNeutral(types[index])) {
      ++index;
      continue;
    }
    size_t end = index;
    while (end < types.size() && isNeutral(types[end])) ++end;
    const BidiClass before = index == 0 ? sos : strongForNeutral(types[index - 1]);
    const BidiClass after = end == types.size() ? eos : strongForNeutral(types[end]);
    const BidiClass direction = before == after ? before : embedding;
    for (size_t position = index; position < end; ++position) {
      types[position] = direction;
    }
    index = end;
  }
}

void applyImplicitLevels(std::vector<Item>& items, const std::vector<size_t>& indices,
        const std::vector<BidiClass>& types) {
  for (size_t position = 0; position < indices.size(); ++position) {
    Item& item = items[indices[position]];
    const BidiClass type = types[position];
    if (item.level % 2 == 0) {
      if (type == BidiClass::R) {
        item.level = static_cast<uint8_t>(item.level + 1);
      } else if (type == BidiClass::EN || type == BidiClass::AN) {
        item.level = static_cast<uint8_t>(item.level + 2);
      }
    } else if (type == BidiClass::L || type == BidiClass::EN || type == BidiClass::AN) {
      item.level = static_cast<uint8_t>(item.level + 1);
    }
  }
}

void applyL1(std::vector<Item>& items, const uint8_t baseLevel) {
  const size_t count = items.size();
  for (size_t index = 0; index < count; ++index) {
    if (items[index].removed) continue;
    const BidiClass original = items[index].original;
    if (original == BidiClass::B || original == BidiClass::S) {
      items[index].level = baseLevel;
    }
  }
  for (size_t index = 0; index < count; ++index) {
    if (items[index].removed) continue;
    if (items[index].original != BidiClass::B && items[index].original != BidiClass::S) continue;
    for (size_t position = index; position > 0; --position) {
      if (items[position - 1].removed) continue;
      const BidiClass original = items[position - 1].original;
      if (original != BidiClass::WS && !isIsolateFormatting(original)) break;
      items[position - 1].level = baseLevel;
    }
  }
  for (size_t position = count; position > 0; --position) {
    if (items[position - 1].removed) continue;
    const BidiClass original = items[position - 1].original;
    if (original != BidiClass::WS && !isIsolateFormatting(original)) break;
    items[position - 1].level = baseLevel;
  }
}

std::vector<size_t> reorderL2(const std::vector<Item>& items, const std::vector<size_t>& active) {
  std::vector<size_t> order = active;
  if (order.empty()) return order;
  uint8_t highest = 0;
  for (const size_t index: order) highest = std::max(highest, items[index].level);
  uint8_t lowestOdd = 0;
  bool hasOdd = false;
  for (const size_t index: order) {
    if (items[index].level % 2 != 0) {
      lowestOdd = hasOdd ? std::min<uint8_t>(lowestOdd, items[index].level) : items[index].level;
      hasOdd = true;
    }
  }
  if (!hasOdd) return order;
  for (int level = highest; level >= static_cast<int>(lowestOdd); --level) {
    size_t index = 0;
    while (index < order.size()) {
      if (items[order[index]].level < level) {
        ++index;
        continue;
      }
      size_t end = index;
      while (end < order.size() && items[order[end]].level >= level) ++end;
      std::reverse(order.begin() + static_cast<std::ptrdiff_t>(index),
              order.begin() + static_cast<std::ptrdiff_t>(end));
      index = end;
    }
  }
  return order;
}

} // namespace

BidiParagraphResult resolveBidiParagraph(const std::vector<uint32_t>& codePoints, const int paragraphDirection) {
  BidiParagraphResult result;
  const size_t count = codePoints.size();
  result.levels.assign(count, 0);
  if (count == 0) {
    result.baseLevel = paragraphDirection == 1 ? 1 : 0;
    return result;
  }

  // With no R/AL/EN/AN, no explicit formatting and an LTR base, every level is
  // 0 and the visual order is the logical order. Detect that before allocating
  // the per-code-point Item array; this is the common Latin/CJK/emoji case.
  if (paragraphDirection != 1) {
    bool simpleLtr = true;
    for (const uint32_t codePoint: codePoints) {
      switch (bidiClass(codePoint)) {
        case BidiClass::L:
        case BidiClass::ES:
        case BidiClass::ET:
        case BidiClass::CS:
        case BidiClass::NSM:
        case BidiClass::ON:
        case BidiClass::WS:
          break;
        default:
          simpleLtr = false;
          break;
      }
      if (!simpleLtr) break;
    }
    if (simpleLtr) {
      result.baseLevel = 0;
      result.levels.assign(count, 0);
      result.visualOrder.resize(count);
      std::iota(result.visualOrder.begin(), result.visualOrder.end(), 0);
      return result;
    }
  }

  std::vector<Item> items(count);
  bool hasIsolates = false;
  for (size_t index = 0; index < count; ++index) {
    items[index].original = bidiClass(codePoints[index]);
    items[index].type = items[index].original;
    if (isIsolateInitiator(items[index].original)) hasIsolates = true;
  }
  result.baseLevel = chooseBaseLevel(items, paragraphDirection);

  // Matching PDI, used by FSI and X10. Most paragraphs have no isolate at all;
  // skip the O(n) scan and the 4-byte-per-codepoint map in that case.
  std::vector<int> matchingPdi;
  if (hasIsolates) {
    matchingPdi.assign(count, -1);
    std::vector<size_t> stack;
    for (size_t index = 0; index < count; ++index) {
      if (isIsolateInitiator(items[index].original)) {
        stack.push_back(index);
      } else if (items[index].original == BidiClass::PDI && !stack.empty()) {
        matchingPdi[stack.back()] = static_cast<int>(index);
        stack.pop_back();
      }
    }
  }

  // X1-X8: resolve explicit levels and overrides.
  std::vector<StackEntry> stack;
  stack.push_back({ result.baseLevel, BidiClass::ON, false });
  int validIsolateCount = 0;
  int overflowIsolateCount = 0;
  int overflowEmbeddingCount = 0;
  for (size_t index = 0; index < count; ++index) {
    Item& item = items[index];
    const BidiClass original = item.original;
    switch (original) {
      case BidiClass::RLE:
      case BidiClass::LRE:
      case BidiClass::RLO:
      case BidiClass::LRO: {
        const uint8_t newLevel = (original == BidiClass::RLE || original == BidiClass::RLO)
                                         ? nextOddLevel(stack.back().level)
                                         : nextEvenLevel(stack.back().level);
        item.removed = true;
        item.level = kBidiRemovedLevel;
        if (newLevel <= kMaxExplicitLevel && overflowIsolateCount == 0 && overflowEmbeddingCount == 0) {
          BidiClass overrideType = BidiClass::ON;
          if (original == BidiClass::RLO) overrideType = BidiClass::R;
          if (original == BidiClass::LRO) overrideType = BidiClass::L;
          stack.push_back({ newLevel, overrideType, false });
        } else if (overflowIsolateCount == 0) {
          ++overflowEmbeddingCount;
        }
        continue;
      }
      case BidiClass::RLI:
      case BidiClass::LRI:
      case BidiClass::FSI: {
        item.level = stack.back().level;
        if (stack.back().overrideType != BidiClass::ON) {
          item.type = stack.back().overrideType;
        }
        bool rightToLeft = original == BidiClass::RLI;
        if (original == BidiClass::FSI) {
          const int pdi = matchingPdi.empty() ? -1 : matchingPdi[index];
          const size_t end = pdi >= 0 ? static_cast<size_t>(pdi) : count;
          const int strong = firstStrongClass(items, index + 1, end);
          // X5c applies P2/P3 to the isolate contents; P3 defaults to LTR, so
          // content without a strong type behaves like LRI.
          rightToLeft = strong == static_cast<int>(BidiClass::R);
        }
        const uint8_t newLevel = rightToLeft ? nextOddLevel(stack.back().level) : nextEvenLevel(stack.back().level);
        if (newLevel <= kMaxExplicitLevel && overflowIsolateCount == 0 && overflowEmbeddingCount == 0) {
          stack.push_back({ newLevel, BidiClass::ON, true });
          ++validIsolateCount;
        } else {
          ++overflowIsolateCount;
        }
        continue;
      }
      case BidiClass::PDI: {
        if (overflowIsolateCount > 0) {
          --overflowIsolateCount;
        } else if (validIsolateCount > 0) {
          overflowEmbeddingCount = 0;
          while (stack.size() > 1 && !stack.back().isolate) stack.pop_back();
          if (stack.size() > 1 && stack.back().isolate) stack.pop_back();
          --validIsolateCount;
        }
        item.level = stack.back().level;
        // X6a: the PDI uses the outer override status, if any.
        if (stack.back().overrideType != BidiClass::ON) {
          item.type = stack.back().overrideType;
        }
        continue;
      }
      case BidiClass::PDF:
        if (overflowIsolateCount > 0) {
          // Do nothing.
        } else if (overflowEmbeddingCount > 0) {
          --overflowEmbeddingCount;
        } else if (stack.size() > 1 && !stack.back().isolate) {
          stack.pop_back();
        }
        item.removed = true;
        item.level = kBidiRemovedLevel;
        continue;
      case BidiClass::BN:
        item.removed = true;
        item.level = kBidiRemovedLevel;
        continue;
      case BidiClass::B:
        item.level = result.baseLevel;
        stack.resize(1);
        stack.back().level = result.baseLevel;
        validIsolateCount = 0;
        overflowIsolateCount = 0;
        overflowEmbeddingCount = 0;
        continue;
      default:
        item.level = stack.back().level;
        if (stack.back().overrideType != BidiClass::ON) {
          item.type = stack.back().overrideType;
        }
        continue;
    }
  }

  // X9: active characters and level runs. X10 must use the levels assigned by the
  // explicit rules, before I1/I2 mutate them.
  std::vector<uint8_t> explicitLevels(count, kBidiRemovedLevel);
  for (size_t index = 0; index < count; ++index) {
    if (!items[index].removed) explicitLevels[index] = items[index].level;
  }
  std::vector<size_t> active;
  active.reserve(count);
  std::vector<int> activePosition;
  if (hasIsolates) activePosition.assign(count, -1);
  for (size_t index = 0; index < count; ++index) {
    if (items[index].removed) continue;
    if (hasIsolates) activePosition[index] = static_cast<int>(active.size());
    active.push_back(index);
  }
  if (active.empty()) {
    for (size_t index = 0; index < count; ++index) {
      result.levels[index] = items[index].removed ? kBidiRemovedLevel : items[index].level;
    }
    return result;
  }

  struct Run {
    size_t start = 0;
    size_t end = 0;
  };
  std::vector<Run> runs;
  std::vector<int> runOfActive;
  if (hasIsolates) runOfActive.assign(active.size(), -1);
  for (size_t position = 0; position < active.size();) {
    const uint8_t level = explicitLevels[active[position]];
    size_t end = position + 1;
    while (end < active.size() && explicitLevels[active[end]] == level) ++end;
    const int runIndex = static_cast<int>(runs.size());
    runs.push_back({ position, end });
    if (hasIsolates) {
      for (size_t inside = position; inside < end; ++inside) runOfActive[inside] = runIndex;
    }
    position = end;
  }

  // X10: isolating run sequences. Process each sequence immediately and reuse
  // the index/type buffers; mixed-script paragraphs otherwise allocate once per
  // run, which dominated the cold path.
  std::vector<char> used(runs.size(), 0);
  std::vector<size_t> indices;
  indices.reserve(count);
  std::vector<BidiClass> types;
  types.reserve(count);
  std::vector<int> sequence;

  const auto processSequence = [&]() {
    indices.clear();
    for (const int runIndex: sequence) {
      for (size_t position = runs[static_cast<size_t>(runIndex)].start;
              position < runs[static_cast<size_t>(runIndex)].end; ++position) {
        indices.push_back(active[position]);
      }
    }
    if (indices.empty()) return;
    const uint8_t level = explicitLevels[indices.front()];
    const size_t firstActive = runs[static_cast<size_t>(sequence.front())].start;
    const size_t lastActive = runs[static_cast<size_t>(sequence.back())].end - 1;
    const uint8_t previousLevel = firstActive > 0 ? explicitLevels[active[firstActive - 1]] : result.baseLevel;
    const bool unmatchedTrailingInitiator =
            isIsolateInitiator(items[indices.back()].original) &&
            (matchingPdi.empty() || matchingPdi[indices.back()] < 0);
    const uint8_t nextLevel = (!unmatchedTrailingInitiator && lastActive + 1 < active.size())
                                      ? explicitLevels[active[lastActive + 1]]
                                      : result.baseLevel;
    const BidiClass sos = directionForLevel(std::max(level, previousLevel));
    const BidiClass eos = directionForLevel(std::max(level, nextLevel));

    types.clear();
    types.reserve(indices.size());
    for (const size_t index: indices) types.push_back(items[index].type);

    applyWeakRules(types, sos);
    applyBracketRule(types, codePoints, indices, sos, level);
    applyNeutralRules(types, sos, eos, level);
    applyImplicitLevels(items, indices, types);
  };

  if (!hasIsolates) {
    // Without an isolate every level run is its own isolating run sequence.
    for (size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
      sequence.clear();
      sequence.push_back(static_cast<int>(runIndex));
      processSequence();
    }
  } else {
    for (size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
      if (used[runIndex]) continue;
      sequence.clear();
      int current = static_cast<int>(runIndex);
      while (current >= 0 && !used[static_cast<size_t>(current)]) {
        used[static_cast<size_t>(current)] = 1;
        sequence.push_back(current);
        const Run& run = runs[static_cast<size_t>(current)];
        const size_t lastOriginal = active[run.end - 1];
        if (!isIsolateInitiator(items[lastOriginal].original) || matchingPdi[lastOriginal] < 0) break;
        const int pdiPosition = activePosition[static_cast<size_t>(matchingPdi[lastOriginal])];
        if (pdiPosition < 0) break;
        const int nextRun = runOfActive[static_cast<size_t>(pdiPosition)];
        if (nextRun < 0 || runs[static_cast<size_t>(nextRun)].start != static_cast<size_t>(pdiPosition)) {
          break;
        }
        current = nextRun;
      }
      processSequence();
    }
  }

  applyL1(items, result.baseLevel);

  for (size_t index = 0; index < count; ++index) {
    result.levels[index] = items[index].removed ? kBidiRemovedLevel : items[index].level;
  }
  result.visualOrder = reorderL2(items, active);
  return result;
}

} // namespace coral::ui::unicode
