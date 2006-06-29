#ifndef __forkexec_h__
#define __forkexec_h__

#include <unistd.h>

pid_t forkexec(char **argv, char **envp, int outpipe[2], int inpipe[2]);
pid_t forkexec_cmd(char *cmd, char **envp, int outpipe[2], int inpipe[2]);

#endif // __forkexec_h__
