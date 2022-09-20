// Author: Anton Vassilev
// Email: avvassil@uwaterloo.ca
// This file generates simple "spec" implementations for a number of functions
// in the c standard library

#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <vector>
#define numOps 10

#define sizeof_array(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

const std::string mkread(const std::string &var) {
  return std::string("sea_dsa_set_read(" + var + ");\n");
}

const std::string mkwrite(const std::string &var) {
  return std::string("sea_dsa_set_modified(" + var + ");\n");
}

const std::string mkheap(const std::string &var) {
  return std::string("sea_dsa_set_heap(" + var + ");\n");
}

std::string collapse(const std::string &var) {
  return std::string("sea_dsa_collapse(" + var + ");\n");
}

std::string alias(const std::vector<std::string> &vect) {
  std::string callsite;
  callsite = "sea_dsa_alias(";

  for (unsigned i = 0; i < vect.size() - 1; i++) {
    callsite.append(vect[i]);
    callsite.append(", ");
  }
  callsite.append(vect[vect.size() - 1]);
  callsite.append(");\n");

  return callsite;
}

std::string new_mem(const std::string &var) {
  return std::string(var + " = sea_dsa_new();\n");
}

struct libAction {
  // The return value/arguments that should be marked read.
  bool read[numOps];

  // The return value/arguments that should be marked modified.
  bool write[numOps];

  // The return value/arguments that should be marked as heap.
  bool heap[numOps];

  // Flags whether the return value should be merged with all arguments.
  bool mergeNodes[numOps];

  // Flags whether the return value and arguments should be folded.
  bool collapse;
};

