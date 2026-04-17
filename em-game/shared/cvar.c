/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#define _REENTRANT
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "cvar.h"
#include "types.h"
#include "llist.h"
#include "stringbuf.h"

#ifdef LINUX
#define stricmp strcasecmp 
#define strnicmp strncasecmp 
#endif

void console_print(char *text);

struct cvar_t
{
	char *name;
	int flags;

	union
	{
		int *int_addr;
		float *float_addr;
		char *string;
		void (*cmd_func)(char*);
	};

	union
	{
		void (*int_qc_func)(int);
		void (*float_qc_func)(float);
		void (*string_qc_func)(char*);
	};

	void (*int_qc_func_wv)(int*, int);
	
	struct cvar_t *next;

} *cvar0;



//
//	INTERNAL FUNCTIONS
//


struct cvar_t *get_cvar(char *name)
{
	if(!cvar0)
		return NULL;

	struct cvar_t *cvar = cvar0;

	while(stricmp(cvar->name, name) != 0)
	{
		cvar = cvar->next;

		if(!cvar)
			return NULL;
	}

	return cvar;
}


struct cvar_t *new_cvar(char *name)
{
	struct cvar_t *cvar;
		
	LL_CALLOC(struct cvar_t, &cvar0, &cvar);

	cvar->name = malloc(strlen(name) + 1);
	strcpy(cvar->name, name);

	return cvar;
}



//
//	INTERFACE FUNCTIONS
//


void create_cvar_int(char *name, int *addr, int flags)
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_INT | (flags & CVAR_FLAGS);
	cvar->int_addr = addr;
}


void create_cvar_int_qc(char *name, int *addr, void (*qc_func)(int))
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_INT;
	cvar->int_addr = addr;
	cvar->int_qc_func = qc_func;
}


void create_cvar_float(char *name, float *addr, int flags)
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_FLOAT | (flags & CVAR_FLAGS);
	cvar->float_addr = addr;
}


void create_cvar_float_qc(char *name, float *addr, void (*qc_func)(float))
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_FLOAT;
	cvar->float_addr = addr;
	cvar->float_qc_func = qc_func;
}


void create_cvar_string(char *name, char *string, int flags)
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_STRING | (flags & CVAR_FLAGS);
	cvar->string = malloc(strlen(string) + 1);
	strcpy(cvar->string, string);
}


void create_cvar_string_qc(char *name, char *string, void (*qc_func)(char*))
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_STRING;
	cvar->string = malloc(strlen(string) + 1);
	strcpy(cvar->string, string);
	cvar->string_qc_func = qc_func;
}


void create_cvar_command(char *name, void (*cmd_func)(char*))
{
	struct cvar_t *cvar = new_cvar(name);

	cvar->flags = CVAR_CMD;
	cvar->cmd_func = cmd_func;
}


void set_int_cvar_qc_function(char *name, void (*qc_func)(int))
{
	struct cvar_t *cvar = get_cvar(name);

	if(cvar)
		cvar->int_qc_func = qc_func;
}


void set_int_cvar_qc_function_wv(char *name, void (*qc_func)(int*, int))
{
	struct cvar_t *cvar = get_cvar(name);

	if(cvar)
		cvar->int_qc_func_wv = qc_func;
}


void set_float_cvar_qc_function(char *name, void (*qc_func)(float))
{
	struct cvar_t *cvar = get_cvar(name);

	if(cvar)
		cvar->float_qc_func = qc_func;
}


void set_string_cvar_qc_function(char *name, void (*qc_func)(char*))
{
	struct cvar_t *cvar = get_cvar(name);

	if(cvar)
		cvar->string_qc_func = qc_func;
}


void set_cvar_int(char *name, int val)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return;

	if(cvar->flags & CVAR_PROTECTED)
	{
		console_print("cvar is protected.\n");
		return;
	}

	if(cvar->int_qc_func)
	{
		cvar->int_qc_func(val);
		return;
	}

	if(cvar->int_qc_func_wv)
	{
		cvar->int_qc_func_wv(cvar->int_addr, val);
		return;
	}

	*(cvar->int_addr) = val;
}


