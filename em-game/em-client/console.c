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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "types.h"
#include "cvar.h"
#include "stringbuf.h"
#include "parse.h"
#include "user.h"
#include "gsub.h"
#include "rcon.h"
#include "render.h"
#include "entry.h"

#define CONSOLE_LL_GRAN 200
#define CONSOLE_INPUT_LENGTH 80

pthread_mutex_t console_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

int console_inputting = 0;
char *console_input = NULL;

int newline = 1;

int tabbed = 0;

char *old_console_input = NULL;
int console_pipe[2];


int r_ConsoleHeight = 8;
int r_ConsoleRed = 0x56;
int r_ConsoleGreen = 0xef;
int r_ConsoleBlue = 0xf0;
int r_ConsoleColour = 0x7f7f;
int r_ConsoleAlpha = 0x7f;
int r_RconConsoleRed = 0xf6;
int r_RconConsoleGreen = 0x2f;
int r_RconConsoleBlue = 0x20;
int r_RconConsoleColour = 0xafaf;
int r_ConsoleTextRed = 0;
int r_ConsoleTextGreen = 0x7f;
int r_ConsoleTextBlue = 0xff;
int r_ConsoleTextColour = 0xf7f3;
int r_ConsoleActiveTextRed = 0xff;
int r_ConsoleActiveTextGreen = 0xff;
int r_ConsoleActiveTextBlue = 0xff;
int r_ConsoleActiveTextColour = 0xffff;
int r_DrawConsole = 1;


struct consolell_t
{
	int numlines;
	char *line[CONSOLE_LL_GRAN];

	struct consolell_t *prev, *next;

} *console0 = NULL, *scrolling_console = NULL;

int scrolling = 0;
int firstconsoleline = 0;


struct cmdhistll_t
{
	char *cmd;

	struct cmdhistll_t *next, *prev;
} *cmdhist0 = NULL, *ccmdhist = NULL;


void render_console()
{
	pthread_mutex_lock(&console_mutex);
	
	if(!r_DrawConsole)
		goto end;

	int consoleheight = r_ConsoleHeight;

	if(console0)
	{
		int consoleline = consoleheight - 1;

		if(console_inputting)
		{
			blit_text(1, (vid_height - consoleheight * 14) + consoleline * 14 + 1, 
				r_ConsoleActiveTextRed, r_ConsoleActiveTextGreen, r_ConsoleActiveTextBlue, 
				s_backbuffer, console_input);
			
			consoleline--;
		}

		struct consolell_t *console;
		int cline;

		if(scrolling)
		{
			console = scrolling_console;

		}
		else
		{
			console = console0;
		}
		

		while(consoleline >= 0)
		{
			int stop = 0;

			for(cline = console->numlines - 1; cline >= 0; cline--)
			{
				blit_text(1, (vid_height - consoleheight * 14) + consoleline * 14 + 1, 
					r_ConsoleTextRed, r_ConsoleTextGreen, r_ConsoleTextBlue, 
					s_backbuffer, console->line[cline]);
				
				if(consoleline-- == 0)
				{
					stop = 1;
					break;
				}
			}

			if(stop)
				break;

			if(console->prev)
				console = console->prev;
			else
				break;
		}
	}
	
	
	struct blit_params_t params;
		
	params.red = r_ConsoleRed;
	params.green = r_ConsoleRed;
	params.blue = r_ConsoleRed;
	params.alpha = r_ConsoleAlpha;
	
	params.dest = s_backbuffer;
	params.dest_x = 0;
	params.dest_y = vid_height - consoleheight * 14;
	params.width = vid_width;
	params.height = consoleheight * 14;

	alpha_draw_rect(&params);
	
/*	if(rconing == RCCON_IN)
		blit_colour = r_RconConsoleColour;
	else
		blit_colour = r_ConsoleColour;

	draw_alpha_rect();
*/
	
	end:
	pthread_mutex_unlock(&console_mutex);
}


void add_cmd_hist(char *cmd)
{
	struct cmdhistll_t *temp = cmdhist0;

	cmdhist0 = malloc(sizeof(struct cmdhistll_t));
	cmdhist0->cmd = malloc(strlen(cmd) + 1);
	strcpy(cmdhist0->cmd, cmd);
	cmdhist0->prev = temp;
	cmdhist0->next = NULL;

	if(temp)
		temp->next = cmdhist0;

	ccmdhist = NULL;
}


