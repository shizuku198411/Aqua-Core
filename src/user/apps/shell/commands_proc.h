#pragma once

void shell_cmd_ps(void);
void shell_cmd_kill(int argc, char **argv);
void shell_cmd_ipc_start(void);
void shell_cmd_ipc_send(const char *pid_s, const char *msg_s);
