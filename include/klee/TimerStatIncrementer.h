//===-- TimerStatIncrementer.h ----------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_TIMERSTATINCREMENTER_H
#define KLEE_TIMERSTATINCREMENTER_H

#include "klee/Statistics.h"
#include "klee/Internal/Support/Timer.h"

namespace klee {
  class TimerStatIncrementer {
  private:
    WallTimer timer;
    Statistic &statistic;
    bool checked;

  public:
    TimerStatIncrementer(Statistic &_statistic) : statistic(_statistic), checked(false) {}
    ~TimerStatIncrementer() {
      if (!checked) {
        check();
      }
    }

    time::Span check() {
      // record microseconds
      time::Span delta = timer.check();
      statistic += delta.toMicroseconds();
      checked = true;
      return delta;
    }
  };
}

#endif