void set_cvar_float(char *name, float val)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return;

	if(cvar->flags & CVAR_PROTECTED)
	{
		console_print("cvar is protected.\n");
		return;
	}

	if(cvar->float_qc_func)
	{
		cvar->float_qc_func(val);
		return;
	}

	*(cvar->float_addr) = val;
}


void set_cvar_string(char *name, char *string)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return;

	if(cvar->flags & CVAR_PROTECTED)
	{
		console_print("cvar is protected.\n");
		return;
	}

	if(cvar->string_qc_func)
		cvar->string_qc_func(string);

	free(cvar->string);
	cvar->string = malloc (strlen(string) + 1);
	strcpy(cvar->string, string);
}


void execute_cvar_command(char *name, char *param)
{
	struct cvar_t *cvar = get_cvar(name);

	if((cvar->flags & CVAR_TYPE) == CVAR_CMD && cvar->cmd_func)
	{
		void (*cmd_func)(char*) = cvar->cmd_func;
		assert(cmd_func);

		cmd_func(param);
		return;
	}
}


int is_cvar(char *name)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return 0;

	strcpy(name, cvar->name);
	return 1;
}


void complete_cvar(char *partial)
{
	struct cvar_t *ccvar = cvar0;
	int l = strlen(partial);

	while(ccvar)
	{
		if(strnicmp(ccvar->name, partial, l) == 0)
		{
			strcpy(partial, ccvar->name);
			strcat(partial, " ");

			break;
		}

		ccvar = ccvar->next;
	}
}


void list_cvars(char *partial)
{
	struct cvar_t *ccvar = cvar0;
	int l = strlen(partial);

	while(ccvar)
	{
		if(strnicmp(ccvar->name, partial, l) == 0)
		{
		//	ConsolePrint(ccvar->name);
		//	ConsolePrint("\n");
			;
		}

		ccvar = ccvar->next;
	}
}


int get_cvar_type(char *name)
{
	struct cvar_t *cvar = get_cvar(name);
	
	if(!cvar)
		return CVAR_UNDEF;

	int type = cvar->flags & CVAR_TYPE;

	return type;
}


int get_cvar_int(char *name)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return 0;

	if((cvar->flags & CVAR_TYPE) != CVAR_INT)
		return 0;

	int val = *(cvar->int_addr);

	return val;
}


float get_cvar_float(char *name)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return 0.0f;

	if((cvar->flags & CVAR_TYPE) != CVAR_FLOAT)
		return 0.0f;

	float val = *(cvar->float_addr);
	
	return val;
}


char *get_cvar_string(char *name)
{
	struct cvar_t *cvar = get_cvar(name);

	if(!cvar)
		return NULL;

	if((cvar->flags & CVAR_TYPE) != CVAR_STRING)
		return NULL;

	char *string = malloc (strlen(cvar->string) + 1);

	strcpy(string, cvar->string);
	
	return string;
}


void dump_cvars(FILE *file)
{
	struct cvar_t *cvar = cvar0;

	while(cvar)
	{
		if(!(cvar->flags & CVAR_PROTECTED))
		{
			struct string_t *s = new_string_text(cvar->name);
			string_cat_char(s, ' ');

			switch(cvar->flags & CVAR_TYPE)
			{
			case CVAR_INT:
				string_cat_int(s, *(cvar->int_addr));
				string_cat_char(s, '\n');
				fputs(s->text, file);
				break;

			case CVAR_FLOAT:
				string_cat_double(s, *(cvar->float_addr), 4);
				string_cat_char(s, '\n');
				fputs(s->text, file);
				break;

			case CVAR_STRING:
				string_cat_text(s, cvar->string);
				string_cat_char(s, '\n');
				fputs(s->text, file);
				break;
			}

			free_string(s);
		}

		cvar = cvar->next;
	}
}




void clear_cvars()
{
	struct cvar_t *cvar = cvar0;

	while(cvar)
	{
		free(cvar->name);

		if((cvar->flags & CVAR_TYPE) == CVAR_STRING)
			free(cvar->string);

		struct cvar_t *next = cvar->next;
		free(cvar);

		cvar = next;
	}

	cvar0 = NULL;
}
