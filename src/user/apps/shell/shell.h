#pragma once

#define CMDLINE_MAX 64
#define HISTORY_MAX 64

int split_args(char *line, char **argv, int argv_max);
int parse_int(const char *s, int *out);
void redraw_cmdline(const char *line);
int str_len(const char *s);
void history_push(const char *line);
void history_print(void);
int history_size(void);
int history_get(int pos, char *out, int out_max);