void console_print(const char *fmt, ...)
{
	pthread_mutex_lock(&console_mutex);
	
	char *msg;
	
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

	printf(msg);
	fflush(stdout);
	
	char *temp = msg;
	
	while(1)
	{
		if(newline)
		{
			if(!console0)
			{
				console0 = malloc(sizeof(struct consolell_t));
				console0->numlines = 1;
				console0->prev = NULL;
				console0->next = NULL;
			}
			else
			{
				if(console0->numlines == CONSOLE_LL_GRAN)
				{
					struct consolell_t *temp = console0;
					console0 = malloc(sizeof(struct consolell_t));
					console0->numlines = 1;
					console0->prev = temp;
					console0->prev->next = console0;
					console0->next = NULL;
				}
				else
					console0->numlines++;
			}
		}


		int l = 0;
		char *cc = msg;
		
		while(*cc && *cc != '\n')
			l++, cc++;


		if(!l && newline)
		{
			console0->line[console0->numlines - 1] = malloc(1);
			*console0->line[console0->numlines - 1] = '\0';
		}

		
		if(l)
		{
			if(newline)
			{
				console0->line[console0->numlines - 1] = malloc(l + 1);
				strncpy(console0->line[console0->numlines - 1], msg, l);
				console0->line[console0->numlines - 1][l] = '\0';
			}
			else
			{
				char *temp = console0->line[console0->numlines - 1];

				console0->line[console0->numlines - 1] = malloc(strlen(temp) + l + 1);

				strcpy(console0->line[console0->numlines - 1], temp);

				free(temp);

				strncat(console0->line[console0->numlines - 1], msg, l);
			}
		}


		newline = 0;

		if(*cc == '\n')
		{
			newline = 1;
			cc++;
		}

		if(*cc == '\0')
			break;

		msg = cc;
	}
	
	free(temp);
	pthread_mutex_unlock(&console_mutex);
}


void console_toggle()
{
	pthread_mutex_lock(&console_mutex);
	
	r_DrawConsole = !r_DrawConsole;

	if(r_DrawConsole)
	{
		console_inputting = 0;
		console_input[0] = '\0';
	}
	
	pthread_mutex_unlock(&console_mutex);
}


void prev_command(int state)
{
	pthread_mutex_lock(&console_mutex);

	if(!state)
		goto end;

	if(!r_DrawConsole)
		goto end;

	if(!ccmdhist)
	{
		ccmdhist = cmdhist0;

		if(ccmdhist)
		{
			strcpy(console_input, ccmdhist->cmd);

			console_inputting = 1;
		}
	}
	else
	{
		ccmdhist = ccmdhist->prev;

		if(!ccmdhist)
			ccmdhist = cmdhist0;

		strcpy(console_input, ccmdhist->cmd);

		console_inputting = 1;
	}

	end:
	pthread_mutex_unlock(&console_mutex);
}


void next_command(int state)
{
	pthread_mutex_lock(&console_mutex);
	
	if(!state)
		goto end;

	if(!r_DrawConsole)
		goto end;

	if(!ccmdhist)
	{
		ccmdhist = cmdhist0;

		if(ccmdhist)
		{
			while(ccmdhist->prev)
				ccmdhist = ccmdhist->prev;

			strcpy(console_input, ccmdhist->cmd);

			console_inputting = 1;
		}
	}
	else
	{
		ccmdhist = ccmdhist->next;

		if(!ccmdhist)
		{
			ccmdhist = cmdhist0;

			while(ccmdhist->prev)
				ccmdhist = ccmdhist->prev;
		}

		strcpy(console_input, ccmdhist->cmd);

		console_inputting = 1;
	}

	end:
	pthread_mutex_unlock(&console_mutex);
}


void console_tab(int state)
{
	pthread_mutex_lock(&console_mutex);
	
	if(!state)
		goto end;

	if(!r_DrawConsole)
		goto end;

	if(tabbed)
	{
		strcpy(console_input, old_console_input);
		list_cvars(console_input);
	}
	else
	{
		strcpy(old_console_input, console_input);

		if(console_inputting)
			complete_cvar(console_input);

		tabbed = 1;
	}

	end:
	pthread_mutex_unlock(&console_mutex);
}


