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
#include <cassert>

namespace klee {

  /**
   * A TimerStatIncrementer adds its lifetime to a specified Statistic.
   */
  class TimerStatIncrementer {
  protected:
    WallTimer timer;
    Statistic &statistic;
    bool checked;

  public:
    explicit TimerStatIncrementer(Statistic &_statistic) : statistic(_statistic), checked(false) {}
    ~TimerStatIncrementer() {
      if (!checked) {
        check();
      }
    }

    time::Span check() {
      assert(!checked && "TimerStatIncrementer should not be checked twice");
      // record microseconds
      time::Span t = delta();
      statistic += t.toMicroseconds();
      checked = true;
      return t;
    }

    void reset() {
      timer.reset();
      checked = false;
    }

    time::Span delta() const { return timer.delta(); }
  };

  class TimerStatIncrementerWithMax: public TimerStatIncrementer {
    protected:
      Statistic &max_stat;
      bool maxUpdated = false;
    public:
      explicit TimerStatIncrementerWithMax(Statistic &_inc_stat, Statistic &_max_stat): TimerStatIncrementer(_inc_stat), max_stat(_max_stat) {}

      time::Span check() {
        time::Span t = TimerStatIncrementer::check();
        uint64_t t_us = t.toMicroseconds();
        if (t_us > max_stat) {
          max_stat.setValue(t_us);
          maxUpdated = true;
        }
        return t;
      }

      bool isMaxUpdated() const { return maxUpdated; }

      ~TimerStatIncrementerWithMax() {
        if (!checked) {
          check();
        }
      }
  };
}

#endif /* KLEE_TIMERSTATINCREMENTER_H */
