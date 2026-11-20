/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_PLATFORM_LINUX_WAKEPIPE_H
#define CORAL_UI_PLATFORM_LINUX_WAKEPIPE_H

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace coral::ui::detail {

/**
 * An eventfd-like byte pipe used to wake a poll waiting on a display connection.
 *
 * A posted task or another thread can call `wake()`; the UI thread's `poll` includes `readFd()` and
 * drains it before returning from `waitForEvents`. Non-blocking writes mean a full pipe is a no-op:
 * one pending byte is already enough to wake the loop.
 */
class WakePipe {
 public:
  WakePipe() = default;
  ~WakePipe() { close(); }

  WakePipe(const WakePipe&) = delete;
  WakePipe& operator=(const WakePipe&) = delete;

  bool open() {
    if (mRead >= 0) return true;
#if defined(__linux__)
    if (pipe2(mFds, O_CLOEXEC | O_NONBLOCK) != 0) return false;
    mRead = mFds[0];
    mWrite = mFds[1];
#else
    return false;
#endif
    return true;
  }

  void close() {
    if (mRead >= 0) ::close(mRead);
    if (mWrite >= 0) ::close(mWrite);
    mRead = -1;
    mWrite = -1;
  }

  void wake() const {
    if (mWrite < 0) return;
    const uint8_t byte = 1;
    const ssize_t written = ::write(mWrite, &byte, sizeof(byte));
    (void)written; // EAGAIN means a wake byte is already queued.
  }

  void drain() const {
    if (mRead < 0) return;
    uint8_t buffer[64];
    while (::read(mRead, buffer, sizeof(buffer)) > 0) {}
  }

  [[nodiscard]] int readFd() const noexcept { return mRead; }

 private:
  int mFds[2]{ -1, -1 };
  int mRead = -1;
  int mWrite = -1;
};

} // namespace coral::ui::detail

#endif // CORAL_UI_PLATFORM_LINUX_WAKEPIPE_H