void console_backspace(int state)
{
	pthread_mutex_lock(&console_mutex);

	if(!state)
		goto end;

	if(!r_DrawConsole)
		goto end;

	int l = strlen(console_input);

	if(l != 0)
		console_input[l - 1] = '\0';

	tabbed = 0;

	end:
	pthread_mutex_unlock(&console_mutex);
}


void console_enter(int state)
{
	pthread_mutex_lock(&console_mutex);

	if(!state)
		goto end;
	
	if(!r_DrawConsole)
		goto end;

	int l = strlen(console_input);

	if(l != 0 && console_inputting)
	{
		console_print(console_input);
		console_print("\n");
		add_cmd_hist(console_input);
		struct string_t *s = new_string_text(console_input);
		console_input[0] = '\0';	// why can't we do this after?

		console_inputting = 0;

		tabbed = 0;

		if(rconing == RCCON_IN)
			rcon_command(s->text);
		else
			TEMP_FAILURE_RETRY(write(console_pipe[1], s->text, strlen(s->text) + 1));

		free_string(s);

		goto end;
	}

	console_inputting = 0;

	tabbed = 0;

	end:
	pthread_mutex_unlock(&console_mutex);
}


void console_keypress(char c)
{
	pthread_mutex_lock(&console_mutex);
	
	if(!r_DrawConsole)
		goto end;

	int l = strlen(console_input);

	console_inputting = 1;

	if(strlen(console_input) < CONSOLE_INPUT_LENGTH - 1)
	{
		console_input[l++] = c;
		console_input[l] = '\0';
	}

	tabbed = 0;

	end:
	pthread_mutex_unlock(&console_mutex);
}


void dump_console()
{
	struct string_t *string = new_string_string(emergence_home_dir);
	string_cat_text(string, "/client.log");
	
	FILE *file = fopen(string->text, "w");
	
	free_string(string);
	
	if(!file)
		return;

	int l;
	struct consolell_t *console = console0;

	if(console)
	{
		while(console->prev)
			console = console->prev;

		do
		{
			for(l = 0; l != console->numlines; l++)
			{
				fputs(console->line[l], file);
				fputc('\n', file);
			}

			console = console->next;

		} while(console);
	}

	fclose(file);
}


void calc_r_ConsoleColour()
{
	r_ConsoleColour = convert_24bit_to_16bit(r_ConsoleRed, r_ConsoleGreen, r_ConsoleBlue);
}


void qc_r_ConsoleRed(int val)
{
	r_ConsoleRed = val;
	calc_r_ConsoleColour();
}


void qc_r_ConsoleGreen(int val)
{
	r_ConsoleGreen = val;
	calc_r_ConsoleColour();
}


void qc_r_ConsoleBlue(int val)
{
	r_ConsoleBlue = val;
	calc_r_ConsoleColour();
}


void calc_r_RconConsoleColour()
{
	r_RconConsoleColour = convert_24bit_to_16bit(r_RconConsoleRed, r_RconConsoleGreen, r_RconConsoleBlue);
}


void qc_r_RconConsoleRed(int val)
{
	r_RconConsoleRed = val;
	calc_r_RconConsoleColour();
}


void qc_r_RconConsoleGreen(int val)
{
	r_RconConsoleGreen = val;
	calc_r_RconConsoleColour();
}


void qc_r_RconConsoleBlue(int val)
{
	r_RconConsoleBlue = val;
	calc_r_RconConsoleColour();
}


void calc_r_ConsoleTextColour()
{
	r_ConsoleTextColour = convert_24bit_to_16bit(r_ConsoleTextRed, r_ConsoleTextGreen, r_ConsoleTextBlue);
}


void qc_r_ConsoleTextRed(int val)
{
	r_ConsoleTextRed = val;
	calc_r_ConsoleTextColour();
}


void qc_r_ConsoleTextGreen(int val)
{
	r_ConsoleTextGreen = val;
	calc_r_ConsoleTextColour();
}


void qc_r_ConsoleTextBlue(int val)
{
	r_ConsoleTextBlue = val;
	calc_r_ConsoleTextColour();
}


