#pragma once

/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

#include "util/compiler-util.h"

namespace cache {

template<typename T>
struct Atomic {
  Atomic() = default;
  constexpr Atomic(T initial) : value(std::forward<T>(initial)) {
  }

  ALWAYS_INLINE T Load(std::memory_order order = std::memory_order_seq_cst) const {
    return value.load(order);
  }

  ALWAYS_INLINE void Store(T desired,
      std::memory_order order = std::memory_order_seq_cst) {
    value.store(desired, order);
  }

  ALWAYS_INLINE bool CompareExchangeWeak(T expected, T desired,
      std::memory_order order = std::memory_order_seq_cst) {
    T expectedOrActual = expected;
    return value.compare_exchange_weak(expectedOrActual, desired, order);
  }

  ALWAYS_INLINE bool CompareExchangeWeakRelaxed(T expected, T desired) {
    return compareExchangeWeak(expected, desired, std::memory_order_relaxed);
  }

  ALWAYS_INLINE bool CompareExchangeWeak(T expected, T desired,
      std::memory_order order_success, std::memory_order order_failure) {
    T expected_or_actual = expected;
    return value.compare_exchange_weak(
        expected_or_actual, desired, order_success, order_failure);
  }

  std::atomic<T> value;
};

// This is copied from the Webkit source tree and adapted minimally.
// https://webkit.org/blog/6161/locking-in-webkit/

// A WordLock is a fully adaptive mutex that uses sizeof(void*) storage. It has a fast
// path that is similar to a spinlock, and a slow path that is similar to std::mutex. In
// most cases, you should use Lock instead. WordLock sits lower in the stack and is used
// to implement Lock, so Lock is the main client of WordLock.

// NOTE: This is also a great lock to use if you are very low in the stack. For example,
// PrintStream uses this so that ParkingLot and Lock can use PrintStream. This means that
// if you try to use dataLog to debug this code, you will have a bad time.
class WordLock final {
 public:
  constexpr WordLock() = default;

  void lock() {
    if (LIKELY(word_.CompareExchangeWeak(0, isLockedBit, std::memory_order_acquire))) {
      // WordLock acquired!
      return;
    }
    LockSlow();
  }

  bool try_lock() {
    if (LIKELY(word_.CompareExchangeWeak(0, isLockedBit, std::memory_order_acquire))) {
      // WordLock acquired!
      return true;
    }
    return SpinLock();
  }

  void unlock() {
    if (LIKELY(word_.CompareExchangeWeak(isLockedBit, 0, std::memory_order_release))) {
      // WordLock released, and nobody was waiting!
      return;
    }
    UnlockSlow();
  }

  bool IsHeld() const {
    return word_.Load(std::memory_order_acquire) & isLockedBit;
  }

  bool IsLocked() const {
    return IsHeld();
  }

 private:
  static constexpr uint64_t isLockedBit = 1;
  static constexpr uint64_t isQueueLockedBit = 2;
  static constexpr uint64_t queueHeadMask = 3;

  bool SpinLock();
  void LockSlow();
  void UnlockSlow();

  // Method used for testing only.
  bool isFullyReset() const {
    return !word_.Load();
  }

  Atomic<uintptr_t> word_ { 0 };
};

}

