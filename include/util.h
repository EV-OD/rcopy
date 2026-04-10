#ifndef RCOPY_UTIL_H
#define RCOPY_UTIL_H

#include <stddef.h>

int util_read_wl_paste(char **out, size_t *len);
int util_read_stdin(char **out, size_t *len);
int util_read_wl_paste_type(const char *mime_type, char **out, size_t *len);
int util_write_wl_copy(const char *data, size_t len);
int util_write_wl_copy_type(const char *mime_type, const char *data, size_t len);
int util_list_clipboard_types(char ***types, size_t *count);
void util_free_string_list(char **items, size_t count);
int util_run_shell(const char *command);

#endif