void calc_r_ConsoleActiveTextColour()
{
	r_ConsoleActiveTextColour = convert_24bit_to_16bit(r_ConsoleActiveTextRed, r_ConsoleActiveTextGreen, r_ConsoleActiveTextBlue);
}


void qc_r_ConsoleActiveTextRed(int val)
{
	r_ConsoleActiveTextRed = val;
	calc_r_ConsoleActiveTextColour();
}


void qc_r_ConsoleActiveTextGreen(int val)
{
	r_ConsoleActiveTextGreen = val;
	calc_r_ConsoleActiveTextColour();
}


void qc_r_ConsoleActiveTextBlue(int val)
{
	r_ConsoleActiveTextBlue = val;
	calc_r_ConsoleActiveTextColour();
}


void init_console_cvars()
{
	create_cvar_int("r_ConsoleHeight", &r_ConsoleHeight, 0);
	create_cvar_int("r_ConsoleRed", &r_ConsoleRed, 0);
	create_cvar_int("r_ConsoleGreen", &r_ConsoleGreen, 0);
	create_cvar_int("r_ConsoleBlue", &r_ConsoleBlue, 0);
	create_cvar_int("r_ConsoleColour", &r_ConsoleColour, 0);
	create_cvar_int("r_ConsoleAlpha", &r_ConsoleAlpha, 0);
	create_cvar_int("r_RconConsoleRed", &r_RconConsoleRed, 0);
	create_cvar_int("r_RconConsoleGreen", &r_RconConsoleGreen, 0);
	create_cvar_int("r_RconConsoleBlue", &r_RconConsoleBlue, 0);
	create_cvar_int("r_RconConsoleColour", &r_RconConsoleColour, 0);
	create_cvar_int("r_ConsoleTextRed", &r_ConsoleTextRed, 0);
	create_cvar_int("r_ConsoleTextGreen", &r_ConsoleTextGreen, 0);
	create_cvar_int("r_ConsoleTextBlue", &r_ConsoleTextBlue, 0);
	create_cvar_int("r_ConsoleTextColour", &r_ConsoleTextColour, 0);
	create_cvar_int("r_ConsoleActiveTextRed", &r_ConsoleActiveTextRed, 0);
	create_cvar_int("r_ConsoleActiveTextGreen", &r_ConsoleActiveTextGreen, 0);
	create_cvar_int("r_ConsoleActiveTextBlue", &r_ConsoleActiveTextBlue, 0);
	create_cvar_int("r_ConsoleActiveTextColour", &r_ConsoleActiveTextColour, 0);
}


void init_console()
{
	console_input = malloc(CONSOLE_INPUT_LENGTH);
	old_console_input = malloc(CONSOLE_INPUT_LENGTH);
	console_input[0] = '\0';

	set_int_cvar_qc_function("r_ConsoleRed", qc_r_ConsoleRed);
	set_int_cvar_qc_function("r_ConsoleGreen", qc_r_ConsoleGreen);
	set_int_cvar_qc_function("r_ConsoleBlue", qc_r_ConsoleBlue);
	set_int_cvar_qc_function("r_RconConsoleRed", qc_r_RconConsoleRed);
	set_int_cvar_qc_function("r_RconConsoleGreen", qc_r_RconConsoleGreen);
	set_int_cvar_qc_function("r_RconConsoleBlue", qc_r_RconConsoleBlue);
	set_int_cvar_qc_function("r_ConsoleTextRed", qc_r_ConsoleTextRed);
	set_int_cvar_qc_function("r_ConsoleTextGreen", qc_r_ConsoleTextGreen);
	set_int_cvar_qc_function("r_ConsoleTextBlue", qc_r_ConsoleTextBlue);
	set_int_cvar_qc_function("r_ConsoleActiveTextRed", qc_r_ConsoleActiveTextRed);
	set_int_cvar_qc_function("r_ConsoleActiveTextGreen", qc_r_ConsoleActiveTextGreen);
	set_int_cvar_qc_function("r_ConsoleActiveTextBlue", qc_r_ConsoleActiveTextBlue);
	
	pipe(console_pipe);
	fcntl(console_pipe[0], F_SETFL, O_NONBLOCK);
}


void kill_console()
{
	free(console_input);
	free(old_console_input);

	console_input = NULL;
	old_console_input = NULL;
}
