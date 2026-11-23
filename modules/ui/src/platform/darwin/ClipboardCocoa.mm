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

/**
 * @file ClipboardCocoa.mm
 *
 * The macOS clipboard: `NSPasteboard` with the MIME types the framework's clipboard speaks.
 *
 * `NSPasteboard` is a single-owner pasteboard, so a write replaces the previous content, matching the
 * Win32 backend. The framework's clipboard API is MIME-first, which keeps the caller unaware that
 * AppKit names its types (`public.utf8-plain-text`, `public.html`, `public.png`, `public.file-url`).
 */

#include "../../ClipboardPlatform.h"

#include <ui/Clipboard.h>

#import <Cocoa/Cocoa.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui::detail {

namespace {

/** Resolves an AppKit UTF-8 pasteboard string into the framework's byte-oriented string. */
std::string fromNSString(NSString* text) {
  if (text == nil) return {};
  const char* utf8 = text.UTF8String;
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

NSString* toNSString(const std::string_view text) {
  return [[NSString alloc] initWithBytes:text.data() length:text.size()
                                encoding:NSUTF8StringEncoding];
}

/** Splits a `text/uri-list` payload into one path/URL per line, ignoring comments. */
std::vector<std::string> uriListLines(const std::string& list) {
  std::vector<std::string> lines;
  size_t offset = 0;
  while (offset < list.size()) {
    size_t end = list.find('\n', offset);
    if (end == std::string::npos) end = list.size();
    std::string line = list.substr(offset, end - offset);
    offset = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line.front() == '#') continue;
    lines.push_back(std::move(line));
  }
  return lines;
}

class CocoaClipboard final : public Clipboard {
 public:
  [[nodiscard]] std::string get(const std::string& mime) const override {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    if (mime == "text/plain") {
      return fromNSString([pasteboard stringForType:NSPasteboardTypeString]);
    }
    if (mime == "text/html") {
      return fromNSString([pasteboard stringForType:NSPasteboardTypeHTML]);
    }
    if (mime == "text/uri-list") {
      return readFiles(pasteboard);
    }
    if (mime == "image/png") {
      NSData* data = [pasteboard dataForType:NSPasteboardTypePNG];
      if (data == nil) return {};
      return std::string(static_cast<const char*>(data.bytes), data.length);
    }
    return {};
  }

  bool set(const std::string& mime, std::string data) override {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    if (mime == "text/plain") {
      [pasteboard clearContents];
      return [pasteboard setString:toNSString(data) forType:NSPasteboardTypeString];
    }
    if (mime == "text/html") {
      [pasteboard clearContents];
      return [pasteboard setString:toNSString(data) forType:NSPasteboardTypeHTML];
    }
    if (mime == "text/uri-list") {
      return writeFiles(pasteboard, data);
    }
    if (mime == "image/png") {
      [pasteboard clearContents];
      NSData* bytes = [NSData dataWithBytes:data.data() length:data.size()];
      return [pasteboard setData:bytes forType:NSPasteboardTypePNG];
    }
    return false;
  }

  [[nodiscard]] std::vector<std::string> formats() const override {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    std::vector<std::string> result;
    if ([pasteboard availableTypeFromArray:@[ NSPasteboardTypeString ]] != nil) {
      result.emplace_back("text/plain");
    }
    if ([pasteboard availableTypeFromArray:@[ NSPasteboardTypeHTML ]] != nil) {
      result.emplace_back("text/html");
    }
    if ([pasteboard availableTypeFromArray:@[ NSPasteboardTypeFileURL ]] != nil) {
      result.emplace_back("text/uri-list");
    }
    if ([pasteboard availableTypeFromArray:@[ NSPasteboardTypePNG ]] != nil) {
      result.emplace_back("image/png");
    }
    return result;
  }

 private:
  static std::string readFiles(NSPasteboard* pasteboard) {
    NSArray<NSURL*>* urls = [pasteboard readObjectsForClasses:@[ [NSURL class] ]
                                                       options:@{ NSPasteboardURLReadingFileURLsOnlyKey: @YES }];
    if (urls == nil || urls.count == 0) return {};
    std::string list;
    for (NSURL* url in urls) {
      if (!list.empty()) list += "\r\n";
      const char* path = url.path.UTF8String;
      if (path != nullptr) list += path;
    }
    return list;
  }

  static bool writeFiles(NSPasteboard* pasteboard, const std::string& list) {
    NSMutableArray<NSURL*>* urls = [NSMutableArray array];
    for (const std::string& line: uriListLines(list)) {
      NSString* text = toNSString(line);
      if (text == nil) continue;
      NSURL* url = [text hasPrefix:@"file://"] ? [NSURL URLWithString:text]
                                               : [NSURL fileURLWithPath:text];
      if (url != nil) [urls addObject:url];
    }
    if (urls.count == 0) return false;
    [pasteboard clearContents];
    return [pasteboard writeObjects:urls];
  }
};

} // namespace

std::shared_ptr<Clipboard> createPlatformClipboard() {
  return std::make_shared<CocoaClipboard>();
}

uint64_t platformClipboardChangeCount() {
  return static_cast<uint64_t>([NSPasteboard generalPasteboard].changeCount);
}

} // namespace coral::ui::detail
