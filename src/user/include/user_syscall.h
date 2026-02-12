#pragma once

void putchar(char ch);
long getchar(void);
int ps(int index);
int clone(int app_id);
int spawn(int app_id);
int waitpid(int pid);
int ipc_send(int pid, int message);
int ipc_recv(int *from_pid);
int bitmap(int index);
__attribute__((noreturn)) void exit(void);
