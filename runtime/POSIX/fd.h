//===-- fd.h ---------------------------------------------------*- C++ -*--===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_FD_H
#define KLEE_FD_H

#include "klee/Config/config.h"

#ifndef _LARGEFILE64_SOURCE
#error "_LARGEFILE64_SOURCE should be defined"
#endif

#include <dirent.h>
#include <sys/types.h>

#ifdef HAVE_SYSSTATFS_H
#include <sys/statfs.h>
#endif

#ifdef __APPLE__
#include <sys/dtrace.h>
#endif
#ifdef __FreeBSD__
#include "FreeBSD.h"
#endif
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/mount.h>
#include <sys/param.h>
#if !defined(dirent64)
#define dirent64 dirent
#endif
#endif

// model a symbolic regular file
typedef struct {
  unsigned size;  /* in bytes */
  char* contents;
  struct stat64* stat;
  char* filename;
} exe_disk_file_t;

typedef enum {
  eOpen         = (1 << 0), /* eOpen is set when this fd is valid*/
  eCloseOnExec  = (1 << 1),
  eReadable     = (1 << 2),
  eWriteable    = (1 << 3),
  eIsFile       = (1 << 4),
  eIsPIPE       = (1 << 5),
  eIsSocket     = (1 << 6)
} exe_file_flag_t;

/* model a linux file descriptor (could be a regular file, pipe, socket, etc.)
 * If the `dfile` field is a nullptr, then this is a concrete file descriptor.
 * Otherwise, a symbolic file descriptor
 */
typedef struct {
  int fd;         /* actual fd if not symbolic */
  unsigned flags; /* set of exe_file_flag_t values. fields
                     are only defined when flags at least
                     has eOpen. */
  off64_t off;    /* offset */
  void *dfile;    /* type: ext_disk_file_t*, if fd is a regular file; TODO pipe
                     type; TODO socket type */
} exe_file_t;

enum sym_file_type {
  STANDALONE, /* file name and size are managed for each file,
                 user can specify the name and size of each
                 symbolic file */
  UNITED      /* file name and size are managed in a predefined
                 manner. e.g.: file names are A, B, C, each has
                 the same size */
};

typedef struct {
  unsigned n_sym_files; /* number of symbolic input files, excluding stdin */
  exe_disk_file_t *sym_stdin, *sym_stdout;
  unsigned stdout_writes; /* how many chars were written to stdout */
  exe_disk_file_t *sym_files;
  /* --- */
  /* the maximum number of failures on one path; gets decremented after each failure */
  unsigned max_failures; 

  /* Which read, write etc. call should fail */
  int *read_fail, *write_fail, *close_fail, *ftruncate_fail, *getcwd_fail;
  int *chmod_fail, *fchmod_fail;

  enum sym_file_type type;
} exe_file_system_t;

#define MAX_FDS 32

/* Note, if you change this structure be sure to update the
   initialization code if necessary. New fields should almost
   certainly be at the end. */
typedef struct {
  exe_file_t fds[MAX_FDS];
  mode_t umask; /* process umask */
  unsigned version;
  /* If set, writes execute as expected.  Otherwise, writes extending
     the file size only change the contents up to the initial
     size. The file offset is always incremented correctly. */
  int save_all_writes; 
} exe_sym_env_t;

extern exe_file_system_t __exe_fs;
extern exe_sym_env_t __exe_env;

void klee_init_fds(unsigned n_files, unsigned file_length,
                   unsigned stdin_length, int sym_file_stdin_flag,
                   int sym_stdout_flag,
                   int do_all_writes_flag, unsigned max_failures,
                   char *concretize_cfg,
                   char **sym_file_names, unsigned *sym_file_lens);
void klee_init_env(int *argcPtr, char ***argvPtr);

/* *** */

int __fd_open(const char *pathname, int flags, mode_t mode);
int __fd_openat(int basefd, const char *pathname, int flags, mode_t mode);
off64_t __fd_lseek(int fd, off64_t offset, int whence);
int __fd_stat(const char *path, struct stat64 *buf);
int __fd_lstat(const char *path, struct stat64 *buf);
int __fd_fstat(int fd, struct stat64 *buf);
int __fd_ftruncate(int fd, off64_t length);
int __fd_statfs(const char *path, struct statfs *buf);
int __fd_getdents(unsigned int fd, struct dirent64 *dirp, unsigned int count);

#endif /* KLEE_FD_H */
