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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "types.h"
#include "stringbuf.h"
#include "cvar.h"

void console_print(char *text);


void parse_command(char *cmd)
{
	char *token = strtok(cmd, " ");

	if(!token)
		return;

	if(strcmp(token, "//") == 0)
		return;
	
	struct string_t *output = new_string();
	
	if(is_cvar(token))
	{
		char *val;

		switch(get_cvar_type(token))
		{
		case CVAR_INT:
			val = strtok(NULL, " ");

			if(val)
				set_cvar_int(token, atoi(val));
			else
			{
				string_cat_text(output, token);
				string_cat_text(output, " is ");
				string_cat_int(output, get_cvar_int(token));
				string_cat_char(output, '\n');

				console_print(output->text);
			}

			break;


		case CVAR_FLOAT:
			val = strtok(NULL, " ");

			if(val)
				set_cvar_float(token, (float)atof(val));
			else
			{
				string_cat_text(output, token);
				string_cat_text(output, " is ");
				string_cat_double(output, get_cvar_float(token), 4);
				string_cat_char(output, '\n');

				console_print(output->text);
			}

			break;


		case CVAR_STRING:
			val = strtok(NULL, " ");

			if(val)
				set_cvar_string(token, val);
			else
			{
				string_cat_text(output, token);
				string_cat_text(output, " is ");
				string_cat_text(output, get_cvar_string(token));
				string_cat_char(output, '\n');

				console_print(output->text);
			}

			break;


		case CVAR_CMD:
			val = strtok(NULL, "");
			execute_cvar_command(token, val);

			break;
		}

		free_string(output);

		return;
	}


	string_cat_text(output, "unknown command: \"");
	string_cat_text(output, token);
	string_cat_text(output, "\"\n");

	console_print(output->text);

	free_string(output);
}
