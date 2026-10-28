/* Test-only newlib support; no OS I/O. Integer snprintf uses the real library. */
#include <stddef.h>
#include <sys/stat.h>
static unsigned char heap[16384];
static ptrdiff_t used;
void *_sbrk(ptrdiff_t amount)
{
  void *previous = heap + used;
  if (used + amount < 0 || used + amount > (ptrdiff_t)sizeof(heap)) return (void *)-1;
  used += amount;
  return previous;
}
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *s) { (void)fd; s->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return -1; }
int _read(int fd, void *data, unsigned count) { (void)fd; (void)data; (void)count; return -1; }
int _write(int fd, const void *data, unsigned count) { (void)fd; (void)data; return (int)count; }
int _getpid(void) { return 1; }
int _kill(int pid, int signal) { (void)pid; (void)signal; return -1; }
void _exit(int status) { (void)status; for (;;) { } }
