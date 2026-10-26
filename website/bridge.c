#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  int flags = fcntl(STDOUT_FILENO, F_GETFL);
  if (flags < 0 || fcntl(STDOUT_FILENO, F_SETFL, flags & ~O_NONBLOCK) < 0) {
    return 1;
  }
  if (dup2(STDOUT_FILENO, STDIN_FILENO) < 0 ||
      dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
    return 1;
  }
  const char message[] = "SHELL_READY\n";
  if (write(STDOUT_FILENO, message, sizeof(message) - 1) < 0) {
    return 1;
  }
  char *const args[] = {"sh", "-i", NULL};
  execv("/bin/sh", args);
  const char *error = strerror(errno);
  write(STDERR_FILENO, error, strlen(error));
  return 1;
}
