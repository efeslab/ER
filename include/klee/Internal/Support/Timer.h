//===-- Timer.h -------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TIMER_H
#define KLEE_TIMER_H

#include "klee/Internal/System/Time.h"

namespace klee {
  class WallTimer {
    time::Point start;
    
  public:
    WallTimer() {
      reset();
    }

    void reset() {
      start = time::getWallTime();
    }

    /// check - Return the delta since the timer was created or reseted
    ///         then reset the timer.
    time::Span check() {
      time::Point now = time::getWallTime();
      time::Span delta(now - start);
      start = now;
      return delta;
    }
  };
}

#endif

