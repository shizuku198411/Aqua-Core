#pragma once

void shell_cmd_mkdir(const char *path);
void shell_cmd_rmdir(const char *path);
void shell_cmd_touch(const char *path);
void shell_cmd_rm(const char *path);
void shell_cmd_write(const char *path, const char *text);
void shell_cmd_cat(const char *path);
void shell_cmd_ls(int argc, char **argv);
