#ifndef PREFIX_H
#define PREFIX_H

#ifdef __cplusplus
extern "C" {
#endif

char *br_strcat(const char *str1, const char *str2);
char *br_extract_dir(const char *path);
char *br_extract_prefix(const char *path);

#ifdef __cplusplus
}
#endif

#endif
