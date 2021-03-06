/*
 * Cloud9 Parallel Symbolic Execution Engine
 *
 * Copyright (c) 2011, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in CLOUD9-AUTHORS file.
 *
 */

#include "misc.h"
//#include "models.h"
//#include "signals.h"
#include "multiprocess.h"
#include "files.h"

#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#include <klee/klee.h>

char useSymbolicgettimeofday;

////////////////////////////////////////////////////////////////////////////////
// Sleeping Operations
////////////////////////////////////////////////////////////////////////////////

void _yield_sleep(unsigned sec, unsigned usec) {
  uint64_t amount = ((uint64_t)sec)*1000000 + (uint64_t)usec;

  uint64_t tstart = klee_get_time();
  __thread_preempt(1);
  uint64_t tend = klee_get_time();

  if (tend - tstart < amount)
    klee_set_time(tstart + amount);
}

int usleep(useconds_t usec) {
  //klee_warning("yielding instead of usleep()-ing");
  _yield_sleep(0, usec);
  return 0;
}

unsigned int sleep(unsigned int seconds) {
  //klee_warning("yielding instead of sleep()-ing");
  _yield_sleep(seconds, 0);
  return 0;
}

#ifdef HAVE_GETTIMEOFDAY_TZ
int gettimeofday(struct timeval *__restrict tv, struct timezone *__restrict tz) {
#elif HAVE_GETTIMEOFDAY_TZ_VOID
int gettimeofday(struct timeval *__restrict tv, void *__restrict tz) {
#else
#error "Cannot find a proper prototype for gettimeofday"
#endif
  static unsigned int call_cnt = 0;
  if (useSymbolicgettimeofday) {
    pthread_t tid = pthread_self();
    char symname[64];
    if (tv) {
      snprintf(symname, sizeof(symname), "gettimeofday_timeval_T%02lu_N%02u",
               tid, call_cnt);
      klee_make_symbolic(tv, sizeof(struct timeval), symname);
    }
    if (tz) {
      snprintf(symname, sizeof(symname), "gettimeofday_timezone_T%02lu_N%02u",
               tid, call_cnt);
      klee_make_symbolic(tv, sizeof(struct timezone), symname);
    }
    ++call_cnt;
  } else {
    if (tv) {
      // FIXME: here should be
      // uint64_t ktime = klee_get_time();
      // Or even call the underlying syscall
      // However, I haven't built the infrastructure to record and replay
      // posix/syscall arguments. Without that, I cannot craft an oracle ktest
      // nor make use of the er-generated posix/syscall arguments.
      // This is not a fundamental limitation of ER, but hundreds of lines of
      // engineering effort.
      // I use fake time (always zero) for the artifact evaluation, which can
      // help me replay the python-2018-1000030 failure.
      uint64_t ktime = 0;
      tv->tv_sec = ktime / 1000000;
      tv->tv_usec = ktime % 1000000;
    }

    if (tz) {
      struct timezone *_tz = (struct timezone *)tz;
      _tz->tz_dsttime = 0;
      _tz->tz_minuteswest = 0;
    }
  }

  return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
  if (tv) {
    uint64_t ktime = tv->tv_sec * 1000000 + tv->tv_usec;
    klee_set_time(ktime);
  }

  if (tz) {
    klee_warning("ignoring timezone set request");
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// mmap Operations
////////////////////////////////////////////////////////////////////////////////

mmap_block_t __mmaps[MAX_MMAPS];

void klee_init_mmap(void) {
  STATIC_LIST_INIT(__mmaps);
}

#ifdef HAVE_MODEL_MMAP
static int _mmap_prepopulate(void *addr, size_t length, int fd, off64_t offset) {
  off64_t origpos = _fd_lseek64(fd, 0, SEEK_CUR);

  if (origpos == -1)
    goto invalid;

  off64_t newpos = _fd_lseek64(fd, offset, SEEK_SET);
  if (newpos == -1)
    goto invalid;

  size_t remaining = length;
  char *dest = addr;

  while (remaining > 0) {
    ssize_t res = read(fd, dest, remaining);

    if (res > 0) {
      dest += res;
      remaining -= res;
    } else if (res == 0) {
      // Could not read everything, it's OK
      break;
    } else {
      goto invalid;
    }
  }

  CALL_MODEL(lseek, fd, SEEK_SET, origpos);

  return 0;

invalid:
  if (origpos >= 0)
    CALL_MODEL(lseek, fd, SEEK_SET, origpos);

  errno = EINVAL;
  return -1;
}
#endif // HAVE_MODEL_MMAP

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
#ifdef HAVE_MODEL_MMAP
  if ((prot & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE)) {
    klee_warning("unsupported read- or write-only mapping, going on anyway");
  }
  if (INJECT_FAULT(mmap, ENOMEM)) {
    return MAP_FAILED;
  }

  // We try allocating the memory
  void *result = malloc(length);

  if (!result) {
    errno = ENOMEM;
    return MAP_FAILED;
  }

  memset(result, 0, length);

  // We pre-populate the mapping
  if (flags & MAP_ANONYMOUS) {
    if (fd >= 0) {
      free(result);

      errno = EINVAL;
      return MAP_FAILED;
    }
  } else {
    int res = _mmap_prepopulate(result, length, fd, offset);

    if (res == -1) {
      free(result);
      return MAP_FAILED;
    }
  }

  // Now we create the mapping
  unsigned int idx;
  STATIC_LIST_ALLOC(__mmaps, idx);

  if (idx == MAX_MMAPS) {
    free(result);

    errno = ENOMEM;
    return MAP_FAILED;
  }

  __mmaps[idx].addr = result;
  __mmaps[idx].length = length;
  __mmaps[idx].prot = prot;
  __mmaps[idx].flags = flags;

  if (flags & MAP_SHARED) {
    klee_make_shared(result, length);
  }

  return result;
#else
  klee_warning("ignoring mmap (EPERM)");
  errno = EPERM;
  return (void*) -1;
#endif // HAVE_MODEL_MMAP
}

void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset) {
  return mmap64(start, length, prot, flags, fd, offset);
}

void *mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset) {
  return mmap(addr, length, prot, flags, fd, pgoffset * getpagesize());
}

