/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_NFSTRING
#define _INC_NFSTRING

#include <stdint.h>
#include <zlib.h>

struct string_t
{
	char *text;
};

struct string_t *new_string();
struct string_t *new_string_string(struct string_t *cat_string);
struct string_t *new_string_text(const char *fmt, ...);
struct string_t *new_string_char(char c);
struct string_t *new_string_int(int val);
struct string_t *new_string_uint32(uint32_t val);
struct string_t *new_string_double(double val, int digits);
struct string_t *new_string_uint64(uint64_t val);

void free_string(struct string_t *string);

void string_cat_string(struct string_t *string, struct string_t *cat_string);
void string_cat_text(struct string_t *string, const char *fmt, ...);
void string_cat_char(struct string_t *string, char c);
void string_cat_int(struct string_t *string, int val);
void string_cat_uint32(struct string_t *string, uint32_t val);
void string_cat_double(struct string_t *string, double val, int digits);
void string_cat_uint64(struct string_t *string, uint64_t val);

#if defined _INC_STDIO || defined _STDIO_H
void fwrite_string(struct string_t *string, FILE *file);
struct string_t *fread_string(FILE *file);
#endif

void gzwrite_string(gzFile file, struct string_t *string);
struct string_t *gzread_string(gzFile file);

int string_isempty(struct string_t *string);
void string_clear(struct string_t *string);


#endif