#define NRET_NARGS                                                             \
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define YRET_NARGS                                                             \
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define NRET_YARGS                                                             \
  { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
#define YRET_YARGS                                                             \
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
#define NRET_NYARGS                                                            \
  { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 }
#define YRET_NYARGS                                                            \
  { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 }
#define NRET_YNARGS                                                            \
  { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 }
#define YRET_YNARGS                                                            \
  { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 }
#define YRET_NNYARGS                                                           \
  { 1, 0, 0, 1, 1, 1, 1, 1, 1, 1 }
#define YRET_YNYARGS                                                           \
  { 1, 1, 0, 1, 1, 1, 1, 1, 1, 1 }
#define NRET_NNYARGS                                                           \
  { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }
#define YRET_NNYNARGS                                                          \
  { 1, 0, 0, 1, 0, 0, 0, 0, 0, 0 }
#define NRET_NNNYARGS                                                          \
  { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1 }

// Table taken from:
// https://github.com/seahorn/llvm-dsa/blob/master/lib/DSA/StdLibPass.cpp
const struct {
  const std::string name;
  const libAction action;
} recFuncs[] = {
    {"stat", {NRET_YNARGS, NRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fstat", {NRET_YNARGS, NRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"lstat", {NRET_YNARGS, NRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},

    {"getenv", {NRET_YNARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"getrusage", {NRET_YNARGS, YRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"getrlimit", {NRET_YNARGS, YRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"setrlimit", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"getcwd", {NRET_NYARGS, YRET_YNARGS, NRET_NARGS, YRET_YNARGS, false}},

    {"select", {NRET_YARGS, YRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},

    {"remove", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"rename", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"unlink", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fileno", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"write", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"read", {NRET_YARGS, YRET_YARGS, NRET_NARGS, NRET_NARGS, false}},
    {"truncate", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"open", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"chdir", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"mkdir", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"rmdir", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"chmod", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fchmod", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"pipe", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"execl", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"execlp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"execle", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"time", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"times", {NRET_YARGS, YRET_YARGS, NRET_NARGS, NRET_NARGS, false}},
    {"ctime", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"asctime", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"utime", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"localtime", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"gmtime", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"ftime", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, false}},

    // printf not strictly true, %n could cause a write
    {"printf", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fprintf", {NRET_YARGS, NRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},
    {"sprintf", {NRET_YARGS, NRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},
    {"snprintf", {NRET_YARGS, NRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},
    {"vsnprintf", {NRET_YARGS, YRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},
    {"sscanf", {NRET_YARGS, YRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"scanf", {NRET_YARGS, YRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fscanf", {NRET_YARGS, YRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},

    {"calloc", {NRET_NARGS, YRET_NARGS, YRET_NARGS, NRET_NARGS, false}},
    {"malloc", {NRET_NARGS, YRET_NARGS, YRET_NARGS, NRET_NARGS, false}},
    {"valloc", {NRET_NARGS, YRET_NARGS, YRET_NARGS, NRET_NARGS, false}},
    {"realloc", {NRET_NARGS, YRET_NARGS, YRET_YNARGS, YRET_YNARGS, false}},

    {"strdup", {NRET_YARGS, YRET_NARGS, YRET_NARGS, YRET_YARGS, false}},
    {"wcsdup", {NRET_YARGS, YRET_NARGS, YRET_NARGS, YRET_YARGS, false}},

    {"strlen", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"wcslen", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"atoi", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"atof", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"atol", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"atoll", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"memcmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"strcmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"wcscmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"strncmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"wcsncmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"strcasecmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"wcscasecmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"strncasecmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"wcsncasecmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"strcat", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"strncat", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},

    {"strcpy", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"wcscpy", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"strncpy", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"wcsncpy", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"memcpy", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"memccpy", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},
    {"memmove", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YARGS, true}},

    {"bcopy", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_YARGS, true}},
    {"bcmp", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, true}},

    {"strerror", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, true}},
    {"clearerr", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, false}},
    {"strstr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"wcsstr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"strspn", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, true}},
    {"wcsspn", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, true}},
    {"strcspn", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, true}},
    {"wcscspn", {NRET_YARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, true}},
    {"strtok", {NRET_YARGS, YRET_YARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"strpbrk", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"wcspbrk", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},

    {"strchr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"wcschr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"strrchr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"wcsrchr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},

    {"memchr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    {"wmemchr", {NRET_YARGS, YRET_NARGS, NRET_NARGS, YRET_YNARGS, true}},
    //{"posix_memalign",  {NRET_YARGS, YRET_YNARGS, NRET_NARGS,  NRET_NARGS,
    // false}},

    {"perror", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"fflush", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fclose", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fopen", {NRET_YARGS, YRET_NARGS, YRET_NARGS, NRET_NARGS, false}},
    {"ftell", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fseek", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, true}},
    {"rewind", {NRET_YARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, true}},
    {"fwrite", {NRET_YARGS, NRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fread", {NRET_NYARGS, NRET_YARGS, NRET_NARGS, NRET_NARGS, false}},

    {"__errno_location",
     {NRET_NARGS, YRET_NARGS, NRET_NARGS, NRET_NARGS, false}},

    {"puts", {NRET_YARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"gets", {NRET_NARGS, YRET_YARGS, NRET_NARGS, YRET_YNARGS, false}},
    {"fgets", {NRET_NYARGS, YRET_YNARGS, NRET_NARGS, YRET_YNARGS, false}},
    {"getc", {NRET_YNARGS, YRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},
    {"ungetc", {NRET_YNARGS, YRET_YARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fgetc", {NRET_YNARGS, YRET_YNARGS, NRET_NARGS, NRET_NARGS, false}},
    {"putc", {NRET_NARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"putchar", {NRET_NARGS, NRET_NARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fputs", {NRET_YARGS, NRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
    {"fputc", {NRET_YARGS, NRET_NYARGS, NRET_NARGS, NRET_NARGS, false}},
};

const struct {
  const std::string retType;
  const std::string name;
  const std::vector<std::string> params;
} decls[] = {
    {"int", "stat", {"const char *restrict path", "struct stat *restrict buf"}},
    {"int", "fstat", {"int fd", "struct stat *st"}},
    {"int",
     "lstat",
     {"const char *restrict path", "struct stat *restrict buf"}},
    {"char *", "getenv", {"const char *name"}},
    {"int", "getrusage", {"int who", "struct rusage *ru"}},
    {"int", "getrlimit", {"int resource", "struct rlimit *rlim"}},
    {"int", "setrlimit", {"int resource", "const struct rlimit *rlim"}},
    {"char *", "getcwd", {"char *buf", "size_t size"}},
    {"int",
     "select",
     {"int n", "fd_set *restrict rfds", "fd_set *restrict wfds",
      "fd_set *restrict efds", "struct timeval *restrict tv"}},
    {"int", "remove", {"const char *path"}},
    {"int", "rename", {"const char *old", "const char *new"}},
    {"int", "unlink", {"const char *path"}},
    {"int", "fileno", {"FILE *f"}},
    {"ssize_t", "write", {"int fd", "const void *buf", "size_t count"}},
    {"ssize_t", "read", {"int fd", "void *buf", "size_t count"}},
    {"int", "truncate", {"const char *path", "off_t length"}},
    {"int", "open", {"const char *filename", "int flags", "..."}},
    {"int", "chdir", {"const char *path"}},
    {"int", "mkdir", {"const char *path", "mode_t mode"}},
    {"int", "rmdir", {"const char *path"}},
    {"int", "chmod", {"const char *path", "mode_t mode"}},
    {"int", "fchmod", {"int fd", "mode_t mode"}},
    {"int", "pipe", {"int fd[2]"}},
    {"int", "execl", {"const char *path", "const char *argv0", "..."}},
    {"int", "execlp", {"const char *file", "const char *argv0", "..."}},
    {"int", "execle", {"const char *path", "const char *argv0", "..."}},
    {"time_t", "time", {"time_t *t"}},
    {"clock_t", "times", {"struct tms *tms"}},
    {"char *", "ctime", {"const time_t *t"}},
    {"char *", "asctime", {"const struct tm *tm"}},
    {"int", "utime", {"const char *path", "const struct utimbuf *times"}},
    {"struct tm *", "localtime", {"const time_t *t"}},
    {"struct tm *", "gmtime", {"const time_t *t"}},
    {"int", "ftime", {"struct timeb *tp"}},
    {"int", "printf", {"const char *restrict fmt", "..."}},
    {"int", "fprintf", {"FILE *restrict f", "const char *restrict fmt", "..."}},
    {"int", "sprintf", {"char *restrict s", "const char *restrict fmt", "..."}},
    {"int",
     "snprintf",
     {"char *restrict s", "size_t n, const char *restrict fmt", "..."}},
    {"int",
     "vsnprintf",
     {"char *restrict s", "size_t n", "const char *restrict fmt",
      "va_list ap"}},
    {"int",
     "sscanf",
     {"const char *restrict s", "const char *restrict fmt", "..."}},
    {"int", "scanf", {"const char *restrict fmt", "..."}},
    {"int", "fscanf", {"FILE *restrict f", "const char *restrict fmt", "..."}},
    {"void *", "calloc", {"size_t m", "size_t n"}},
    {"void *", "malloc", {"size_t n"}},
    {"void *", "valloc", {"size_t size"}},
    {"void *", "realloc", {"void *p", "size_t n"}},
    {"char *", "strdup", {"const char *s"}},
    {"wchar_t *", "wcsdup", {"const wchar_t *s"}},
    {"size_t", "strlen", {"const char *s"}},
    {"size_t", "wcslen", {"const wchar_t *s"}},
    {"int", "atoi", {"const char *s"}},
    {"double", "atof", {"const char *s"}},
    {"long", "atol", {"const char *s"}},
    {"long long", "atoll", {"const char *s"}},
    {"int", "memcmp", {"const void *vl", "const void *vr", "size_t n"}},
    {"int", "strcmp", {"const char *l", "const char *r"}},
    {"int", "wcscmp", {"const wchar_t *l", "const wchar_t *r"}},
    {"int", "strncmp", {"const char *_l", "const char *_r", "size_t n"}},
    {"int", "wcsncmp", {"const wchar_t *l", "const wchar_t *r", "size_t n"}},
    {"int", "strcasecmp", {"const char *_l", "const char *_r"}},
    {"int", "wcscasecmp", {"const wchar_t *l", "const wchar_t *r"}},
    {"int", "strncasecmp", {"const char *_l", "const char *_r", "size_t n"}},
    {"int",
     "wcsncasecmp",
     {"const wchar_t *l", "const wchar_t *r", "size_t n"}},
    {"char *", "strcat", {"char *restrict dest", "const char *restrict src"}},
    {"char *",
     "strncat",
     {"char *restrict d", "const char *restrict s", "size_t n"}},
    {"char *", "strcpy", {"char *restrict dest", "const char *restrict src"}},
    {"wchar_t *",
     "wcscpy",
     {"wchar_t *restrict d", "const wchar_t *restrict s"}},
    {"char *",
     "strncpy",
     {"char *restrict d", "const char *restrict s", "size_t n"}},
    {"wchar_t *",
     "wcsncpy",
     {"wchar_t *restrict d", "const wchar_t *restrict s", "size_t n"}},
    {"void *",
     "memcpy",
     {"void *restrict dest", "const void *restrict src", "size_t n"}},
    {"void *",
     "memccpy",
     {"void *restrict dest", "const void *restrict src", "int c", "size_t n"}},
    {"void *", "memmove", {"void *dest", "const void *src", "size_t n"}},
    {"void", "bcopy", {"const void *s1", "void *s2", "size_t n"}},
    {"int", "bcmp", {"const void *s1", "const void *s2", "size_t n"}},
    {"char *", "strerror", {"int e"}},
    {"void", "clearerr", {"FILE *f"}},
    {"char *", "strstr", {"const char *h", "const char *n"}},
    {"wchar_t *",
     "wcsstr",
     {"const wchar_t *restrict h", "const wchar_t *restrict n"}},
    {"size_t", "strspn", {"const char *s", "const char *c"}},
    {"size_t", "wcsspn", {"const wchar_t *s", "const wchar_t *c"}},
    {"size_t", "strcspn", {"const char *s", "const char *c"}},
    {"size_t", "wcscspn", {"const wchar_t *s", "const wchar_t *c"}},
    {"char *", "strtok", {"char *restrict s", "const char *restrict sep"}},
    {"char *", "strpbrk", {"const char *s", "const char *b"}},
    {"wchar_t *", "wcspbrk", {"const wchar_t *s", "const wchar_t *b"}},
    {"char *", "strchr", {"const char *s", "int c"}},
    {"wchar_t *", "wcschr", {"const wchar_t *s", "wchar_t c"}},
    {"char *", "strrchr", {"const char *s", "int c"}},
    {"wchar_t *", "wcsrchr", {"const wchar_t *s", "wchar_t c"}},
    {"void *", "memchr", {"const void *src", "int c", "size_t n"}},
    {"wchar_t *", "wmemchr", {"const wchar_t *s", "wchar_t c", "size_t n"}},
    {"void", "perror", {"const char *msg"}},
    {"int", "fflush", {"FILE *f"}},
    {"int", "fclose", {"FILE *f"}},
    {"FILE *",
     "fopen",
     {"const char *restrict filename", "const char *restrict mode"}},
    {"long", "ftell", {"FILE *f"}},
    {"int", "fseek", {"FILE *f", "long off", "int whence"}},
    {"void", "rewind", {"FILE *f"}},
    {"size_t",
     "fwrite",
     {"const void *restrict src", "size_t size", "size_t nmemb",
      "FILE *restrict f"}},
    {"size_t",
     "fread",
     {"void *restrict destv", "size_t size", "size_t nmemb",
      "FILE *restrict f"}},
    {"int *", "__errno_location", {"void"}},
    {"int", "puts", {"const char *s"}},
    {"char *", "gets", {"char *s"}},
    {"char *", "fgets", {"char *restrict s", "int n", "FILE *restrict f"}},
    {"int", "getc", {"FILE *f"}},
    {"int", "ungetc", {"int c", "FILE *f"}},
    {"int", "fgetc", {"FILE *f"}},
    {"int", "putc", {"int c", "FILE *f"}},
    {"int", "putchar", {"int c"}},
    {"int", "fputs", {"const char *restrict s", "FILE *restrict f"}},
    {"int", "fputc", {"int c", "FILE *f"}}};

std::string getVarName(const std::string &str) {

  auto pos = str.find_last_of(' ');
  pos++;
  if (str.at(pos) == '*') pos++;

  return str.substr(pos, str.length() - pos);
}

bool isPtrType(const std::string &str) {
  return (str.find('*') != std::string::npos);
}

void processFunc(int index, std::ofstream &out_file) {
  int actionIndex = 1;

  auto decl = decls[index];

  auto recFunc = recFuncs[index];

  std::vector<std::string> toMerge;

  if (decl.name == "vsnprintf")

    for (auto param : decl.params) {
      if (isPtrType(param)) {
        if (recFunc.action.heap[actionIndex]) {
          out_file << mkheap(getVarName(param));
        }
        if (recFunc.action.read[actionIndex]) {
          out_file << mkread(getVarName(param));
        }
        if (recFunc.action.write[actionIndex]) {
          out_file << mkwrite(getVarName(param));
        }
        if (recFunc.action.mergeNodes[actionIndex]) {
          toMerge.push_back(getVarName(param));
        }

        // only increment action index when we have a pointer to memory
        actionIndex++;
      }
    }

  // handle return value
  if (isPtrType(decl.retType)) out_file << decl.retType << " retVal = NULL;\n";
  else if (decl.retType != "void")
    out_file << decl.retType << " retVal = 0;\n";

  // Merge all marked pointers
  if (isPtrType(decl.retType) && recFunc.action.mergeNodes[0])
    toMerge.push_back(std::string("retVal"));

  if (isPtrType(decl.retType) && !recFunc.action.mergeNodes[0]) {
    out_file << new_mem("retVal");
    if (recFunc.action.heap[0]) { out_file << mkheap(getVarName("retVal")); }
    if (recFunc.action.read[0]) { out_file << mkread(getVarName("retVal")); }
    if (recFunc.action.write[0]) { out_file << mkwrite(getVarName("retVal")); }
  }

  if (toMerge.size()) out_file << alias(toMerge);

  // collapse all marked pointers
  if (recFunc.action.collapse) {
    if (isPtrType(decl.retType)) out_file << collapse("retVal");

    for (auto param : decl.params) {
      if (isPtrType(param)) out_file << collapse(getVarName(param));
    }
  }

  // return something (if applicable)
  if (decl.retType != "void") out_file << "return retVal;\n";
}

int main() {

  std::string name("libc.spec.c");

  std::ofstream out_file;
  out_file.open(name);

  if (out_file.is_open()) {
    out_file << "#include \"seadsa/sea_dsa.h\"\n";
    out_file << "#include <stdlib.h>\n";
    out_file << "#include <stddef.h>\n";
    out_file << "#include <stdio.h>\n";
    out_file << "#include <stdint.h>\n";
    out_file << "#include <utime.h>\n";
    out_file << "#include <time.h>\n";
    out_file << "#include <sys/stat.h>\n";
    out_file << "#include <sys/resource.h>\n";
    out_file << "#include <sys/times.h>\n";
    out_file << "#include <sys/timeb.h>\n";
    out_file << "#include <sys/select.h>\n\n";
    out_file << "#undef sprintf\n";
    out_file << "#undef snprintf\n";
    out_file << "#undef vsnprintf\n";
  } else {
    throw std::invalid_argument("Couldn't open file");
  }

  for (unsigned x = 0; x < sizeof_array(recFuncs); x++) {

    out_file << decls[x].retType;
    out_file << " ";
    out_file << decls[x].name;
    out_file << "(";

    for (unsigned i = 0; i < decls[x].params.size() - 1; i++) {
      out_file << decls[x].params[i];
      out_file << ", ";
    }
    out_file << decls[x].params[decls[x].params.size() - 1];
    out_file << ") {\n";

    // definition created here
    processFunc(x, out_file);

    out_file << "\n}";
  }

  out_file.close();
  return 0;
}
