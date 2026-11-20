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

#include <ui/ElementSpec.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace coral::ui {

namespace {

bool isSpace(const char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string_view trim(std::string_view text) {
  while (!text.empty() && isSpace(text.front())) text.remove_prefix(1);
  while (!text.empty() && isSpace(text.back())) text.remove_suffix(1);
  return text;
}

bool isIdentChar(const char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

std::optional<Color> parseColor(std::string_view value) {
  if (value.size() != 7 || value.front() != '#') return std::nullopt;
  const auto hex = [](const char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  int channels[3]{};
  for (int index = 0; index < 3; ++index) {
    const int high = hex(value[static_cast<size_t>(1 + index * 2)]);
    const int low = hex(value[static_cast<size_t>(2 + index * 2)]);
    if (high < 0 || low < 0) return std::nullopt;
    channels[index] = high * 16 + low;
  }
  return Color::MakeRGBA8(static_cast<uint8_t>(channels[0]), static_cast<uint8_t>(channels[1]),
    static_cast<uint8_t>(channels[2]), 255);
}

std::optional<float> parseNumber(std::string_view value) {
  float result = 0.0f;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) return std::nullopt;
  if (!std::isfinite(result)) return std::nullopt;
  return result;
}

std::optional<bool> parseBoolean(std::string_view value) {
  if (value == "true") return true;
  if (value == "false") return false;
  return std::nullopt;
}

/** A quoted value keeps its spaces; an unquoted one ends at the next space. */
std::optional<std::string> readValue(std::string_view& rest) {
  if (rest.empty()) return std::nullopt;
  if (rest.front() == '"') {
    rest.remove_prefix(1);
    const size_t close = rest.find('"');
    if (close == std::string_view::npos) return std::nullopt;
    std::string value(rest.substr(0, close));
    rest.remove_prefix(close + 1);
    return value;
  }
  size_t length = 0;
  while (length < rest.size() && !isSpace(rest[length])) ++length;
  if (length == 0) return std::nullopt;
  std::string value(rest.substr(0, length));
  rest.remove_prefix(length);
  return value;
}

} // namespace

ElementPtr parseElementSpec(const std::string_view spec, const ElementRegistry& registry,
    const ElementSpecOptions& options) {
  std::string_view rest = trim(spec);
  if (rest.empty()) return nullptr;

  size_t typeLength = 0;
  while (typeLength < rest.size() && isIdentChar(rest[typeLength])) ++typeLength;
  if (typeLength == 0) return nullptr;
  const std::string type(rest.substr(0, typeLength));
  rest.remove_prefix(typeLength);

  std::string key;
  if (!rest.empty() && rest.front() == '#') {
    rest.remove_prefix(1);
    size_t keyLength = 0;
    while (keyLength < rest.size() && isIdentChar(rest[keyLength])) ++keyLength;
    if (keyLength == 0) return nullptr;
    key = std::string(rest.substr(0, keyLength));
    rest.remove_prefix(keyLength);
  }

  ElementProps props;
  std::optional<Color> background;
  std::optional<float> radius;
  std::optional<bool> checked;
  std::optional<bool> spinning;
  std::optional<float> width;
  std::optional<float> height;
  std::string text;
  bool hasProperties = false;

  while (true) {
    rest = trim(rest);
    if (rest.empty()) break;
    size_t nameLength = 0;
    while (nameLength < rest.size() && (isIdentChar(rest[nameLength]))) ++nameLength;
    if (nameLength == 0 || nameLength >= rest.size() || rest[nameLength] != '=') return nullptr;
    const std::string_view name = rest.substr(0, nameLength);
    rest.remove_prefix(nameLength + 1);
    const auto value = readValue(rest);
    if (!value.has_value()) return nullptr;

    if (name == "background") {
      const auto color = parseColor(*value);
      if (!color.has_value()) return nullptr;
      background = color;
    } else if (name == "radius") {
      const auto number = parseNumber(*value);
      if (!number.has_value() || *number < 0.0f) return nullptr;
      radius = number;
    } else if (name == "opacity") {
      const auto number = parseNumber(*value);
      if (!number.has_value()) return nullptr;
      props.opacity = *number;
    } else if (name == "width") {
      const auto number = parseNumber(*value);
      if (!number.has_value() || *number < 0.0f) return nullptr;
      width = number;
    } else if (name == "height") {
      const auto number = parseNumber(*value);
      if (!number.has_value() || *number < 0.0f) return nullptr;
      height = number;
    } else if (name == "text") {
      text = *value;
    } else if (name == "checked") {
      const auto boolean = parseBoolean(*value);
      if (!boolean.has_value()) return nullptr;
      checked = boolean;
    } else if (name == "spinning") {
      const auto boolean = parseBoolean(*value);
      if (!boolean.has_value()) return nullptr;
      spinning = boolean;
    } else {
      return nullptr;
    }
    hasProperties = true;
  }

  if (width.has_value() != height.has_value()) return nullptr;
  if (width.has_value()) props.bounds = Rect::MakeWH(*width, *height);
  if (radius.has_value()) props.cornerRadius = radius;
  if (background.has_value()) props.background = background;

  ElementPtr element;
  if (type == "Rect") {
    element = std::make_shared<ColorElement>(background.value_or(Color::MakeRGBA8(0, 0, 0, 0)), key);
    // The colour is already the element's own state; the wrapper only carries layout/opacity.
    props.background.reset();
  } else if (type == "Text") {
    element = std::make_shared<TextElement>(text, options.font, Color::White(), key);
  } else if (type == "CheckBox") {
    element = std::make_shared<CheckBoxElement>(text, options.font, checked.value_or(false),
      CheckBoxElement::ChangeHandler{}, key);
  } else if (type == "Spinner") {
    element = std::make_shared<SpinnerElement>(spinning.value_or(true),
      background.value_or(Color::MakeRGBA8(120, 170, 255, 255)), key);
    props.background.reset();
  } else {
    element = registry.build(type, props, {}, key);
    if (element == nullptr) return nullptr;
    return element;
  }

  if (!hasProperties) return element;
  return components::WithProps(std::move(element), props);
}

} // namespace coral::ui
