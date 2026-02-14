#pragma once

void shell_cmd_history(void);
void shell_cmd_stdin_test(void);
int shell_cmd_write_file(const char *path, const char *text);
int shell_cmd_cd(const char *path);
int shell_cmd_pwd(void);
__attribute__((noreturn)) void shell_cmd_exit(void);