int munmap(void *addr, size_t length) {
#ifdef HAVE_MODEL_MMAP
  unsigned idx;
  for (idx = 0; idx < MAX_MMAPS; idx++) {
    if (!STATIC_LIST_CHECK(__mmaps, idx))
      continue;

    if (__mmaps[idx].addr == addr)
      break;
  }

  if (idx == MAX_MMAPS) {
    klee_warning("inexistent mapping or unsupported fragment unmapping");
    errno = EINVAL;
    return -1;
  }

  assert(__mmaps[idx].addr);

  if (__mmaps[idx].length != length) {
    klee_warning("unsupported fragment unmapping");
    errno = EINVAL;
    return -1;
  }

  free(__mmaps[idx].addr);

  STATIC_LIST_CLEAR(__mmaps, idx);

  return 0;
#else
  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
#endif // HAVE_MODEL_MMAP
}

////////////////////////////////////////////////////////////////////////////////
// Low-level Memory Manipulation Routines
////////////////////////////////////////////////////////////////////////////////

void *__rawmemchr(const void *s, int c) {
  const char *current_char = (const char*)s;
  while (*current_char != (char)c) {
    current_char++;
  }

  return (void*)current_char;
}

////////////////////////////////////////////////////////////////////////////////
// Misc. API
////////////////////////////////////////////////////////////////////////////////

int sched_yield(void) {
  __thread_preempt(1);
  return 0;
}

int getrusage(int who, struct rusage *usage) {
  if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD) {
    errno = EINVAL;
    return -1;
  }

  memset(usage, 0, sizeof(*usage)); // XXX Refine this as further needed

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_SYMBOLIC_CTYPE
static const int32_t *tolower_locale = NULL;

DEFINE_MODEL(const int32_t **, __ctype_tolower_loc, void) {
  if (tolower_locale != NULL)
    return &tolower_locale;

  int32_t *cached = (int32_t*)malloc(384*sizeof(int32_t));
  klee_make_shared(cached, 384*sizeof(int32_t));

  const int32_t **locale = CALL_UNDERLYING(__ctype_tolower_loc);

  memcpy(cached, &((*locale)[-128]), 384*sizeof(int32_t));

  tolower_locale = &cached[128];

  return &tolower_locale;
}

static const unsigned short *b_locale = NULL;

DEFINE_MODEL(const unsigned short **, __ctype_b_loc, void) {
  if (b_locale != NULL)
    return &b_locale;

  unsigned short *cached = (unsigned short*)malloc(384*sizeof(unsigned short));
  klee_make_shared(cached, 384*sizeof(unsigned short));

  const unsigned short **locale = CALL_UNDERLYING(__ctype_b_loc);

  memcpy(cached, &((*locale)[-128]), 384*sizeof(unsigned short));

  b_locale = &cached[128];

  return &b_locale;
}
#endif

int pthread_sigmask(int how, void *set, void *oldset) {
  klee_warning("ignore pthread_sigmask");
  return 0;
}
