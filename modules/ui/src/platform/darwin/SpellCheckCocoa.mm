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

#include <ui/SpellCheck.h>

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

namespace coral::ui {

namespace {

std::size_t utf8Offset(NSString* text, NSUInteger utf16Index) {
  if (utf16Index > text.length) return static_cast<std::size_t>(-1);
  NSString* prefix = [text substringToIndex:utf16Index];
  return static_cast<std::size_t>([prefix lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
}

} // namespace

std::vector<SpellIssue> spellCheck(const std::string_view utf8, const std::string_view language) {
  std::vector<SpellIssue> issues;
  if (utf8.empty()) return issues;
  @autoreleasepool {
    NSString* text = [[NSString alloc] initWithBytes:utf8.data() length:utf8.size()
                                            encoding:NSUTF8StringEncoding];
    if (text == nil || text.length == 0) return issues;
    NSString* preferred = language.empty() ? nil
      : [NSString stringWithUTF8String:std::string(language).c_str()];
    NSSpellChecker* checker = [NSSpellChecker sharedSpellChecker];
    NSUInteger start = 0;
    while (start < text.length) {
      NSInteger wordCount = 0;
      const NSRange range = [checker checkSpellingOfString:text startingAt:start
        language:preferred wrap:NO inSpellDocumentWithTag:0 wordCount:&wordCount];
      if (range.location == NSNotFound || range.length == 0) break;
      SpellIssue issue;
      issue.begin = utf8Offset(text, range.location);
      issue.end = utf8Offset(text, NSMaxRange(range));
      if (issue.begin != static_cast<std::size_t>(-1) &&
          issue.end != static_cast<std::size_t>(-1) && issue.end > issue.begin) {
        issues.push_back(issue);
      }
      const NSUInteger next = NSMaxRange(range);
      if (next <= start) break;
      start = next;
    }
  }
  return issues;
}

bool supportsSpellChecking() noexcept {
  return true;
}

} // namespace coral::ui
