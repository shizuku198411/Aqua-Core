#pragma once

#include "stdtypes.h"

#define CMDLINE_MAX 64
#define HISTORY_MAX 64

#define CONSOLE_BLUE  "\033[34m"
#define CONSOLE_WHITE "\033[0m"

int split_args(char *line, char **argv, int argv_max);
int parse_int(const char *s, int *out);
int parse_redirection(char **argv, int argc, char **exec_argv, int *exec_argc, char **in_path, char **out_path);
void redraw_cmdline(const char *line, int cursor_pos, char *cwd_path);
int str_len(const char *s);
void shell_set_input_fd(int fd);
void shell_reset_input_fd(void);
int shell_read_input(void *buf, size_t size);
void shell_set_output_fd(int fd);
void shell_reset_output_fd(void);
void shell_printf(const char *fmt, ...);
void history_push(const char *line);
void history_print(void);
int history_size(void);
int history_get(int pos, char *out, int out_max);
char *proc_state_to_string(int state);
char *proc_wait_reason_to_string(int wait_reason);
void history_load(void);
void history_write(void);
bool is_background(char **argv, int argc);
int run_external(char **argv, int argc, bool background);
