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

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <memory.h>
#include <assert.h>
#include <pthread.h>

#include <zlib.h>

#include "prefix.h"
#include "resource.h"

#include "types.h"
#include "minmax.h"
#include "llist.h"
#include "stringbuf.h"
#include "buffer.h"
#include "resource.h"
#include "cvar.h"
#include "network.h"
#include "fileinfo.h"
#include "particles.h"
#include "sgame.h"
#include "rcon.h"
#include "console.h"
#include "gsub.h"
#include "render.h"
#include "main.h"
#include "stars.h"
#include "ping.h"
#include "tick.h"
#include "skin.h"
#include "game.h"
#include "line.h"
#include "entry.h"
#include "sound.h"
#include "floats.h"
#include "ris.h"
#include "key.h"
#include "download.h"
#include "map.h"
#include "servers.h"

#ifdef LINUX
#include "timer.h"
#endif

#ifdef WIN32
#include "win32/math.h"
#endif


struct event_craft_data_t
{
	float acc;
	float theta;
	int braking;
	int rolling_left;
	int rolling_right;
	uint32_t left_weapon_index;
	uint32_t right_weapon_index;
	float shield_flare;
	
	uint8_t shield_red, shield_green, shield_blue;
	uint8_t magic_smoke;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
	uint8_t plasma_red, plasma_green, plasma_blue;
	
	int carcass;	// handled sepatately
};


struct event_weapon_data_t
{
	int type;
	float theta;
	uint32_t craft_index;
	float shield_flare;
	
	uint8_t shield_red, shield_green, shield_blue;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
	
	int detached;
};


struct event_rocket_data_t
{
	float theta;
	uint32_t start_tick;
	uint8_t in_weapon;
	uint32_t weapon_id;
	uint8_t magic_smoke;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
};


struct event_mine_data_t
{
	uint8_t under_craft;
	uint32_t craft_id;
};


struct event_plasma_data_t
{
	uint8_t in_weapon;
	uint32_t weapon_id;
	uint8_t red, green, blue;
};


struct event_bullet_data_t
{
	uint8_t in_weapon;
	uint32_t weapon_id;
};


struct event_t
{
	uint32_t tick;
	uint8_t type;
	int ooo;
	int applied;
	uint32_t index;
	
	union
	{
		struct
		{
			uint32_t index;
			uint8_t type;
			
			uint32_t skin;
			
			float xdis, ydis;
			float xvel, yvel;
			
			int teleporting;
			uint32_t teleporting_tick;
			uint32_t teleport_spawn_index;
			
			union
			{
				struct event_craft_data_t craft_data;
				struct event_weapon_data_t weapon_data;
				struct event_plasma_data_t plasma_data;
				struct event_bullet_data_t bullet_data;
				struct event_rocket_data_t rocket_data;
				struct event_mine_data_t mine_data;
			};
			
		} ent_data;
		
		struct
		{
			uint32_t index;
			
		} follow_me_data;
		
		struct
		{
			uint32_t index;
			
		} carcass_data;
		
		struct
		{
			float x1, y1;
			float x2, y2;
			
			uint8_t inner_red, inner_green, inner_blue;
			uint8_t outer_red, outer_green, outer_blue;
			
		} railtrail_data;
		
		struct
		{
			float xdis, ydis;
			float xvel, yvel;
			float size;
			uint8_t magic;
			uint8_t start_red, start_green, start_blue;
			uint8_t end_red, end_green, end_blue;
			
		} explosion_data;
		
		struct
		{
			uint32_t index;
			uint8_t magic_smoke;
			uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
			uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
			uint8_t shield_red, shield_green, shield_blue;
			uint8_t plasma_red, plasma_green, plasma_blue;
			
		} colours_data;
		
		struct
		{
			float craft_shield;
			float left_shield, right_shield;
			
		} shield_strengths_data;
		
		struct
		{
			int rails, mines;
			int left, right;
			
		} ammo_levels_data;
		
		struct
		{
			float x, y;
			
		} static_sound_data;
	};
	
	struct event_t *next;
	
} *event0 = NULL;


struct message_reader_t
{
	uint8_t type;
	struct buffer_t *stream;
	gzFile gzdemo;

	uint8_t message_type;
	uint32_t event_tick;

} message_reader;


#define MESSAGE_READER_STREAM					0
#define MESSAGE_READER_GZDEMO					1
#define MESSAGE_READER_STREAM_WRITE_GZDEMO		2
#define MESSAGE_READER_STREAM_WRITE_GZDEMO_NO	3	// we are reading a net only message



int game_state = GAMESTATE_DEAD;
int recording = 0;
struct string_t *recording_filename;
gzFile gzrecording;

uint32_t cgame_tick;
uint32_t render_tick;
float cgame_time;

struct entity_t *centity0;

double viewx = 0.0, viewy = 0.0;

uint8_t ready;
int match_begun;
int match_over;
uint32_t match_start_tick;
uint32_t match_end_tick;

int moving_view;
float moving_view_time;

float moving_view_x;
float moving_view_y;

float moving_view_xa;
float moving_view_ya;


#define MOVING_VIEW_TRAVEL_TIME 1.0


float offset_view_x;
float offset_view_y;

float teleporting_start_x, teleporting_start_y;


struct ris_t *ris_plasma, *ris_craft_shield, *ris_weapon_shield, 
	*ris_shield_pickup, *ris_mine;

uint32_t game_conn;

struct game_state_t *game_state0, last_known_game_state, *cgame_state;


uint32_t demo_first_tick;
uint32_t demo_last_tick;
uint32_t demo_follow_me;

gzFile gzdemo;

#define RAIL_TRAIL_EXPAND_TIME 0.5
#define RAIL_TRAIL_EXPAND_DISTANCE 10.0
#define RAIL_TRAIL_INITIAL_EXPAND_VELOCITY ((2 * RAIL_TRAIL_EXPAND_DISTANCE) / RAIL_TRAIL_EXPAND_TIME)
#define RAIL_TRAIL_EXPAND_ACC ((-RAIL_TRAIL_INITIAL_EXPAND_VELOCITY) / RAIL_TRAIL_EXPAND_TIME)


pthread_mutex_t gamestate_mutex = PTHREAD_MUTEX_INITIALIZER;


uint8_t winner_type;
uint32_t winner_index;

struct rail_trail_t
{
	float start_time;
	float x1, y1;
	float x2, y2;
	
	uint8_t inner_red, inner_green, inner_blue;
	uint8_t outer_red, outer_green, outer_blue;

	struct rail_trail_t *next;
		
} *rail_trail0;

struct sample_t *railgun_sample;
struct sample_t *teleporter_sample;
struct sample_t *speedup_ramp_sample;
struct sample_t *explosion_sample;
struct sample_t *plasma_cannon_sample;
struct sample_t *rocket_sample;


struct demo_t
{
	struct string_t *filename;
	struct demo_t *next;
		
} *demo0, *cdemo;

uint32_t demo_start_frame;
float demo_start_time;


int game_rendering = 1;


int rail_inner_red = 25, rail_inner_green = 187, rail_inner_blue = 241;
int rail_outer_red = 0, rail_outer_green = 86, rail_outer_blue = 217;

int magic_smoke = 0;
int smoke_start_red = 218, smoke_start_green = 210, smoke_start_blue = 45;
int smoke_end_red = 97, smoke_end_green = 12, smoke_end_blue = 0;

int plasma_red = 187, plasma_green = 241, plasma_blue = 0xff;

int shield_red = 56, shield_green = 23, shield_blue = 245;

int old_proto;


struct player_t
{
	uint32_t index;
	struct string_t *name;
	uint8_t ready;
	int frags;
	
	struct player_t *next;
	
} *player0 = NULL;


void clear_game()
{
	LL_REMOVE_ALL(struct event_t, &event0);
	moving_view = 0;
	
	struct game_state_t *cgame_state = game_state0;
		
	while(cgame_state)
	{
		LL_REMOVE_ALL(struct entity_t, &cgame_state->entity0);
		cgame_state = cgame_state->next;
	}
	
	LL_REMOVE_ALL(struct entity_t, &last_known_game_state.entity0);
	memset(&last_known_game_state, 0, sizeof(last_known_game_state));
	
	LL_REMOVE_ALL(struct game_state_t, &game_state0);
		
	LL_REMOVE_ALL(struct rail_trail_t, &rail_trail0);
	
	clear_sgame();
	clear_floating_images();
	clear_ticks();
	clear_skins();
	clear_particles();
	
	if((game_state == GAMESTATE_SPECTATING || game_state == GAMESTATE_PLAYING) && recording)
	{
		recording = 0;
		gzclose(gzrecording);
	}
	
	cgame_tick = 0;
	cgame_time = 0.0;
	
	memset(&message_reader, 0, sizeof(message_reader));
	
	centity0 = NULL;
	
	match_begun = 0;
	match_over = 0;
	LL_REMOVE_ALL(struct player_t, &player0);
}


/*
void world_to_screen(double worldx, double worldy, int *screenx, int *screeny)
{
	*screenx = (int)floor(worldx * (double)(vid_width) / 1600.0) - 
		(int)floor(viewx * (double)(vid_width) / 1600.0) + vid_width / 2;
	*screeny = vid_height / 2 - 1 - (int)floor(worldy * (double)(vid_width) / 1600.0) + 
		(int)floor(viewy * (double)(vid_width) / 1600.0);
}


void screen_to_world(int screenx, int screeny, double *worldx, double *worldy)
{
	*worldx = ((double)screenx - vid_width / 2 + 0.5f) / 
		((double)(vid_width) / 1600.0) + viewx;
	*worldy = (((double)(vid_height / 2 - 1 - screeny)) + 0.5f) / 
		((double)(vid_width) / 1600.0) + viewx;
}
*/

/*
void world_to_screen(double worldx, double worldy, int *screenx, int *screeny)
{
	*screenx = (int)floor((worldx - viewx) * ((double)(vid_width) / 1600.0)) + vid_width / 2;
	*screeny = vid_height / 2 - 1 - (int)floor((worldy - viewy) * ((double)(vid_width) / 1600.0));
}


void screen_to_world(int screenx, int screeny, double *worldx, double *worldy)
{
	*worldx = ((double)screenx - vid_width / 2 + 0.5f) / 
		((double)(vid_width) / 1600.0) + viewx;
	*worldy = (((double)(vid_height / 2 - 1 - screeny)) + 0.5f) / 
		((double)(vid_width) / 1600.0) + viewx;
}
*/
/*
void add_offset_view(struct entity_t *entity)
{
	double sin_theta, cos_theta;
	sincos(entity->craft_data.theta, &sin_theta, &cos_theta);
	
	double target_x = - sin_theta * 500;// + entity->xvel * 24.0;
	double target_y = + cos_theta * 500;// + entity->yvel * 24.0;
	
	double deltax = target_x - offset_view_x;
	double deltay = target_y - offset_view_y;
	
	offset_view_x += deltax / 500.0;
	offset_view_y += deltay / 500.0;
	
	viewx += offset_view_x;
	viewy += offset_view_y;
}
*/

void start_moving_view(float x1, float y1, float x2, float y2)
{
	moving_view_x = x2 - x1;
	moving_view_y = y2 - y1;
	
	moving_view_xa = ((moving_view_x) / 2) / (0.5 * (MOVING_VIEW_TRAVEL_TIME * 0.5) * 
		(MOVING_VIEW_TRAVEL_TIME * 0.5));
	moving_view_ya = ((moving_view_y) / 2) / (0.5 * (MOVING_VIEW_TRAVEL_TIME * 0.5) * 
		(MOVING_VIEW_TRAVEL_TIME * 0.5));
	
	moving_view_time = cgame_time;
}


void add_moving_view()
{
	double time = cgame_time - moving_view_time;
	
	if(time > MOVING_VIEW_TRAVEL_TIME)
		return;
	
	viewx -= moving_view_x;
	viewy -= moving_view_y;
	
	if(time < MOVING_VIEW_TRAVEL_TIME * 0.5)
	{
		viewx += 0.5 * moving_view_xa * time * time;
		viewy += 0.5 * moving_view_ya * time * time;
	}
	else
	{
		viewx += 0.5 * moving_view_xa * MOVING_VIEW_TRAVEL_TIME * 0.5 * 
			MOVING_VIEW_TRAVEL_TIME * 0.5;
		viewy += 0.5 * moving_view_ya * MOVING_VIEW_TRAVEL_TIME * 0.5 * 
			MOVING_VIEW_TRAVEL_TIME * 0.5;
		
		viewx += moving_view_xa * MOVING_VIEW_TRAVEL_TIME * 0.5 * 
			(time - MOVING_VIEW_TRAVEL_TIME * 0.5);
		viewy += moving_view_ya * MOVING_VIEW_TRAVEL_TIME * 0.5 * 
			(time - MOVING_VIEW_TRAVEL_TIME * 0.5);
		
		viewx += 0.5 * -moving_view_xa * (time - MOVING_VIEW_TRAVEL_TIME * 0.5) * 
			(time - MOVING_VIEW_TRAVEL_TIME * 0.5);
		viewy += 0.5 * -moving_view_ya * (time - MOVING_VIEW_TRAVEL_TIME * 0.5) * 
			(time - MOVING_VIEW_TRAVEL_TIME * 0.5);
	}
}


int message_reader_more()
{
	switch(message_reader.type)
	{
	case MESSAGE_READER_STREAM:
	case MESSAGE_READER_STREAM_WRITE_GZDEMO:
	case MESSAGE_READER_STREAM_WRITE_GZDEMO_NO:
		return buffer_more(message_reader.stream);
	
	case MESSAGE_READER_GZDEMO:
		return !gzeof(message_reader.gzdemo);
	}
	
	return 0;
}


uint8_t message_reader_read_uint8()
{
	uint8_t d;
	
	switch(message_reader.type)
	{
	case MESSAGE_READER_STREAM:
	case MESSAGE_READER_STREAM_WRITE_GZDEMO_NO:
		return buffer_read_uint8(message_reader.stream);
	
	case MESSAGE_READER_GZDEMO:
		gzread(message_reader.gzdemo, &d, 1);
		return d;

	case MESSAGE_READER_STREAM_WRITE_GZDEMO:
		d = buffer_read_uint8(message_reader.stream);
		gzwrite(message_reader.gzdemo, &d, 1);
		return d;
	}
	
	return 0;
}


uint32_t message_reader_read_uint32()
{
	uint32_t d;
	
	switch(message_reader.type)
	{
	case MESSAGE_READER_STREAM:
	case MESSAGE_READER_STREAM_WRITE_GZDEMO_NO:
		return buffer_read_uint32(message_reader.stream);
	
	case MESSAGE_READER_GZDEMO:
		gzread(message_reader.gzdemo, &d, 4);
		return d;

	case MESSAGE_READER_STREAM_WRITE_GZDEMO:
		d = buffer_read_uint32(message_reader.stream);
		gzwrite(message_reader.gzdemo, &d, 4);
		return d;
	}
	
	return 0;
}


int message_reader_read_int()
{
	uint32_t d = message_reader_read_uint32();
	return *(int*)(&d);
}


float message_reader_read_float()
{
	uint32_t d = message_reader_read_uint32();
	return *(float*)(void*)(&d);
}


struct string_t *message_reader_read_string()
{
	struct string_t *s;
	
	switch(message_reader.type)
	{
	case MESSAGE_READER_STREAM:
	case MESSAGE_READER_STREAM_WRITE_GZDEMO_NO:
		return buffer_read_string(message_reader.stream);
	
	case MESSAGE_READER_GZDEMO:
		s = gzread_string(message_reader.gzdemo);
		return s;

	case MESSAGE_READER_STREAM_WRITE_GZDEMO:
		s = buffer_read_string(message_reader.stream);
		gzwrite_string(message_reader.gzdemo, s);
		return s;
	}
	
	return NULL;
}


int message_reader_new_message()
{
	if(!message_reader_more())
		return 0;
	
	int old_reader_type = message_reader.type;
	
	if(old_reader_type == MESSAGE_READER_STREAM_WRITE_GZDEMO_NO)
		old_reader_type = MESSAGE_READER_STREAM_WRITE_GZDEMO;
		
	if(message_reader.type == MESSAGE_READER_STREAM_WRITE_GZDEMO)
		message_reader.type = MESSAGE_READER_STREAM_WRITE_GZDEMO_NO;
	
	message_reader.message_type = message_reader_read_uint8();
	
	switch((message_reader.message_type & EMMSGCLASS_MASK))
	{
	case EMMSGCLASS_STND:
		if(old_reader_type == MESSAGE_READER_STREAM_WRITE_GZDEMO)
			gzwrite(message_reader.gzdemo, &message_reader.message_type, 1);
		message_reader.type = old_reader_type;
		break;
	
	case EMMSGCLASS_NETONLY:
		if(old_reader_type == MESSAGE_READER_GZDEMO)
			;	// do something: the demo file is corrupt
		break;
	
	case EMMSGCLASS_EVENT:
		if(message_reader.message_type != EMEVENT_PULSE)
		{
			if(old_reader_type == MESSAGE_READER_STREAM_WRITE_GZDEMO)
				gzwrite(message_reader.gzdemo, &message_reader.message_type, 1);
			message_reader.type = old_reader_type;
		}
		
		message_reader.event_tick = message_reader_read_uint32();
		break;
	
	default:
		// do something: the demo file is corrupt
		break;
	}
	
	return 1;
}


void read_craft_data(struct event_craft_data_t *craft_data)
{
	craft_data->acc = message_reader_read_float();
	craft_data->theta = message_reader_read_float();
	craft_data->braking = message_reader_read_int();
	craft_data->rolling_left = message_reader_read_int();
	craft_data->rolling_right = message_reader_read_int();
	craft_data->left_weapon_index = message_reader_read_uint32();
	craft_data->right_weapon_index = message_reader_read_uint32();
	craft_data->shield_flare = message_reader_read_float();
}


void read_weapon_data(struct event_weapon_data_t *weapon_data)
{
	weapon_data->type = message_reader_read_int();
	weapon_data->theta = message_reader_read_float();
	weapon_data->craft_index = message_reader_read_uint32();
	weapon_data->shield_flare = message_reader_read_float();
}


void read_rocket_data(struct event_rocket_data_t *rocket_data)
{
	rocket_data->theta = message_reader_read_float();
	rocket_data->start_tick = message_reader_read_uint32();
	rocket_data->in_weapon = message_reader_read_uint8();
	rocket_data->weapon_id = message_reader_read_uint32();
}


void read_plasma_data(struct event_plasma_data_t *plasma_data)
{
	plasma_data->in_weapon = message_reader_read_uint8();
	plasma_data->weapon_id = message_reader_read_uint32();
}


void read_bullet_data(struct event_bullet_data_t *bullet_data)
{
	bullet_data->in_weapon = message_reader_read_uint8();
	bullet_data->weapon_id = message_reader_read_uint32();
}


void read_mine_data(struct event_mine_data_t *mine_data)
{
	mine_data->under_craft = message_reader_read_uint8();
	mine_data->craft_id = message_reader_read_uint32();
}


void game_process_connecting()
{
	console_print(">");
}


void game_process_cookie_echoed()
{
	console_print("<\xbb");
}


void game_process_connection(uint32_t conn)
{
	switch(game_state)
	{
	case GAMESTATE_DEAD:
		break;
	
	case GAMESTATE_DEMO:
		gzclose(gzdemo);
		gzdemo = NULL;
		break;
	
	case GAMESTATE_CONNECTING:
	case GAMESTATE_SPECTATING:
	case GAMESTATE_JOINING:
	case GAMESTATE_CREATING_SESSION:
	case GAMESTATE_AWAITING_APPROVAL:
	case GAMESTATE_PLAYING:
		em_disconnect(game_conn);
		break;
	}
	
	clear_game();
	
	
	game_state = GAMESTATE_CONNECTING;
	game_conn = conn;
	
	if(recording)
	{
		message_reader.type = MESSAGE_READER_STREAM_WRITE_GZDEMO;
		message_reader.gzdemo = gzrecording;
	}
	else
	{
		message_reader.type = MESSAGE_READER_STREAM;
	}
	
	stop_server_discovery();
	console_print("\xab\nConnected!\n");
}


void game_process_connection_failed()
{
	console_print("\nFailed to connect!\n");
}


void game_process_disconnection(uint32_t conn)
{
	if(game_state == GAMESTATE_DEMO)
		return;
		
	if(conn == game_conn)
		game_state = GAMESTATE_DEAD;
	
//	LL_REMOVE_ALL(struct entity_t, &entity0);
	
	console_print("Disconnected!\n");
}


void game_process_conn_lost(uint32_t conn)
{
	game_state = GAMESTATE_DEAD;
	console_print("Connetion lost.\n");
}


int game_process_print()
{
	struct string_t *s = message_reader_read_string();
	console_print(s->text);
	free_string(s);
	return 1;
}


void emit_colours()
{
	net_emit_uint8(game_conn, rail_inner_red);
	net_emit_uint8(game_conn, rail_inner_green);
	net_emit_uint8(game_conn, rail_inner_blue);
	net_emit_uint8(game_conn, rail_outer_red);
	net_emit_uint8(game_conn, rail_outer_green);
	net_emit_uint8(game_conn, rail_outer_blue);

	net_emit_uint8(game_conn, magic_smoke);
	net_emit_uint8(game_conn, smoke_start_red);
	net_emit_uint8(game_conn, smoke_start_green);
	net_emit_uint8(game_conn, smoke_start_blue);
	net_emit_uint8(game_conn, smoke_end_red);
	net_emit_uint8(game_conn, smoke_end_green);
	net_emit_uint8(game_conn, smoke_end_blue);

	net_emit_uint8(game_conn, plasma_red);
	net_emit_uint8(game_conn, plasma_green);
	net_emit_uint8(game_conn, plasma_blue);

	net_emit_uint8(game_conn, shield_red);
	net_emit_uint8(game_conn, shield_green);
	net_emit_uint8(game_conn, shield_blue);
}


int game_process_proto_ver()
{
	if(game_state != GAMESTATE_CONNECTING && game_state != GAMESTATE_DEMO)
		return 0;
	
	uint8_t proto_ver = message_reader_read_uint8();
	
	if(proto_ver == EM_PROTO_VER - 1)
	{
		old_proto = 1;
		proto_ver = EM_PROTO_VER;
	}
	else
		old_proto = 0;
	
	if(proto_ver == EM_PROTO_VER)
	{
		console_print("Correct protocol version\n");
		
		if(game_state != GAMESTATE_DEMO)
		{
			emit_servers(game_conn);
			net_emit_uint8(game_conn, EMMSG_JOIN);
			net_emit_end_of_stream(game_conn);
			game_state = GAMESTATE_JOINING;
		}
	}
	else
	{
		console_print("Incorrect protocol version\n");
		
		if(game_state != GAMESTATE_DEMO)
			em_disconnect(game_conn);
		
		clear_game();
		game_state = GAMESTATE_DEAD;
		
		return 0;
	}
	
	return 1;
}


int game_process_authenticate()
{
	if(game_state != GAMESTATE_JOINING)
		return 1;
	
	console_print("Authenticating\n>");
	
	game_state = GAMESTATE_CREATING_SESSION;
	
	if(!key_create_session())
	{
		console_print("!\nYou don't have a key.\nTo play online you must purchase a key from emergence.uk.net");
		game_state = GAMESTATE_DEAD;
	}
	
	return 1;
}


void game_process_key_accepted(char temp_key[16])
{
	if(game_state != GAMESTATE_CREATING_SESSION)
		return;
	
	console_print("<\xbb");
	
	net_emit_uint8(game_conn, EMMSG_SESSION_KEY);
	net_emit_buf(game_conn, temp_key, 16);
	net_emit_end_of_stream(game_conn);
	
	game_state = GAMESTATE_AWAITING_APPROVAL;
}


void game_process_key_declined()
{
	if(game_state != GAMESTATE_CREATING_SESSION)
		return;
	
	console_print("!\nYour key does not seem to be valid.\n");
	
	game_state = GAMESTATE_DEAD;
}


void game_process_key_error()
{
	if(game_state != GAMESTATE_CREATING_SESSION)
		return;
	
	console_print("!\nError authenticating user.\n");
	
	game_state = GAMESTATE_DEAD;
}


int game_process_joined()
{
//	if(game_state != GAMESTATE_AWAITING_APPROVAL)
//		return 1;
	
	console_print("\xab\n");

	game_state = GAMESTATE_JOINED;
	
	char *name = get_cvar_string("name");
	
	net_emit_uint8(game_conn, EMMSG_NAMECNGE);
	net_emit_string(game_conn, name);
	net_emit_uint8(game_conn, EMMSG_COLOURS);
	emit_colours();
	net_emit_uint8(game_conn, EMMSG_PLAY);
	net_emit_end_of_stream(game_conn);
	
	free(name);
	
	return 1;
}


int game_process_failed()
{
	if(game_state != GAMESTATE_AWAITING_APPROVAL && 
		game_state != GAMESTATE_JOINED &&
		game_state != GAMESTATE_PLAYING)
		return 1;
	
	console_print("!\nAuthentication failed.\n");

	game_state = GAMESTATE_DEAD;
	em_disconnect(game_conn);
	
	return 1;
}


int game_process_playing()
{
	r_DrawConsole = 0;
	
	if(game_state == GAMESTATE_DEMO)
	{
		message_reader_read_uint32();
		return 1;
	}
	
	if(game_state == GAMESTATE_DEAD)
		return 0;
	
	if(game_state == GAMESTATE_AWAITING_APPROVAL)
		console_print("\xab\n");
	
	if(!map_loaded)
	{
		game_state = GAMESTATE_DEAD;
		em_disconnect(game_conn);
		return 0;
	}
	
	game_state0 = calloc(1, sizeof(struct game_state_t));
	game_state0->tick = message_reader_read_uint32();		// oh dear
	
	last_known_game_state.tick = game_state0->tick;
	
	
	game_state = GAMESTATE_PLAYING;

	net_emit_uint8(game_conn, EMMSG_START);
	net_emit_end_of_stream(game_conn);
	
	match_begun = 0;
	match_over = 0;
	ready = 0;
	
	return 1;
}


int game_process_spectating()
{
	r_DrawConsole = 0;
	
	if(game_state == GAMESTATE_DEMO)
	{
		message_reader_read_uint32();
		return 1;
	}
	
	if(game_state == GAMESTATE_DEAD)
		return 0;
	
	if(game_state == GAMESTATE_AWAITING_APPROVAL)
		console_print("\xab\n");
	
	game_state0 = calloc(1, sizeof(struct game_state_t));
	game_state0->tick = message_reader_read_uint32();		// oh dear
	
	last_known_game_state.tick = game_state0->tick;
	
	
	game_state = GAMESTATE_SPECTATING;

	net_emit_uint8(game_conn, EMMSG_START);
	net_emit_end_of_stream(game_conn);
	
	return 1;
}


void toggle_ready(int state)
{
	if(!state)
		return;
	
	if(game_state != GAMESTATE_PLAYING || (match_begun & !match_over))
		return;
	
	ready = !ready;
	
	net_emit_uint8(game_conn, EMMSG_READY);
	net_emit_uint8(game_conn, ready);
	net_emit_end_of_stream(game_conn);
}


int game_process_match_begun()
{
	match_begun = 1;
	match_over = 0;
	match_start_tick = message_reader_read_uint32();
	
	console_print("The match has begun.\n");
	
	return 1;
}


int game_process_match_over()
{
	match_over = 1;
	console_print("The match is over.\n");
	
	match_end_tick = message_reader_read_uint32();
	
	winner_type = message_reader_read_uint8();
	
	if(winner_type == WINNER_INDEX)
	{
		winner_index = message_reader_read_uint32();
		
		struct player_t *cplayer = player0;
		
		while(cplayer)
		{
			if(cplayer->index == winner_index)
				break;
			
			cplayer = cplayer->next;
		}
		
		console_print("%s has won.\n", cplayer->name->text);
	}
	else
	{
		console_print("You have won.\n");
	}

	return 1;
}


int game_process_lobby()
{
	match_begun = 0;
	match_over = 0;
	ready = 0;

	console_print("The game has returned to the lobby.\n");
	
	return 1;
}


int game_process_player_info()
{
	uint32_t index = message_reader_read_uint32();
	
	struct player_t *cplayer = player0;
		
	while(cplayer)
	{
		if(cplayer->index == index)
		{
			free_string(cplayer->name);
			cplayer->name = message_reader_read_string();
			cplayer->ready = message_reader_read_uint8();
			cplayer->frags = message_reader_read_int();
			break;
		}
		
		cplayer = cplayer->next;
	}
	
	if(!cplayer)
	{
		struct player_t player;
		player.index = index;
		player.name = message_reader_read_string();
		player.ready = message_reader_read_uint8();
		player.frags = message_reader_read_int();
		
		LL_ADD(struct player_t, &player0, &player);
	}
	
	
	if(match_begun)
	{
		// sort players by frag
		
		struct player_t *new_player0 = NULL, *lowest_player;
			
		while(player0)
		{
			lowest_player = player0;
			cplayer = player0->next;
			
			while(cplayer)
			{
				if(cplayer->frags < lowest_player->frags)
					lowest_player = cplayer;
				else if(cplayer->frags == lowest_player->frags)
					if(cplayer->index < lowest_player->index)
						lowest_player = cplayer;
			
				cplayer = cplayer->next;
			}
			
			LL_ADD(struct player_t, &new_player0, lowest_player);
			LL_REMOVE(struct player_t, &player0, lowest_player);
		}
		
		player0 = new_player0;
	}
	
	return 1;
}


int game_process_remove_player_info()
{
	uint32_t index = message_reader_read_uint32();
	
	struct player_t *cplayer = player0;
		
	while(cplayer)
	{
		if(cplayer->index == index)
		{
			LL_REMOVE(struct player_t, &player0, cplayer);
			break;
		}
		
		cplayer = cplayer->next;
	}
	
	return 1;
}


void add_spawn_ent_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
	event->ent_data.type = message_reader_read_uint8();
	event->ent_data.skin = message_reader_read_uint32();
	event->ent_data.xdis = message_reader_read_float();
	event->ent_data.ydis = message_reader_read_float();
	event->ent_data.xvel = message_reader_read_float();
	event->ent_data.yvel = message_reader_read_float();
	event->ent_data.teleporting = message_reader_read_int();
	event->ent_data.teleporting_tick = message_reader_read_uint32();
	event->ent_data.teleport_spawn_index = message_reader_read_uint32();
	
	switch(event->ent_data.type)
	{
	case ENT_CRAFT:
		read_craft_data(&event->ent_data.craft_data);
	
		event->ent_data.craft_data.magic_smoke = message_reader_read_uint8();
	
		event->ent_data.craft_data.smoke_start_red = message_reader_read_uint8();
		event->ent_data.craft_data.smoke_start_green = message_reader_read_uint8();
		event->ent_data.craft_data.smoke_start_blue = message_reader_read_uint8();
	
		event->ent_data.craft_data.smoke_end_red = message_reader_read_uint8();
		event->ent_data.craft_data.smoke_end_green = message_reader_read_uint8();
		event->ent_data.craft_data.smoke_end_blue = message_reader_read_uint8();
	
		event->ent_data.craft_data.shield_red = message_reader_read_uint8();
		event->ent_data.craft_data.shield_green = message_reader_read_uint8();
		event->ent_data.craft_data.shield_blue = message_reader_read_uint8();
	
		event->ent_data.craft_data.plasma_red = message_reader_read_uint8();
		event->ent_data.craft_data.plasma_green = message_reader_read_uint8();
		event->ent_data.craft_data.plasma_blue = message_reader_read_uint8();
	
		event->ent_data.craft_data.carcass = message_reader_read_uint8();
		break;
	
	case ENT_WEAPON:
		read_weapon_data(&event->ent_data.weapon_data);
	
		event->ent_data.weapon_data.shield_red = message_reader_read_uint8();
		event->ent_data.weapon_data.shield_green = message_reader_read_uint8();
		event->ent_data.weapon_data.shield_blue = message_reader_read_uint8();
	
		event->ent_data.weapon_data.detached = message_reader_read_uint8();
		break;
	
	case ENT_PLASMA:
		read_plasma_data(&event->ent_data.plasma_data);

		event->ent_data.plasma_data.red = message_reader_read_uint8();
		event->ent_data.plasma_data.green = message_reader_read_uint8();
		event->ent_data.plasma_data.blue = message_reader_read_uint8();
		break;
	
	case ENT_BULLET:
		read_bullet_data(&event->ent_data.bullet_data);
		break;
	
	case ENT_ROCKET:
		read_rocket_data(&event->ent_data.rocket_data);
	
		event->ent_data.rocket_data.magic_smoke = message_reader_read_uint8();

		event->ent_data.rocket_data.smoke_start_red = message_reader_read_uint8();
		event->ent_data.rocket_data.smoke_start_green = message_reader_read_uint8();
		event->ent_data.rocket_data.smoke_start_blue = message_reader_read_uint8();
	
		event->ent_data.rocket_data.smoke_end_red = message_reader_read_uint8();
		event->ent_data.rocket_data.smoke_end_green = message_reader_read_uint8();
		event->ent_data.rocket_data.smoke_end_blue = message_reader_read_uint8();
		break;
	
	case ENT_MINE:
		read_mine_data(&event->ent_data.mine_data);
		break;
	}
}


void process_spawn_ent_event(struct event_t *event)
{
	struct entity_t *entity = new_entity(&centity0);
		
	assert(entity);

	entity->index = event->ent_data.index;
	entity->type = event->ent_data.type;
	entity->xdis = event->ent_data.xdis;
	entity->ydis = event->ent_data.ydis;
	entity->xvel = event->ent_data.xvel;
	entity->yvel = event->ent_data.yvel;
	entity->teleporting = event->ent_data.teleporting;
	entity->teleporting_tick = event->ent_data.teleporting_tick;
	entity->teleport_spawn_index = event->ent_data.teleport_spawn_index;
	
	switch(entity->type)
	{
	case ENT_CRAFT:
		entity->craft_data.acc = event->ent_data.craft_data.acc;
		entity->craft_data.old_theta = entity->craft_data.theta = 
			event->ent_data.craft_data.theta;
		entity->craft_data.braking = event->ent_data.craft_data.braking;
		entity->craft_data.rolling_left = event->ent_data.craft_data.rolling_left;
		entity->craft_data.rolling_right = event->ent_data.craft_data.rolling_right;
	
		entity->craft_data.left_weapon = NULL;
	/*get_entity(centity0, 
			event->ent_data.craft_data.left_weapon_index);
	
		if(entity->craft_data.left_weapon)
			entity->craft_data.left_weapon->weapon_data.craft = entity;
		else
			if(event->ent_data.craft_data.left_weapon_index != NO_ENT_INDEX)
				console_print("left weapon not found\n");
	
	*/	entity->craft_data.right_weapon = NULL;
	/*get_entity(centity0, 
			event->ent_data.craft_data.right_weapon_index);
	
		if(entity->craft_data.right_weapon)
			entity->craft_data.right_weapon->weapon_data.craft = entity;
		else
			if(event->ent_data.craft_data.right_weapon_index != NO_ENT_INDEX)
				console_print("right weapon not found\n");
	*/	
		entity->craft_data.shield_flare = event->ent_data.craft_data.shield_flare;
	
		entity->craft_data.magic_smoke = event->ent_data.craft_data.magic_smoke;
	
		entity->craft_data.smoke_start_red = event->ent_data.craft_data.smoke_start_red;
		entity->craft_data.smoke_start_green = event->ent_data.craft_data.smoke_start_green;
		entity->craft_data.smoke_start_blue = event->ent_data.craft_data.smoke_start_blue;
	
		entity->craft_data.smoke_end_red = event->ent_data.craft_data.smoke_end_red;
		entity->craft_data.smoke_end_green = event->ent_data.craft_data.smoke_end_green;
		entity->craft_data.smoke_end_blue = event->ent_data.craft_data.smoke_end_blue;
	
		entity->craft_data.shield_red = event->ent_data.craft_data.shield_red;
		entity->craft_data.shield_green = event->ent_data.craft_data.shield_green;
		entity->craft_data.shield_blue = event->ent_data.craft_data.shield_blue;
	
		entity->craft_data.plasma_red = event->ent_data.craft_data.plasma_red;
		entity->craft_data.plasma_green = event->ent_data.craft_data.plasma_green;
		entity->craft_data.plasma_blue = event->ent_data.craft_data.plasma_blue;
	
		entity->craft_data.carcass = event->ent_data.craft_data.carcass;
	
		entity->craft_data.skin = event->ent_data.skin;
		entity->craft_data.surface = skin_get_craft_surface(event->ent_data.skin);
		entity->craft_data.particle = 0.0;
		break;
	
	case ENT_WEAPON:
		entity->weapon_data.type = event->ent_data.weapon_data.type;
		entity->weapon_data.theta = event->ent_data.weapon_data.theta;
	
		entity->weapon_data.craft = NULL;
	/*get_entity(centity0, 
			event->ent_data.weapon_data.craft_index);
	
		if(!entity->weapon_data.craft && event->ent_data.weapon_data.craft_index != NO_ENT_INDEX)
			console_print("craft not found\n");
		*/	
	
		entity->weapon_data.shield_flare = event->ent_data.weapon_data.shield_flare;
	
		entity->weapon_data.shield_red = event->ent_data.weapon_data.shield_red;
		entity->weapon_data.shield_green = event->ent_data.weapon_data.shield_green;
		entity->weapon_data.shield_blue = event->ent_data.weapon_data.shield_blue;
	
		entity->weapon_data.detached = event->ent_data.weapon_data.detached;
	
		entity->weapon_data.skin = event->ent_data.skin;

		switch(entity->weapon_data.type)
		{
		case WEAPON_PLASMA_CANNON:
			entity->weapon_data.surface = skin_get_plasma_cannon_surface(event->ent_data.skin);
			break;
		
		case WEAPON_MINIGUN:
			entity->weapon_data.surface = skin_get_minigun_surface(event->ent_data.skin);
			break;
		
		case WEAPON_ROCKET_LAUNCHER:
			entity->weapon_data.surface = skin_get_rocket_launcher_surface(event->ent_data.skin);
			break;
		}
		
		break;
		
	case ENT_PLASMA:
		entity->plasma_data.in_weapon = event->ent_data.plasma_data.in_weapon;
		entity->plasma_data.weapon_id = event->ent_data.plasma_data.weapon_id;
		
		entity->plasma_data.red = event->ent_data.plasma_data.red;
		entity->plasma_data.green = event->ent_data.plasma_data.green;
		entity->plasma_data.blue = event->ent_data.plasma_data.blue;
		break;
	
	case ENT_BULLET:
		entity->bullet_data.in_weapon = event->ent_data.bullet_data.in_weapon;
		entity->bullet_data.weapon_id = event->ent_data.bullet_data.weapon_id;
		break;
	
	case ENT_ROCKET:
		entity->rocket_data.theta = event->ent_data.rocket_data.theta;
		entity->rocket_data.start_tick = event->ent_data.rocket_data.start_tick;
		entity->rocket_data.in_weapon = event->ent_data.rocket_data.in_weapon;
		entity->rocket_data.weapon_id = event->ent_data.rocket_data.weapon_id;
	
		entity->rocket_data.magic_smoke = event->ent_data.rocket_data.magic_smoke;

		entity->rocket_data.smoke_start_red = event->ent_data.rocket_data.smoke_start_red;
		entity->rocket_data.smoke_start_green = event->ent_data.rocket_data.smoke_start_green;
		entity->rocket_data.smoke_start_blue = event->ent_data.rocket_data.smoke_start_blue;
	
		entity->rocket_data.smoke_end_red = event->ent_data.rocket_data.smoke_end_red;
		entity->rocket_data.smoke_end_green = event->ent_data.rocket_data.smoke_end_green;
		entity->rocket_data.smoke_end_blue = event->ent_data.rocket_data.smoke_end_blue;
		entity->rocket_data.sample = start_entity_sample(rocket_sample, entity->index, event->tick);
		break;
	
	case ENT_MINE:
		entity->mine_data.under_craft = event->ent_data.mine_data.under_craft;
		entity->mine_data.craft_id = event->ent_data.mine_data.craft_id;
		break;
	}
}


void add_update_ent_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
	event->ent_data.type = message_reader_read_uint8();
	event->ent_data.xdis = message_reader_read_float();
	event->ent_data.ydis = message_reader_read_float();
	event->ent_data.xvel = message_reader_read_float();
	event->ent_data.yvel = message_reader_read_float();
	event->ent_data.teleporting = message_reader_read_int();
	event->ent_data.teleporting_tick = message_reader_read_uint32();
	event->ent_data.teleport_spawn_index = message_reader_read_uint32();
	
	switch(event->ent_data.type)
	{
	case ENT_CRAFT:	
		read_craft_data(&event->ent_data.craft_data);
		break;
	
	case ENT_WEAPON:
		read_weapon_data(&event->ent_data.weapon_data);
		break;
	
	case ENT_PLASMA:
		read_plasma_data(&event->ent_data.plasma_data);
		break;
	
	case ENT_ROCKET:
		read_rocket_data(&event->ent_data.rocket_data);
		break;
	
	case ENT_MINE:
		read_mine_data(&event->ent_data.mine_data);
		break;
	}
}


void process_update_ent_event(struct event_t *event)
{
	struct entity_t *entity = get_entity(centity0, event->ent_data.index);
		
	if(!entity)
		return;		// due to ooo
	
	int old_teleporting = entity->teleporting;
	float teleporter_x, teleporter_y;

	if(!entity)
	{
		printf("consistency error in process_update_ent_event\n");
		return;		// possibly due to ooo
	}
	
	entity->xdis = event->ent_data.xdis;
	entity->ydis = event->ent_data.ydis;
	entity->xvel = event->ent_data.xvel;
	entity->yvel = event->ent_data.yvel;
	entity->teleporting = event->ent_data.teleporting;
	entity->teleporting_tick = event->ent_data.teleporting_tick;
	entity->teleport_spawn_index = event->ent_data.teleport_spawn_index;
	
	switch(entity->type)
	{
	case ENT_CRAFT:
		entity->craft_data.acc = event->ent_data.craft_data.acc;
		entity->craft_data.old_theta = entity->craft_data.theta;
		entity->craft_data.theta = event->ent_data.craft_data.theta;
		entity->craft_data.braking = event->ent_data.craft_data.braking;
		entity->craft_data.rolling_left = event->ent_data.craft_data.rolling_left;
		entity->craft_data.rolling_right = event->ent_data.craft_data.rolling_right;
		entity->craft_data.left_weapon = get_entity(centity0, 
			event->ent_data.craft_data.left_weapon_index);
		entity->craft_data.right_weapon = get_entity(centity0, 
			event->ent_data.craft_data.right_weapon_index);
		entity->craft_data.shield_flare = event->ent_data.craft_data.shield_flare;
		break;
	
	case ENT_WEAPON:
		entity->weapon_data.craft = get_entity(centity0, 
			event->ent_data.weapon_data.craft_index);
		entity->weapon_data.shield_flare = event->ent_data.weapon_data.shield_flare;
		break;
	
	case ENT_PLASMA:
		break;
	
	case ENT_ROCKET:
		break;
	
	case ENT_MINE:
		break;
	}
	
	if(entity->type == ENT_CRAFT &&
		old_teleporting == TELEPORTING_FINISHED &&
		entity->teleporting == TELEPORTING_DISAPPEARING &&
		((game_state == GAMESTATE_DEMO && entity->index == demo_follow_me) ||
		(game_state == GAMESTATE_PLAYING && entity->index == cgame_state->follow_me)))
	{
		get_spawn_point_coords(entity->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
		start_moving_view(viewx, viewy, teleporter_x, teleporter_y);	// violates gamestate scheme
		
		teleporting_start_x = teleporter_x;
		teleporting_start_y = teleporter_y;
	}
}


void add_kill_ent_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
}


void process_kill_ent_event(struct event_t *event)
{
	struct entity_t *entity = get_entity(centity0, event->ent_data.index);

	if(!entity)
		return;		// due to ooo
	
	switch(entity->type)
	{
	case ENT_CRAFT:
		strip_weapons_from_craft(entity);
		break;
	
	case ENT_WEAPON:
		strip_craft_from_weapon(entity);
		break;
	}
	
	remove_entity(&centity0, entity);
}


void add_follow_me_event(struct event_t *event)
{
	event->follow_me_data.index = message_reader_read_uint32();
}


void process_follow_me_event(struct event_t *event)
{
	struct entity_t *entity = get_entity(centity0, event->follow_me_data.index);
	
	if(!entity)
		return;		// due to ooo

	if(game_state == GAMESTATE_DEMO)
		demo_follow_me = event->follow_me_data.index;	
	else
		cgame_state->follow_me = event->follow_me_data.index;
	start_moving_view(viewx, viewy, entity->xdis, entity->ydis);	// violates gamestate scheme
		// should possibly not call this second time around after ooo
}


void add_carcass_event(struct event_t *event)
{
	event->carcass_data.index = message_reader_read_uint32();
}


void process_carcass_event(struct event_t *event)
{
	struct entity_t *craft = get_entity(centity0, event->carcass_data.index);

	if(!craft)
		return;		// due to ooo

	craft->craft_data.carcass = 1;
	craft->craft_data.braking = 0;
	craft->craft_data.particle = 0.0;
	
	strip_weapons_from_craft(craft);
}


void add_railtrail_event(struct event_t *event)
{
	event->railtrail_data.x1 = message_reader_read_float();
	event->railtrail_data.y1 = message_reader_read_float();
	event->railtrail_data.x2 = message_reader_read_float();
	event->railtrail_data.y2 = message_reader_read_float();

	event->railtrail_data.inner_red = message_reader_read_uint8();
	event->railtrail_data.inner_green = message_reader_read_uint8();
	event->railtrail_data.inner_blue = message_reader_read_uint8();
	event->railtrail_data.outer_red = message_reader_read_uint8();
	event->railtrail_data.outer_green = message_reader_read_uint8();
	event->railtrail_data.outer_blue = message_reader_read_uint8();
}


void process_railtrail_event(struct event_t *event)
{
	struct rail_trail_t rail_trail;
		
	rail_trail.start_time = cgame_time;
	rail_trail.x1 = event->railtrail_data.x1;
	rail_trail.y1 = event->railtrail_data.y1;
	rail_trail.x2 = event->railtrail_data.x2;
	rail_trail.y2 = event->railtrail_data.y2;
	
	rail_trail.inner_red = event->railtrail_data.inner_red;
	rail_trail.inner_green = event->railtrail_data.inner_green;
	rail_trail.inner_blue = event->railtrail_data.inner_blue;
	rail_trail.outer_red = event->railtrail_data.outer_red;
	rail_trail.outer_green = event->railtrail_data.outer_green;
	rail_trail.outer_blue = event->railtrail_data.outer_blue;
	
	LL_ADD(struct rail_trail_t, &rail_trail0, &rail_trail);
	start_static_sample(railgun_sample, rail_trail.x1, rail_trail.y1, event->tick);
}


void add_detach_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
}


void process_detach_event(struct event_t *event)
{
	struct entity_t *weapon = get_entity(centity0, event->ent_data.index);

	if(!weapon)
		return;

	weapon->weapon_data.detached = 1;
	
	if(!weapon->weapon_data.craft)
		return;
	
	weapon->xvel = weapon->weapon_data.craft->xvel;
	weapon->yvel = weapon->weapon_data.craft->yvel;
	
	if(weapon->weapon_data.craft->craft_data.left_weapon == weapon)
		weapon->weapon_data.craft->craft_data.left_weapon = NULL;
	else
		weapon->weapon_data.craft->craft_data.right_weapon = NULL;
	
	weapon->weapon_data.craft = NULL;
}


void add_explosion_event(struct event_t *event)
{
	event->explosion_data.xdis = message_reader_read_float();
	event->explosion_data.ydis = message_reader_read_float();
	event->explosion_data.xvel = message_reader_read_float();
	event->explosion_data.yvel = message_reader_read_float();
	event->explosion_data.size = message_reader_read_float();
	
	event->explosion_data.magic = message_reader_read_uint8();
	
	event->explosion_data.start_red = message_reader_read_uint8();
	event->explosion_data.start_green = message_reader_read_uint8();
	event->explosion_data.start_blue = message_reader_read_uint8();
	
	event->explosion_data.end_red = message_reader_read_uint8();
	event->explosion_data.end_green = message_reader_read_uint8();
	event->explosion_data.end_blue = message_reader_read_uint8();
}


void process_explosion_event(struct event_t *event)
{
	explosion(event->explosion_data.xdis, event->explosion_data.ydis, 
		event->explosion_data.xvel, event->explosion_data.yvel, 
		event->explosion_data.size, event->explosion_data.magic, 
		event->explosion_data.start_red, event->explosion_data.start_green, 
		event->explosion_data.start_blue, event->explosion_data.end_red, 
		event->explosion_data.end_green, event->explosion_data.end_blue);

	start_static_sample(explosion_sample, 
		event->explosion_data.xdis, event->explosion_data.ydis, 
		event->tick);
}


void add_teleport_event(struct event_t *event)
{
	if(!old_proto)
		event->ent_data.index = message_reader_read_uint32();
}


void process_teleport_event(struct event_t *event)
{
	if(!old_proto)
		start_entity_sample(teleporter_sample, 
			event->ent_data.index, event->tick);
	else
		start_global_sample(teleporter_sample, event->tick);
}


void add_speedup_event(struct event_t *event)
{
	if(!old_proto)
	{
		event->static_sound_data.x = message_reader_read_float();
		event->static_sound_data.y = message_reader_read_float();
	}
}


void process_speedup_event(struct event_t *event)
{
	if(!old_proto)
		start_static_sample(speedup_ramp_sample, 
			event->static_sound_data.x, event->static_sound_data.y, 
			event->tick);
	else
		start_global_sample(speedup_ramp_sample, event->tick);
}


void add_colours_event(struct event_t *event)
{
	uint8_t type;
	
	event->colours_data.index = message_reader_read_uint32();
	
	type = message_reader_read_uint8();

	switch(type)
	{
	case ENT_CRAFT:
		event->colours_data.magic_smoke = message_reader_read_uint8();
	
		event->colours_data.smoke_start_red = message_reader_read_uint8();
		event->colours_data.smoke_start_green = message_reader_read_uint8();
		event->colours_data.smoke_start_blue = message_reader_read_uint8();
	
		event->colours_data.smoke_end_red = message_reader_read_uint8();
		event->colours_data.smoke_end_green = message_reader_read_uint8();
		event->colours_data.smoke_end_blue = message_reader_read_uint8();
	
		event->colours_data.shield_red = message_reader_read_uint8();
		event->colours_data.shield_green = message_reader_read_uint8();
		event->colours_data.shield_blue = message_reader_read_uint8();
	
		event->colours_data.plasma_red = message_reader_read_uint8();
		event->colours_data.plasma_green = message_reader_read_uint8();
		event->colours_data.plasma_blue = message_reader_read_uint8();
		break;
	
	case ENT_WEAPON:
		event->colours_data.shield_red = message_reader_read_uint8();
		event->colours_data.shield_green = message_reader_read_uint8();
		event->colours_data.shield_blue = message_reader_read_uint8();
		break;
	}
}


void process_colours_event(struct event_t *event)
{
	struct entity_t *entity = get_entity(centity0, event->colours_data.index);

	if(!entity)
		return;

	switch(entity->type)
	{
	case ENT_CRAFT:
		entity->craft_data.magic_smoke = event->colours_data.magic_smoke;
	
		entity->craft_data.smoke_start_red = event->colours_data.smoke_start_red;
		entity->craft_data.smoke_start_green = event->colours_data.smoke_start_green;
		entity->craft_data.smoke_start_blue = event->colours_data.smoke_start_blue;
	
		entity->craft_data.smoke_end_red = event->colours_data.smoke_end_red;
		entity->craft_data.smoke_end_green = event->colours_data.smoke_end_green;
		entity->craft_data.smoke_end_blue = event->colours_data.smoke_end_blue;
	
		entity->craft_data.shield_red = event->colours_data.shield_red;
		entity->craft_data.shield_green = event->colours_data.shield_green;
		entity->craft_data.shield_blue = event->colours_data.shield_blue;
	
		entity->craft_data.plasma_red = event->colours_data.plasma_red;
		entity->craft_data.plasma_green = event->colours_data.plasma_green;
		entity->craft_data.plasma_blue = event->colours_data.plasma_blue;
		break;
	
	case ENT_WEAPON:
		entity->weapon_data.shield_red = event->colours_data.shield_red;
		entity->weapon_data.shield_green = event->colours_data.shield_green;
		entity->weapon_data.shield_blue = event->colours_data.shield_blue;
		break;
	}
}


void add_weapon_start_firing_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
}


void process_weapon_start_firing_event(struct event_t *event)
{
	struct entity_t *entity = get_entity(centity0, event->ent_data.index);

	if(!entity)
		return;
	
	entity->weapon_data.firing = 1;
	entity->weapon_data.firing_start = cgame_tick + 1;
	entity->weapon_data.fired = 0;
}


void add_weapon_stop_firing_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
}


void process_weapon_stop_firing_event(struct event_t *event)
{
	struct entity_t *entity = get_entity(centity0, event->ent_data.index);

	if(!entity)
		return;

	entity->weapon_data.firing = 0;
}


void add_shield_strengths_event(struct event_t *event)
{
	event->shield_strengths_data.craft_shield = message_reader_read_float();
	event->shield_strengths_data.left_shield = message_reader_read_float();
	event->shield_strengths_data.right_shield = message_reader_read_float();
}


void process_shield_strengths_event(struct event_t *event)
{
	if(game_state != GAMESTATE_DEMO)
	{
		cgame_state->craft_shield = event->shield_strengths_data.craft_shield;
		cgame_state->left_shield = event->shield_strengths_data.left_shield;
		cgame_state->right_shield = event->shield_strengths_data.right_shield;
	}
}


void add_ammo_levels_event(struct event_t *event)
{
	event->ammo_levels_data.rails = message_reader_read_int();
	event->ammo_levels_data.mines = message_reader_read_int();
	event->ammo_levels_data.left = message_reader_read_int();
	event->ammo_levels_data.right = message_reader_read_int();
}


void process_ammo_levels_event(struct event_t *event)
{
	if(game_state != GAMESTATE_DEMO)
	{
		cgame_state->rails = event->ammo_levels_data.rails;
		cgame_state->mines = event->ammo_levels_data.mines;
		cgame_state->left_ammo = event->ammo_levels_data.left;
		cgame_state->right_ammo = event->ammo_levels_data.right;
	}
}


void process_tick_events(uint32_t tick)
{
	if(!event0)
		return;
	
	struct event_t *event = event0;
	
	while(event)
	{
		if(event->tick == tick && !event->ooo)
		{
			switch(event->type)
			{
			case EMEVENT_SPAWN_ENT:
			//	console_print("process_spawn_ent_event; tick: %u; index: %u\n", tick, event->index);
				process_spawn_ent_event(event);
				break;
			
			case EMEVENT_UPDATE_ENT:
				process_update_ent_event(event);
				break;
			
			case EMEVENT_KILL_ENT:
				process_kill_ent_event(event);
				break;
			
			case EMEVENT_FOLLOW_ME:
			//	console_print("process_follow_me_event; tick: %u; index: %u\n", tick, event->index);
				process_follow_me_event(event);
				break;
			
			case EMEVENT_CARCASS:
				process_carcass_event(event);
				break;
			
			case EMEVENT_RAILTRAIL:
				process_railtrail_event(event);
				break;
			
			case EMEVENT_DETACH:
				process_detach_event(event);
				break;
			
			case EMEVENT_TELEPORT:
				process_teleport_event(event);
				break;
			
			case EMEVENT_SPEEDUP:
				process_speedup_event(event);
				break;
			
			case EMEVENT_EXPLOSION:
				process_explosion_event(event);
				break;
			
			case EMEVENT_COLOURS:
				process_colours_event(event);
				break;
			
			case EMEVENT_WEAPON_START_FIRING:
				process_weapon_start_firing_event(event);
				break;
				
			case EMEVENT_WEAPON_STOP_FIRING:
				process_weapon_stop_firing_event(event);
				break;
				
			case EMEVENT_SHIELD_STRENGTHS:
				process_shield_strengths_event(event);
				break;
				
			case EMEVENT_AMMO_LEVELS:
				process_ammo_levels_event(event);
				break;
			}
			
			struct event_t *next = event->next;
			LL_REMOVE(struct event_t, &event0, event);
			event = next;
			continue;
		}
		
		event = event->next;
	}
}


int process_tick_events_do_not_remove(uint32_t tick)
{
	if(!event0)
		return 0;
	
	struct event_t *event = event0;
	int any = 0;
	
	while(event)
	{
		if(event->tick == tick)
		{
			switch(event->type)
			{
			case EMEVENT_SPAWN_ENT:
				process_spawn_ent_event(event);
				any = 1;
				break;
			
			case EMEVENT_UPDATE_ENT:
				process_update_ent_event(event);
				any = 1;
				break;
			
			case EMEVENT_KILL_ENT:
				process_kill_ent_event(event);
				any = 1;
				break;

			case EMEVENT_FOLLOW_ME:
				process_follow_me_event(event);
				any = 1;
				break;
			
			case EMEVENT_CARCASS:
				process_carcass_event(event);
				any = 1;
				break;

			case EMEVENT_RAILTRAIL:
				process_railtrail_event(event);
				break;

			case EMEVENT_DETACH:
				process_detach_event(event);
				any = 1;
				break;
			
			case EMEVENT_TELEPORT:
				process_teleport_event(event);
				break;
			
			case EMEVENT_SPEEDUP:
				process_speedup_event(event);
				break;
			
			case EMEVENT_EXPLOSION:
				process_explosion_event(event);	// ?
				break;
			
			case EMEVENT_COLOURS:
				process_colours_event(event);
				break;
			
			case EMEVENT_WEAPON_START_FIRING:
				process_weapon_start_firing_event(event);
				break;
				
			case EMEVENT_WEAPON_STOP_FIRING:
				process_weapon_stop_firing_event(event);
				break;
				
			case EMEVENT_SHIELD_STRENGTHS:
				process_shield_strengths_event(event);
				break;
				
			case EMEVENT_AMMO_LEVELS:
				process_ammo_levels_event(event);
				break;
			}
			
			event->applied = 1;
		}
		
		event = event->next;
	}
	
	return any;
}


int get_event_by_ooo_index(uint32_t index)
{
	struct event_t *event = event0;
		
	while(event)
	{
		if(event->ooo && event->index == index)
		{
		//	console_print("ooo neutralized\n");
			
			event->ooo = 0;
			return 1;
		}
		
		event = event->next;
	}
	
	return 0;
}


void insert_event_in_order(struct event_t *event)
{
	struct event_t *cevent, *temp;


	// if event0 is NULL, then create new event here

	if(!event0)
	{
		event0 = malloc(sizeof(struct event_t));
		memcpy(event0, event, sizeof(struct event_t));
		event0->next = NULL;
		return;
	}

	
	if(event0->index > event->index)
	{
		temp = event0;
		event0 = malloc(sizeof(struct event_t));
		memcpy(event0, event, sizeof(struct event_t));
		event0->next = temp;
		return;
	}


	// search through the rest of the list
	// to find the event before the first
	// event to have a index greater
	// than event->index
	
	cevent = event0;
	
	while(cevent->next)
	{
		if(cevent->next->index > event->index)
		{
			temp = cevent->next;
			cevent->next = malloc(sizeof(struct event_t));
			cevent = cevent->next;
			memcpy(cevent, event, sizeof(struct event_t));
			cevent->next = temp;
			return;
		}

		cevent = cevent->next;
	}


	// we have reached the end of the list and not found
	// an event that has a index greater than or equal to event->index

	cevent->next = malloc(sizeof(struct event_t));
	cevent = cevent->next;
	memcpy(cevent, event, sizeof(struct event_t));
	cevent->next = NULL;
}


int game_demo_process_event()
{
	if(message_reader.message_type == EMEVENT_PULSE)
		return 1;
	
	struct event_t event;
	
	event.tick = message_reader.event_tick;
	
	event.type = message_reader.message_type;
	event.ooo = 0;
	
	switch(event.type)
	{
	case EMEVENT_SPAWN_ENT:
		add_spawn_ent_event(&event);
		break;
	
	case EMEVENT_UPDATE_ENT:
		add_update_ent_event(&event);
		break;
	
	case EMEVENT_KILL_ENT:
		add_kill_ent_event(&event);
		break;
	
	case EMEVENT_FOLLOW_ME:
		add_follow_me_event(&event);
		break;
	
	case EMEVENT_CARCASS:
		add_carcass_event(&event);
		break;
	
	case EMEVENT_RAILTRAIL:
		add_railtrail_event(&event);
		break;
	
	case EMEVENT_DETACH:
		add_detach_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
		break;
	
	case EMEVENT_TELEPORT:
		add_teleport_event(&event);
		break;
	
	case EMEVENT_SPEEDUP:
		add_speedup_event(&event);
		break;
	
	case EMEVENT_COLOURS:
		add_colours_event(&event);
		break;
			
	case EMEVENT_WEAPON_START_FIRING:
		add_weapon_start_firing_event(&event);
		break;
		
	case EMEVENT_WEAPON_STOP_FIRING:
		add_weapon_stop_firing_event(&event);
		break;
		
	case EMEVENT_SHIELD_STRENGTHS:
		add_shield_strengths_event(&event);
		break;
		
	case EMEVENT_AMMO_LEVELS:
		add_ammo_levels_event(&event);
		break;
	}
	
	LL_ADD_TAIL(struct event_t, &event0, &event);
	
	
	return 1;
}


int game_process_event_timed(uint32_t index, uint64_t *stamp)
{
	struct event_t event;
	int ooon = 0;
	
	event.tick = message_reader.event_tick;
	
	add_game_tick(event.tick, stamp);
	
	if(get_event_by_ooo_index(index))
		ooon = 1;
	
	event.type = message_reader.message_type;
	event.ooo = 0;
	event.index = index;
	
	switch(event.type)
	{
	case EMEVENT_SPAWN_ENT:
		add_spawn_ent_event(&event);
		break;
	
	case EMEVENT_UPDATE_ENT:
		add_update_ent_event(&event);
		break;
	
	case EMEVENT_KILL_ENT:
		add_kill_ent_event(&event);
		break;
	
	case EMEVENT_FOLLOW_ME:
		add_follow_me_event(&event);
		break;
	
	case EMEVENT_CARCASS:
		add_carcass_event(&event);
		break;
	
	case EMEVENT_RAILTRAIL:
		add_railtrail_event(&event);
		break;
	
	case EMEVENT_DETACH:
		add_detach_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
		break;
	
	case EMEVENT_TELEPORT:
		add_teleport_event(&event);
		break;
	
	case EMEVENT_SPEEDUP:
		add_speedup_event(&event);
		break;
	
	case EMEVENT_COLOURS:
		add_colours_event(&event);
		break;
	
	case EMEVENT_WEAPON_START_FIRING:
		add_weapon_start_firing_event(&event);
		break;
	
	case EMEVENT_WEAPON_STOP_FIRING:
		add_weapon_stop_firing_event(&event);
		break;
		
	case EMEVENT_SHIELD_STRENGTHS:
		add_shield_strengths_event(&event);
		break;
		
	case EMEVENT_AMMO_LEVELS:
		add_ammo_levels_event(&event);
		break;
	}
	
	if(!ooon)
		insert_event_in_order(&event);
	
	
	return 1;
}


int game_process_event_untimed(uint32_t index)
{
	struct event_t event;
	int ooon = 0;
	
	event.tick = message_reader.event_tick;
	event.type = message_reader.message_type;
	
	if(get_event_by_ooo_index(index))
		ooon = 1;
	
	event.ooo = 0;
	event.index = index;
	
	switch(event.type)
	{
	case EMEVENT_SPAWN_ENT:
		add_spawn_ent_event(&event);
		break;
	
	case EMEVENT_UPDATE_ENT:
		add_update_ent_event(&event);
		break;
	
	case EMEVENT_KILL_ENT:
		add_kill_ent_event(&event);
		break;
	
	case EMEVENT_FOLLOW_ME:
		add_follow_me_event(&event);
		break;
	
	case EMEVENT_CARCASS:
		add_carcass_event(&event);
		break;
	
	case EMEVENT_RAILTRAIL:
		add_railtrail_event(&event);
		break;
	
	case EMEVENT_DETACH:
		add_detach_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
		break;
	
	case EMEVENT_TELEPORT:
		add_teleport_event(&event);
		break;
	
	case EMEVENT_SPEEDUP:
		add_speedup_event(&event);
		break;
	
	case EMEVENT_COLOURS:
		add_colours_event(&event);
		break;
	
	case EMEVENT_WEAPON_START_FIRING:
		add_weapon_start_firing_event(&event);
		break;
	
	case EMEVENT_WEAPON_STOP_FIRING:
		add_weapon_stop_firing_event(&event);
		break;
		
	case EMEVENT_SHIELD_STRENGTHS:
		add_shield_strengths_event(&event);
		break;
		
	case EMEVENT_AMMO_LEVELS:
		add_ammo_levels_event(&event);
		break;
	}
	
	if(!ooon)
		insert_event_in_order(&event);
	
	
	return 1;
}


int game_process_event_timed_ooo(uint32_t index, uint64_t *stamp)
{
	struct event_t event;
	
	event.tick = message_reader.event_tick;
	add_game_tick(event.tick, stamp);
	event.type = message_reader.message_type;
	event.ooo = 1;
	event.applied = 0;
	event.index = index;
	
	switch(event.type)
	{
	case EMEVENT_SPAWN_ENT:
		add_spawn_ent_event(&event);
		break;
	
	case EMEVENT_UPDATE_ENT:
		add_update_ent_event(&event);
		break;
	
	case EMEVENT_KILL_ENT:
		add_kill_ent_event(&event);
		break;
	
	case EMEVENT_FOLLOW_ME:
		add_follow_me_event(&event);
		break;
	
	case EMEVENT_CARCASS:
		add_carcass_event(&event);
		break;
	
	case EMEVENT_RAILTRAIL:
		add_railtrail_event(&event);
		break;
	
	case EMEVENT_DETACH:
		add_detach_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
		break;
	
	case EMEVENT_TELEPORT:
		add_teleport_event(&event);
		break;
	
	case EMEVENT_SPEEDUP:
		add_speedup_event(&event);
		break;
	
	case EMEVENT_COLOURS:
		add_colours_event(&event);
		break;
	
	case EMEVENT_WEAPON_START_FIRING:
		add_weapon_start_firing_event(&event);
		break;
	
	case EMEVENT_WEAPON_STOP_FIRING:
		add_weapon_stop_firing_event(&event);
		break;
		
	case EMEVENT_SHIELD_STRENGTHS:
		add_shield_strengths_event(&event);
		break;
		
	case EMEVENT_AMMO_LEVELS:
		add_ammo_levels_event(&event);
		break;
	}
	
	insert_event_in_order(&event);
	
	
	return 1;
}


int game_process_event_untimed_ooo(uint32_t index)
{
	struct event_t event;
	
	event.tick = message_reader.event_tick;
	event.type = message_reader.message_type;
	event.ooo = 1;
	event.applied = 0;
	event.index = index;
	
	switch(event.type)
	{
	case EMEVENT_SPAWN_ENT:
		add_spawn_ent_event(&event);
		break;
	
	case EMEVENT_UPDATE_ENT:
		add_update_ent_event(&event);
		break;
	
	case EMEVENT_KILL_ENT:
		add_kill_ent_event(&event);
		break;
	
	case EMEVENT_FOLLOW_ME:
		add_follow_me_event(&event);
		break;
	
	case EMEVENT_CARCASS:
		add_carcass_event(&event);
		break;
	
	case EMEVENT_RAILTRAIL:
		add_railtrail_event(&event);
		break;
	
	case EMEVENT_DETACH:
		add_detach_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
		break;
	
	case EMEVENT_TELEPORT:
		add_teleport_event(&event);
		break;
	
	case EMEVENT_SPEEDUP:
		add_speedup_event(&event);
		break;
	
	case EMEVENT_COLOURS:
		add_colours_event(&event);
		break;
	
	case EMEVENT_WEAPON_START_FIRING:
		add_weapon_start_firing_event(&event);
		break;
	
	case EMEVENT_WEAPON_STOP_FIRING:
		add_weapon_stop_firing_event(&event);
		break;
		
	case EMEVENT_SHIELD_STRENGTHS:
		add_shield_strengths_event(&event);
		break;
		
	case EMEVENT_AMMO_LEVELS:
		add_ammo_levels_event(&event);
		break;
	}
	
	insert_event_in_order(&event);
	
	
	return 1;
}


int game_process_message()
{
	switch(message_reader.message_type)
	{
	case EMMSG_PROTO_VER:
		return game_process_proto_ver();
	
	case EMMSG_LOADMAP:
		return game_process_load_map();
	
	case EMMSG_LOADSKIN:
		return game_process_load_skin();
		
	case EMNETMSG_PRINT:
		return game_process_print();
	
	case EMNETMSG_AUTHENTICATE:
		return game_process_authenticate();
	
	case EMNETMSG_JOINED:
		return game_process_joined();
	
	case EMNETMSG_FAILED:
		return game_process_failed();
	
	case EMMSG_PLAYING:
		return game_process_playing();
	
	case EMMSG_SPECTATING:
		return game_process_spectating();
	
	case EMNETMSG_INRCON:
		break;
	
	case EMNETMSG_NOTINRCON:
		break;
	
	case EMNETMSG_MATCH_BEGUN:
		return game_process_match_begun();
	
	case EMNETMSG_MATCH_OVER:
		return game_process_match_over();
	
	case EMNETMSG_LOBBY:
		return game_process_lobby();
	
	case EMNETMSG_PLAYER_INFO:
		return game_process_player_info();
	
	case EMNETMSG_REMOVE_PLAYER_INFO:
		return game_process_remove_player_info();
	}
	
	return 0;
}


void game_process_stream_timed(uint32_t conn, uint32_t index, 
	uint64_t *stamp, struct buffer_t *stream)
{
	if(conn != game_conn)
		return;
	
	message_reader.stream = stream;
	
	while(message_reader_new_message())
	{
		switch(message_reader.message_type & EMMSGCLASS_MASK)
		{
		case EMMSGCLASS_STND:
		case EMMSGCLASS_NETONLY:
			if(!game_process_message())
				return;
			break;
			
		case EMMSGCLASS_EVENT:
			if(!game_process_event_timed(index, stamp))
				return;
			break;
			
		default:
			return;
		}
	}
}


void game_process_stream_untimed(uint32_t conn, uint32_t index, struct buffer_t *stream)
{
	if(conn != game_conn)
		return;
	
	message_reader.stream = stream;
	
	while(message_reader_new_message())
	{
		switch(message_reader.message_type & EMMSGCLASS_MASK)
		{
		case EMMSGCLASS_STND:
		case EMMSGCLASS_NETONLY:
			if(!game_process_message())
				return;
			break;
			
		case EMMSGCLASS_EVENT:
			if(!game_process_event_untimed(index))
				return;
			break;
			
		default:
			return;
		}
	}
}


void game_process_stream_timed_ooo(uint32_t conn, uint32_t index, 
	uint64_t *stamp, struct buffer_t *stream)
{
	if(conn != game_conn)
		return;
	
	message_reader.stream = stream;
	
	while(message_reader_new_message())
	{
		switch(message_reader.message_type & EMMSGCLASS_MASK)
		{
		case EMMSGCLASS_EVENT:
			if(!game_process_event_timed_ooo(index, stamp))
				return;
			break;
			
		default:
			return;
		}
	}
}


void game_process_stream_untimed_ooo(uint32_t conn, uint32_t index, struct buffer_t *stream)
{
	if(conn != game_conn)
		return;
	
	message_reader.stream = stream;
	
	while(message_reader_new_message())
	{
		switch(message_reader.message_type & EMMSGCLASS_MASK)
		{
		case EMMSGCLASS_EVENT:
			if(!game_process_event_untimed_ooo(index))
				return;
			break;
			
		default:
			return;
		}
	}
}


void game_resolution_change()
{
	struct game_state_t *cgame_state;
	struct entity_t *entity;
	
	set_ri_surface_multiplier((double)vid_width / 1600.0);
	reload_map();
	reload_skins();
	
	
	switch(game_state)
	{
	case GAMESTATE_PLAYING:
		
		// getting entities' new surfaces
		
		cgame_state = game_state0;
			
		while(cgame_state)
		{
			struct entity_t *entity = cgame_state->entity0;
		
			while(entity)
			{
				switch(entity->type)
				{
				case ENT_CRAFT:
					entity->craft_data.surface = skin_get_craft_surface(entity->craft_data.skin);
					break;
				
				case ENT_WEAPON:
					switch(entity->weapon_data.type)
					{
					case WEAPON_PLASMA_CANNON:
						entity->weapon_data.surface = 
							skin_get_plasma_cannon_surface(entity->weapon_data.skin);
						break;
					
					case WEAPON_MINIGUN:
						entity->weapon_data.surface = 
							skin_get_minigun_surface(entity->weapon_data.skin);
						break;
					
					case WEAPON_ROCKET_LAUNCHER:
						entity->weapon_data.surface = 
							skin_get_rocket_launcher_surface(entity->weapon_data.skin);
						break;
					}
		
					break;
				}
				
				entity = entity->next;
			}
			
			cgame_state = cgame_state->next;
		}
	
		
		entity = last_known_game_state.entity0;
	
		while(entity)
		{
			switch(entity->type)
			{
			case ENT_CRAFT:
				entity->craft_data.surface = skin_get_craft_surface(entity->craft_data.skin);
				break;
			
			case ENT_WEAPON:
				switch(entity->weapon_data.type)
				{
				case WEAPON_PLASMA_CANNON:
					entity->weapon_data.surface = 
						skin_get_plasma_cannon_surface(entity->weapon_data.skin);
					break;
				
				case WEAPON_MINIGUN:
					entity->weapon_data.surface = 
						skin_get_minigun_surface(entity->weapon_data.skin);
					break;
				
				case WEAPON_ROCKET_LAUNCHER:
					entity->weapon_data.surface = 
						skin_get_rocket_launcher_surface(entity->weapon_data.skin);
					break;
				}
	
				break;
			}
			
			entity = entity->next;
		}
		
		break;
		
		
	case GAMESTATE_DEMO:
		
		// getting entities' new surfaces
		
		entity = centity0;
	
		while(entity)
		{
			switch(entity->type)
			{
			case ENT_CRAFT:
				entity->craft_data.surface = skin_get_craft_surface(entity->craft_data.skin);
				break;
			
			case ENT_WEAPON:
				switch(entity->weapon_data.type)
				{
				case WEAPON_PLASMA_CANNON:
					entity->weapon_data.surface = 
						skin_get_plasma_cannon_surface(entity->weapon_data.skin);
					break;
				
				case WEAPON_MINIGUN:
					entity->weapon_data.surface = 
						skin_get_minigun_surface(entity->weapon_data.skin);
					break;
				
				case WEAPON_ROCKET_LAUNCHER:
					entity->weapon_data.surface = 
						skin_get_rocket_launcher_surface(entity->weapon_data.skin);
					break;
				}
	
				break;
			}
			
			entity = entity->next;
		}
	}
}


void cf_status(char *c)
{
/*	if(net_state != NETSTATE_CONNECTED)
		console_print("You are not connected.\n");
	else
	{
		net_emit_uint8(game_conn, EMMSG_STATUS);
		net_emit_end_of_stream(game_conn);
	}
*/}


void cf_say(char *c)
{
/*	if(net_state != NETSTATE_CONNECTED)
		console_print("You are not connected.\n");
	else
	{
		net_emit_uint8(game_conn, EMMSG_SAY);
		net_emit_string(game_conn, c);
		net_emit_end_of_stream(game_conn);
	}
*/}


void cf_spectate(char *c)
{
	switch(game_state)
	{
	case GAMESTATE_DEAD:
	case GAMESTATE_DEMO:
	case GAMESTATE_CONNECTING:
		console_print("You are not connected.\n");
		break;
	
	case GAMESTATE_SPECTATING:
		console_print("You are already spectating.\n");
		break;
	
	case GAMESTATE_PLAYING:
		net_emit_uint8(game_conn, EMMSG_SPECTATE);
		net_emit_end_of_stream(game_conn);
		break;
	}	
}


void cf_play(char *c)
{
	switch(game_state)
	{
	case GAMESTATE_DEAD:
	case GAMESTATE_DEMO:
	case GAMESTATE_CONNECTING:
		console_print("You are not connected.\n");
		break;
	
	case GAMESTATE_SPECTATING:
		net_emit_uint8(game_conn, EMMSG_PLAY);
		net_emit_end_of_stream(game_conn);
		break;
	
	case GAMESTATE_PLAYING:
		console_print("You are already playing.\n");
		break;
	}	
}


void write_craft_data_to_demo(struct entity_t *craft)
{
	uint32_t msg = NO_ENT_INDEX;
	
	gzwrite(gzrecording, &craft->craft_data.acc, 4);
	gzwrite(gzrecording, &craft->craft_data.theta, 4);
	gzwrite(gzrecording, &craft->craft_data.braking, 4);
	gzwrite(gzrecording, &craft->craft_data.rolling_left, 4);
	gzwrite(gzrecording, &craft->craft_data.rolling_right, 4);
	
	if(craft->craft_data.left_weapon)
		gzwrite(gzrecording, &craft->craft_data.left_weapon->index, 4);
	else
		gzwrite(gzrecording, &msg, 4);
	
	if(craft->craft_data.right_weapon)
		gzwrite(gzrecording, &craft->craft_data.right_weapon->index, 4);
	else
		gzwrite(gzrecording, &msg, 4);
	
	gzwrite(gzrecording, &craft->craft_data.shield_flare, 4);
}


void write_weapon_data_to_demo(struct entity_t *weapon)
{
	uint32_t msg = NO_ENT_INDEX;
	
	gzwrite(gzrecording, &weapon->weapon_data.type, 4);
	gzwrite(gzrecording, &weapon->weapon_data.theta, 4);
	
	if(weapon->weapon_data.craft)
		gzwrite(gzrecording, &weapon->weapon_data.craft->index, 4);
	else
		gzwrite(gzrecording, &msg, 4);
		
	gzwrite(gzrecording, &weapon->weapon_data.shield_flare, 4);
}


void write_plasma_data_to_demo(struct entity_t *plasma)
{
	gzwrite(gzrecording, &plasma->plasma_data.in_weapon, 1);
	gzwrite(gzrecording, &plasma->plasma_data.weapon_id, 4);
}


void write_bullet_data_to_demo(struct entity_t *bullet)
{
	gzwrite(gzrecording, &bullet->bullet_data.in_weapon, 1);
	gzwrite(gzrecording, &bullet->bullet_data.weapon_id, 4);
}


void write_rocket_data_to_demo(struct entity_t *rocket)
{
	gzwrite(gzrecording, &rocket->rocket_data.theta, 4);
	gzwrite(gzrecording, &rocket->rocket_data.start_tick, 4);
	gzwrite(gzrecording, &rocket->rocket_data.in_weapon, 1);
	gzwrite(gzrecording, &rocket->rocket_data.weapon_id, 4);
}


void write_mine_data_to_demo(struct entity_t *mine)
{
	gzwrite(gzrecording, &mine->mine_data.under_craft, 1);
	gzwrite(gzrecording, &mine->mine_data.craft_id, 4);
}


void write_rails_data_to_demo(struct entity_t *rails)
{
	;
}


void write_shield_data_to_demo(struct entity_t *shield)
{
	;
}


void write_entity_to_demo(struct entity_t *entity)
{
	uint8_t msg = EMEVENT_SPAWN_ENT;
	uint32_t skin = 0;
	
	gzwrite(gzrecording, &msg, 1);
	gzwrite(gzrecording, &last_known_game_state.tick, 4);
	gzwrite(gzrecording, &entity->index, 4);
	gzwrite(gzrecording, &entity->type, 1);
	gzwrite(gzrecording, &skin, 4);		// skin
	gzwrite(gzrecording, &entity->xdis, 4);
	gzwrite(gzrecording, &entity->ydis, 4);
	gzwrite(gzrecording, &entity->xvel, 4);
	gzwrite(gzrecording, &entity->yvel, 4);
	gzwrite(gzrecording, &entity->teleporting, 4);
	gzwrite(gzrecording, &entity->teleporting_tick, 4);
	gzwrite(gzrecording, &entity->teleport_spawn_index, 4);
	
	switch(entity->type)
	{
	case ENT_CRAFT:
		write_craft_data_to_demo(entity);
		gzwrite(gzrecording, &entity->craft_data.magic_smoke, 1);

		gzwrite(gzrecording, &entity->craft_data.smoke_start_red, 1);
		gzwrite(gzrecording, &entity->craft_data.smoke_start_green, 1);
		gzwrite(gzrecording, &entity->craft_data.smoke_start_blue, 1);
	
		gzwrite(gzrecording, &entity->craft_data.smoke_end_red, 1);
		gzwrite(gzrecording, &entity->craft_data.smoke_end_green, 1);
		gzwrite(gzrecording, &entity->craft_data.smoke_end_blue, 1);
	
		gzwrite(gzrecording, &entity->craft_data.shield_red, 1);
		gzwrite(gzrecording, &entity->craft_data.shield_green, 1);
		gzwrite(gzrecording, &entity->craft_data.shield_blue, 1);
	
		gzwrite(gzrecording, &entity->craft_data.plasma_red, 1);
		gzwrite(gzrecording, &entity->craft_data.plasma_green, 1);
		gzwrite(gzrecording, &entity->craft_data.plasma_blue, 1);
	
		gzwrite(gzrecording, &entity->craft_data.carcass, 1);
		break;
	
	case ENT_WEAPON:
		write_weapon_data_to_demo(entity);
		gzwrite(gzrecording, &entity->weapon_data.shield_red, 1);
		gzwrite(gzrecording, &entity->weapon_data.shield_green, 1);
		gzwrite(gzrecording, &entity->weapon_data.shield_blue, 1);
		gzwrite(gzrecording, &entity->weapon_data.detached, 1);
		break;
	
	case ENT_PLASMA:
		write_plasma_data_to_demo(entity);
	
		gzwrite(gzrecording, &entity->plasma_data.red, 1);
		gzwrite(gzrecording, &entity->plasma_data.green, 1);
		gzwrite(gzrecording, &entity->plasma_data.blue, 1);
		break;
	
	case ENT_BULLET:
		write_bullet_data_to_demo(entity);
		break;
	
	case ENT_ROCKET:
		write_rocket_data_to_demo(entity);
		gzwrite(gzrecording, &entity->rocket_data.magic_smoke, 1);
	
		gzwrite(gzrecording, &entity->rocket_data.smoke_start_red, 1);
		gzwrite(gzrecording, &entity->rocket_data.smoke_start_green, 1);
		gzwrite(gzrecording, &entity->rocket_data.smoke_start_blue, 1);
	
		gzwrite(gzrecording, &entity->rocket_data.smoke_end_red, 1);
		gzwrite(gzrecording, &entity->rocket_data.smoke_end_green, 1);
		gzwrite(gzrecording, &entity->rocket_data.smoke_end_blue, 1);
		break;
	
	case ENT_MINE:
		write_mine_data_to_demo(entity);
		break;
	
	case ENT_RAILS:
		write_rails_data_to_demo(entity);
		break;
	
	case ENT_SHIELD:
		write_shield_data_to_demo(entity);
		break;
	}
}


void write_all_entities_to_demo()
{
	struct entity_t *centity = last_known_game_state.entity0;

	while(centity)
	{
		write_entity_to_demo(centity);
		centity = centity->next;
	}
}


void cf_record(char *c)
{
	if(recording)
	{
		recording = 0;
		gzclose(gzrecording);
	}
	
	message_reader.type = MESSAGE_READER_STREAM_WRITE_GZDEMO;
	recording_filename = new_string_text(c);
	gzrecording = gzopen(c, "wb9");
	message_reader.gzdemo = gzrecording;
	
	uint8_t msg;
	uint32_t msg_32;
	
	switch(game_state)
	{
	case GAMESTATE_DEAD:
	case GAMESTATE_DEMO:
	case GAMESTATE_CONNECTING:
		console_print("Recording will begin when you start playing or spectating.\n");
		break;
	
	case GAMESTATE_SPECTATING:
	case GAMESTATE_PLAYING:
		
		msg = EMMSG_PROTO_VER;
		gzwrite(gzrecording, &msg, 1);
		msg = EM_PROTO_VER;
		gzwrite(gzrecording, &msg, 1);
	
		msg = EMMSG_LOADMAP;
		gzwrite(gzrecording, &msg, 1);
	
	
		gzwrite_string(message_reader.gzdemo, map_name);
		gzwrite(message_reader.gzdemo, &map_size, 4);
		gzwrite(message_reader.gzdemo, map_hash, FILEINFO_DIGEST_SIZE);
		
		msg = EMMSG_LOADSKIN;
		gzwrite(gzrecording, &msg, 1);
		
		struct string_t *string = new_string_text("default");
		gzwrite_string(message_reader.gzdemo, string);
		free_string(string);
		
		msg_32 = 0;
		gzwrite(gzrecording, &msg_32, 4);
		
		msg = EMMSG_PLAYING;
		gzwrite(gzrecording, &msg, 1);
		gzwrite(gzrecording, &msg_32, 4);
		
		write_all_entities_to_demo();

		msg = EMEVENT_FOLLOW_ME;
		gzwrite(gzrecording, &msg, 1);
		gzwrite(gzrecording, &last_known_game_state.tick, 4);
		gzwrite(gzrecording, &last_known_game_state.follow_me, 4);
		
		console_print("Recording...\n");
		break;
	}
	
	
	recording = 1;
}


void cf_stop(char *c)
{
	recording = 0;
	gzclose(gzrecording);
	message_reader.type = MESSAGE_READER_STREAM;
}


void cf_demo(char *c)
{
	while(demo0)
	{
		free_string(demo0->filename);
		LL_REMOVE(struct demo_t, &demo0, demo0)
	}
	
	switch(game_state)
	{
	case GAMESTATE_DEAD:
		break;
	
	case GAMESTATE_DEMO:
		gzclose(gzdemo);
		gzdemo = NULL;
		break;
	
	case GAMESTATE_CONNECTING:
	case GAMESTATE_SPECTATING:
	case GAMESTATE_PLAYING:
		em_disconnect(game_conn);
		break;
	}
	
	clear_game();
	
	struct demo_t demo;
	
	char *token = strtok(c, " ");
	demo.filename = new_string_text(token);
	LL_ADD(struct demo_t, &demo0, &demo);
		
	while((token = strtok(NULL, " ")))
	{
		demo.filename = new_string_text(token);
		LL_ADD_TAIL(struct demo_t, &demo0, &demo);
	}		
	
	game_conn = 0;
	gzdemo = gzopen(demo0->filename->text, "rb");
	
	if(!gzdemo)
	{
		char *filename = br_strcat("demos/", demo0->filename->text);
		gzdemo = gzopen(find_resource(filename), "rb");
		free(filename);
		
		if(!gzdemo)
		{
			console_print("File not found.\n");
			game_state = GAMESTATE_DEAD;
			return;
		}
	}
	
	game_state = GAMESTATE_DEMO;
	message_reader.gzdemo = gzdemo;
	message_reader.type = MESSAGE_READER_GZDEMO;
	demo_first_tick = 0;
	cgame_tick = 0;
	cdemo = demo0;
	
//	render_frame();
}


void cf_suicide(char *c)
{
	switch(game_state)
	{
	case GAMESTATE_DEAD:
	case GAMESTATE_DEMO:
	case GAMESTATE_CONNECTING:
	case GAMESTATE_SPECTATING:
		break;
	
	case GAMESTATE_PLAYING:
		net_emit_uint8(game_conn, EMMSG_SUICIDE);
		net_emit_end_of_stream(game_conn);
		break;
	}
}


void roll_left(uint32_t state)
{
//	if(game_state != GAMESTATE_ALIVE)
		return;
/*
	net_write_dword(EMNETMSG_CTRLCNGE);
	net_write_dword(EMCTRL_ROLL);

	if(state)
		net_write_float(-1.0f);
	else
		net_write_float(0.0f);

	finished_writing();
	*/
}


void roll_right(uint32_t state)
{
//	if(game_state != GAMESTATE_ALIVE)
		return;
/*
	net_write_dword(EMNETMSG_CTRLCNGE);
	net_write_dword(EMCTRL_ROLL);

	if(state)
		net_write_float(1.0f);
	else
		net_write_float(0.0f);

	finished_writing();
	*/
}


void explosion(float xdis, float ydis, float xvel, float yvel, 
	float size, uint8_t magic, 
	uint8_t start_red, uint8_t start_green, uint8_t start_blue,
	uint8_t end_red, uint8_t end_green, uint8_t end_blue)
{
	int np, p;
	np = lrint(size / 5);
	
	struct particle_t particle;
		
	if(!magic)
	{
		particle.start_red = start_red;
		particle.start_green = start_green;
		particle.start_blue = start_blue;
	
		particle.end_red = end_red;
		particle.end_green = end_green;
		particle.end_blue = end_blue;
	}
	
//	printf("xvel: %f; yvel: %f\n", xvel, yvel);
	
	particle.xpos = xdis;
	particle.ypos = ydis;
	
	for(p = 0; p < np; p++)
	{
		double sin_theta, cos_theta;
		sincos(drand48() * 2 * M_PI, &sin_theta, &cos_theta);
		
		double r = drand48();
		
		particle.xvel = xvel + -sin_theta * size * r;
		particle.yvel = yvel + cos_theta * size * r;
		particle.creation = particle.last = cgame_time;
		create_upper_particle(&particle);
	}
}


void tick_craft(struct entity_t *craft, float old_xdis, float old_ydis)
{
	if(cgame_tick <= craft->craft_data.last_tick)
		return;
	
	if(craft->teleporting)
		return;
	
	craft->craft_data.last_tick = cgame_tick;
	
	int np = 0, p;
	double sin_theta, cos_theta;
	struct particle_t particle;
		
	if(!craft->craft_data.magic_smoke)
	{
		particle.start_red = craft->craft_data.smoke_start_red;
		particle.start_green = craft->craft_data.smoke_start_green;
		particle.start_blue = craft->craft_data.smoke_start_blue;
	
		particle.end_red = craft->craft_data.smoke_end_red;
		particle.end_green = craft->craft_data.smoke_end_green;
		particle.end_blue = craft->craft_data.smoke_end_blue;
	}
	else
	{
		particle.start_red = lrand48();
		particle.start_green = lrand48();
		particle.start_blue = lrand48();
	
		particle.end_red = lrand48();
		particle.end_green = lrand48();
		particle.end_blue = lrand48();
	}

	if(craft->craft_data.carcass)
	{
		double vel = hypot(craft->xvel, craft->yvel);

		craft->craft_data.particle += vel * 0.2;

		while(craft->craft_data.particle >= 1.0)
		{
			craft->craft_data.particle -= 1.0;
			np++;
		}
		
		for(p = 0; p < np; p++)
		{
			sincos(drand48() * 2 * M_PI, &sin_theta, &cos_theta);
			
			double r = drand48();
			
			double nxdis = old_xdis + (craft->xdis - old_xdis) * (double)(p + 1) / (double)np;
			double nydis = old_ydis + (craft->ydis - old_ydis) * (double)(p + 1) / (double)np;
			

			switch(game_state)
			{
			case GAMESTATE_PLAYING:
				particle.creation = particle.last = get_time_from_game_tick((double)cgame_tick + 
					(double)(p + 1) / (double)np - 1.0);
				break;
			
			case GAMESTATE_DEMO:
				particle.creation = particle.last = ((double)cgame_tick + 
					(double)(p + 1) / (double)np - 1.0) / 200.0;
				break;
			}
			
			particle.xpos = nxdis;
			particle.ypos = nydis;
			
			particle.xvel = craft->xvel - sin_theta * 500.0 * r;
			particle.yvel = craft->yvel + cos_theta * 500.0 * r;
				
			create_upper_particle(&particle);
			
		}
	}
	else
	{
		craft->craft_data.particle += craft->craft_data.acc * 200;
		
		
		
		while(craft->craft_data.particle >= 1.0)
		{
			craft->craft_data.particle -= 1.0;
			np++;
		}
	
		for(p = 0; p < np; p++)
		{
			
			double r = drand48();
			
			double nxdis = old_xdis + (craft->xdis - old_xdis) * (double)(p + 1) / (double)np;
			double nydis = old_ydis + (craft->ydis - old_ydis) * (double)(p + 1) / (double)np;
			
			
	
			switch(game_state)
			{
			case GAMESTATE_PLAYING:
				particle.creation = particle.last = get_time_from_game_tick((double)cgame_tick + 
					(double)(p + 1) / (double)np - 1.0);
				break;
			
			case GAMESTATE_DEMO:
				particle.creation = particle.last = ((double)cgame_tick + 
					(double)(p + 1) / (double)np - 1.0) / 200.0;
				break;
			}
			
			
			double delta_theta = craft->craft_data.theta - craft->craft_data.old_theta;
			
			if(delta_theta > M_PI)
				delta_theta -= 2 * M_PI;
			else if(delta_theta < -M_PI)
				delta_theta += 2 * M_PI;
			
			double theta = craft->craft_data.old_theta + 
				(delta_theta) * (double)(p + 1) / (double)np;
			
		
			sincos(theta, &sin_theta, &cos_theta);
			
			particle.xpos = nxdis + sin_theta * 25.52;
			particle.ypos = nydis - cos_theta * 25.52;
	
			sincos(theta + M_PI / 2, &sin_theta, &cos_theta);
			
			particle.xpos -= sin_theta * 13;
			particle.ypos += cos_theta * 13;
			
			sincos(theta + (drand48() - 0.5) * 0.1, &sin_theta, &cos_theta);
			particle.xvel = craft->xvel + craft->craft_data.acc * sin_theta * 100000 * r;
			particle.yvel = craft->yvel + -craft->craft_data.acc * cos_theta * 100000 * r;
				
			create_lower_particle(&particle);
	
			particle.xpos = nxdis + sin_theta * 25.52;
			particle.ypos = nydis - cos_theta * 25.52;
	
			sincos(theta + M_PI / 2, &sin_theta, &cos_theta);
			
			particle.xpos += sin_theta * 13;
			particle.ypos -= cos_theta * 13;
			
			sincos(theta + (drand48() - 0.5) * 0.1, &sin_theta, &cos_theta);
			particle.xvel = craft->xvel + craft->craft_data.acc * sin_theta * 100000 * r;
			particle.yvel = craft->yvel + -craft->craft_data.acc * cos_theta * 100000 * r;
				
			create_lower_particle(&particle);
		}
	}
}


void tick_rocket(struct entity_t *rocket, float xdis, float ydis)
{
	if(cgame_tick <= rocket->rocket_data.last_tick)
		return;
	
	if(rocket->teleporting)
		return;
	
	rocket->rocket_data.last_tick = cgame_tick;
	
	rocket->rocket_data.particle += 15.0;
	
	struct particle_t particle;
		
	if(!rocket->rocket_data.magic_smoke)
	{
		particle.start_red = rocket->rocket_data.smoke_start_red;
		particle.start_green = rocket->rocket_data.smoke_start_green;
		particle.start_blue = rocket->rocket_data.smoke_start_blue;
	
		particle.end_red = rocket->rocket_data.smoke_end_red;
		particle.end_green = rocket->rocket_data.smoke_end_green;
		particle.end_blue = rocket->rocket_data.smoke_end_blue;
	}
	else
	{
		particle.start_red = lrand48();
		particle.start_green = lrand48();
		particle.start_blue = lrand48();
	
		particle.end_red = lrand48();
		particle.end_green = lrand48();
		particle.end_blue = lrand48();
	}
		
	
	int np = 0, p;
	
	while(rocket->rocket_data.particle >= 1.0)
	{
		rocket->rocket_data.particle -= 1.0;
		np++;
	}
	

	for(p = 0; p < np; p++)	
	{
		double sin_theta, cos_theta;
		sincos(rocket->rocket_data.theta, &sin_theta, &cos_theta);
		
		double r = drand48();
		

		double nxdis = rocket->xdis + (xdis - rocket->xdis) * (double)(p + 1) / (double)np;
		double nydis = rocket->ydis + (ydis - rocket->ydis) * (double)(p + 1) / (double)np;
		
		

		switch(game_state)
		{
		case GAMESTATE_PLAYING:
			particle.creation = particle.last = get_time_from_game_tick((double)cgame_tick + 
				(double)(p + 1) / (double)np - 1.0);
			break;
		
		case GAMESTATE_DEMO:
			particle.creation = particle.last = ((double)cgame_tick + 
				(double)(p + 1) / (double)np - 1.0) / 200.0;
			break;
		}
		
		
		particle.xpos = nxdis;
		particle.ypos = nydis;

		sincos(rocket->rocket_data.theta + (drand48() - 0.5) * 0.1, &sin_theta, &cos_theta);
		particle.xvel = rocket->xvel + sin_theta * 1000 * r;
		particle.yvel = rocket->yvel - cos_theta * 1000 * r;
			
		create_lower_particle(&particle);
	}
}


void render_entities()
{
	struct entity_t *entity = centity0;
	struct blit_params_t params;
	params.dest = s_backbuffer;
	double time;
	int f;

	while(entity)
	{
		uint32_t x, y;
		
		switch(entity->type)
		{
		case ENT_MINE:
			
			params.source = ris_mine->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.dest_x = x - ris_mine->surface->width / 2;
			params.dest_y = y - ris_mine->surface->height / 2;
			
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				
				case TELEPORTING_TRAVELLING:
					break;
					
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				}
			}
			else
				blit_surface(&params);
		
			break;
		}

		entity = entity->next;
	}


	entity = centity0;
		
	while(entity)
	{
		uint32_t x, y;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			
			while(entity->craft_data.theta >= M_PI)
				entity->craft_data.theta -= 2 * M_PI;
			
			while(entity->craft_data.theta < -M_PI)
				entity->craft_data.theta += 2 * M_PI;
		
			params.source = entity->craft_data.surface;
		
			params.source_x = 0;
			
			f = lrint((entity->craft_data.theta / (2 * M_PI) + 0.5) * ROTATIONS);
			
			assert(f >= 0);
			
			params.source_y = (f % ROTATIONS) * entity->craft_data.surface->width;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.dest_x = x - entity->craft_data.surface->width / 2;
			params.dest_y = y - entity->craft_data.surface->width / 2;
		
			params.width = entity->craft_data.surface->width;
			params.height = entity->craft_data.surface->width;
		
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					break;
				
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					break;
				}
				
				alpha_blit_partial_surface(&params);
			}
			else
				blit_partial_surface(&params);
		
		
			if(!entity->teleporting && !entity->craft_data.carcass)
			{				
				params.source = ris_craft_shield->surface;
			
				params.dest_x = x - ris_craft_shield->surface->width / 2;
				params.dest_y = y - ris_craft_shield->surface->height / 2;

				params.red = entity->craft_data.shield_red;
				params.green = entity->craft_data.shield_green;
				params.blue = entity->craft_data.shield_blue;
				params.alpha = lrint(entity->craft_data.shield_flare * 255.0);
			
				alpha_blit_surface(&params);
			}
		
			break;
		
			
		case ENT_WEAPON:
			params.source = entity->weapon_data.surface;
		
			while(entity->weapon_data.theta >= M_PI)
				entity->weapon_data.theta -= 2 * M_PI;
			
			while(entity->weapon_data.theta < -M_PI)
				entity->weapon_data.theta += 2 * M_PI;
		
			params.source = entity->weapon_data.surface;
		
			params.source_x = 0;
			
			f = lrint((entity->weapon_data.theta / (2 * M_PI) + 0.5) * ROTATIONS);
			
			assert(f >= 0);
			
			params.source_y = (f % ROTATIONS) * entity->weapon_data.surface->width;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.dest_x = x - entity->weapon_data.surface->width / 2;
			params.dest_y = y - entity->weapon_data.surface->width / 2;
		
			params.width = entity->weapon_data.surface->width;
			params.height = entity->weapon_data.surface->width;
		
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					break;
				
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					break;
				}
				
				alpha_blit_partial_surface(&params);
			}
			else
				blit_partial_surface(&params);
		
			if(!entity->teleporting)
			{				
				params.source = ris_weapon_shield->surface;
			
				params.dest_x = x - ris_weapon_shield->surface->width / 2;
				params.dest_y = y - ris_weapon_shield->surface->height / 2;
				
				params.red = entity->weapon_data.shield_red;
				params.green = entity->weapon_data.shield_green;
				params.blue = entity->weapon_data.shield_blue;
				params.alpha = lrint(entity->weapon_data.shield_flare * 255.0);
			
				alpha_blit_surface(&params);
			}
		
			break;
		
			
		case ENT_PLASMA:

			params.source = ris_plasma->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.red = entity->plasma_data.red;
			params.green = entity->plasma_data.green;
			params.blue = entity->plasma_data.blue;
			
			params.dest_x = x - ris_plasma->surface->width / 2;
			params.dest_y = y - ris_plasma->surface->width / 2;
			
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					break;
				
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					break;
				}
				
				alpha_blit_surface(&params);
			}
			else
				blit_surface(&params);
		
			break;
		
			
		case ENT_BULLET:
			world_to_screen(entity->bullet_data.old_xdis, entity->bullet_data.old_ydis, 
				&params.x1, &params.y1);
			world_to_screen(entity->xdis, entity->ydis, 
				&params.x2, &params.y2);
		
			params.red = 0xff;
			params.green = 0xff;
			params.blue = 0xff;
			
			draw_line(&params);
		
			entity->bullet_data.old_xdis = entity->xdis;
			entity->bullet_data.old_ydis = entity->ydis;
		
			break;
		
		
		case ENT_ROCKET:
			
			params.source = ris_plasma->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			if(entity->rocket_data.magic_smoke)
			{
				params.red = lrand48();
				params.green = lrand48();
				params.blue = lrand48();
			}
			else
			{
				params.red = entity->rocket_data.smoke_end_red;
				params.green = entity->rocket_data.smoke_end_green;
				params.blue = entity->rocket_data.smoke_end_blue;
			}
			
			params.dest_x = x - ris_plasma->surface->width / 2;
			params.dest_y = y - ris_plasma->surface->height / 2;
			
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				
				case TELEPORTING_TRAVELLING:
					break;
					
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				}
			}
			else
				blit_surface(&params);
		
			break;
		
			
		case ENT_RAILS:
			
			params.source = ris_plasma->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.red = 0x32;
			params.green = 0xa9;
			params.blue = 0xf0;
			
			params.dest_x = x - ris_plasma->surface->width / 2;
			params.dest_y = y - ris_plasma->surface->height / 2;
			
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				
				case TELEPORTING_TRAVELLING:
					break;
					
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				}
			}
			else
				blit_surface(&params);
		
			break;
		
			
		case ENT_SHIELD:
			
			params.source = ris_shield_pickup->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.red = 0xff;
			params.green = 0xff;
			params.blue = 0xff;
			
			params.dest_x = x - ris_shield_pickup->surface->width / 2;
			params.dest_y = y - ris_shield_pickup->surface->height / 2;
			
			if(entity->teleporting)
			{
				time = (double)(cgame_tick - entity->teleporting_tick) / 200.0;
				
				switch(entity->teleporting)
				{
				case TELEPORTING_DISAPPEARING:
					params.alpha = 255 - min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				
				case TELEPORTING_TRAVELLING:
					break;
					
				case TELEPORTING_APPEARING:
					params.alpha = min(lround(time / TELEPORT_FADE_TIME * 255.0), 255);
					alpha_blit_surface(&params);
					break;
				}
			}
			else
				blit_surface(&params);
		
			break;
		}

		entity = entity->next;
	}
}


void duplicate_game_state(struct game_state_t *old_game_state, struct game_state_t *new_game_state)
{
	new_game_state->tick = old_game_state->tick;
	new_game_state->tainted = old_game_state->tainted;
	new_game_state->entity0 = NULL;
	new_game_state->follow_me = old_game_state->follow_me;
	new_game_state->next = NULL;
	
	
	// duplicate entities, maintaining order
	
	struct entity_t *centity = old_game_state->entity0, 
		**tail_entity = &new_game_state->entity0;
		
	while(centity)
	{
		LL_ADD(struct entity_t, tail_entity, centity);
		tail_entity = &(*tail_entity)->next;
		
		centity = centity->next;
	}
	
	
	// fix weapons
	
	centity = new_game_state->entity0;
		
	while(centity)
	{
		switch(centity->type)
		{
		case ENT_CRAFT:
			
			if(centity->craft_data.left_weapon)
				centity->craft_data.left_weapon = get_entity(new_game_state->entity0, 
					centity->craft_data.left_weapon->index);
			
			if(centity->craft_data.right_weapon)
				centity->craft_data.right_weapon = get_entity(new_game_state->entity0, 
					centity->craft_data.right_weapon->index);
			break;
		
		case ENT_WEAPON:
			if(centity->weapon_data.craft)
				centity->weapon_data.craft = get_entity(new_game_state->entity0, 
					centity->weapon_data.craft->index);
			break;
		}
		
		centity = centity->next;
	}

	new_game_state->craft_shield = old_game_state->craft_shield;
	new_game_state->left_shield = old_game_state->left_shield;
	new_game_state->right_shield = old_game_state->right_shield;
	new_game_state->rails = old_game_state->rails;
	new_game_state->mines = old_game_state->mines;
	new_game_state->left_ammo = old_game_state->left_ammo;
	new_game_state->right_ammo = old_game_state->right_ammo;
}


void free_game_state_list(struct game_state_t **game_state0)
{
	assert(game_state0);
	
	while(*game_state0)
	{
		LL_REMOVE_ALL(struct entity_t, &(*game_state0)->entity0);
		struct game_state_t *temp = (*game_state0)->next;
		free(*game_state0);
		*game_state0 = temp;
	}
}


void update_game()
{
	update_tick_parameters();
	
	render_tick = get_game_tick();
	
	uint32_t new_io_tick = 0, new_ooo_tick = 0;
	uint32_t first_io_tick = 0, last_io_tick = 0, first_ooo_tick = 0;
	
	struct event_t *cevent = event0;
		
	while(cevent)
	{
		if(new_io_tick || new_ooo_tick)
			cevent->applied = 0;
		
		if(cevent->tick <= render_tick)
		{
			if(cevent->ooo)
			{
				if(!cevent->applied && !new_ooo_tick)
				{
					new_ooo_tick = 1;
					first_ooo_tick = cevent->tick;
				}
			}
			else
			{
				if(!new_io_tick)
				{
					new_io_tick = 1;
					last_io_tick = first_io_tick = cevent->tick;
				}
				else
				{
					last_io_tick = cevent->tick;
				}
			}
		}
		
		cevent = cevent->next;
	}


	if(new_io_tick)
	{
		// absorb game states from the prediction cache as much as possible
		// game_state0 is always the same tick as last_known_game_state,
		// but can be tainted
		
		if(last_known_game_state.tick != first_io_tick 
			&& !game_state0->tainted && game_state0->next)
		{
			if(!game_state0->next->tainted)
			{
				struct game_state_t *game_state = game_state0->next;
					
				while(game_state->tick < first_io_tick && game_state->next)
				{
					if(game_state->next->tainted)
						break;
					
					game_state = game_state->next;
				}
				
				LL_REMOVE_ALL(struct entity_t, &last_known_game_state.entity0);
				duplicate_game_state(game_state, &last_known_game_state);
			}
		}
		
		cgame_state = &last_known_game_state;
		centity0 = last_known_game_state.entity0;
		cgame_tick = last_known_game_state.tick;
		cgame_time = get_time_from_game_tick(cgame_tick);
		process_tick_events(last_known_game_state.tick);
		last_known_game_state.entity0 = centity0;
		
		while(last_known_game_state.tick < last_io_tick)
		{
			centity0 = last_known_game_state.entity0;
			cgame_tick = ++last_known_game_state.tick;
			cgame_time = get_time_from_game_tick(cgame_tick);
			s_tick_entities(&centity0);
			process_tick_events(last_known_game_state.tick);
			last_known_game_state.entity0 = centity0;
		}
			
		free_game_state_list(&game_state0);
		game_state0 = malloc(sizeof(struct game_state_t));
		duplicate_game_state(&last_known_game_state, game_state0);
	}
	else
	{
		if(new_ooo_tick && game_state0->tick == first_ooo_tick && game_state0->tainted)
		{
			free_game_state_list(&game_state0);
			game_state0 = malloc(sizeof(struct game_state_t));
			duplicate_game_state(&last_known_game_state, game_state0);
		}
	}

	struct game_state_t *game_state = game_state0;
		
	if(game_state0->tick != first_ooo_tick)
	{
		while(game_state->tick < render_tick && game_state->next)
		{
			if(new_ooo_tick)
			{
				if(game_state->tick == first_ooo_tick)
					break;
				
				if(game_state->next->tainted && game_state->next->tick == first_ooo_tick)
					break;
			}
			
			game_state = game_state->next;
		}
	}
	
	free_game_state_list(&game_state->next);
	
	cgame_state = game_state;
	centity0 = game_state->entity0;
	cgame_tick = game_state->tick;
	cgame_time = get_time_from_game_tick(cgame_tick);
	
	if(process_tick_events_do_not_remove(cgame_tick))
		game_state->tainted = 1;
	
	game_state->entity0 = centity0;
	
	while(game_state->tick < render_tick)
	{
		game_state->next = malloc(sizeof(struct game_state_t));
		duplicate_game_state(game_state, game_state->next);
		game_state->next->tick = game_state->tick + 1;
		game_state = game_state->next;
		
		cgame_state = game_state;
		centity0 = game_state->entity0;
		cgame_tick = game_state->tick;
		cgame_time = get_time_from_game_tick(cgame_tick);
	
		s_tick_entities(&centity0);
		
		if(process_tick_events_do_not_remove(cgame_tick))
			game_state->tainted = 1;
		
		game_state->entity0 = centity0;
	}
}


void update_demo()
{
	uint32_t tick = 0;
	
	top:
	
	if(demo_first_tick)		// i.e. not the first event
	{
		tick = (uint32_t)lround((get_wall_time() - demo_start_time) * 200.0) + demo_first_tick;
	}
	else
	{
		message_reader_new_message();
	}
	
	
	while(1)
	{
		int stop = 0;
		switch(message_reader.message_type & EMMSGCLASS_MASK)
		{
		case EMMSGCLASS_STND:
			if(!game_process_message())
				return;
			break;
			
		case EMMSGCLASS_EVENT:
			if(!demo_first_tick)
			{
				demo_last_tick = demo_first_tick = 
					tick = message_reader.event_tick;
				
				init_fr();
				demo_start_frame = frame;
				demo_start_time = get_wall_time();
			}
			
			if(message_reader.event_tick > tick)
			{
				stop = 1;
				break;
			}
				
			if(!game_demo_process_event())
			{
				return;
			}
			break;
			
		default:
			return;
		}
		
		if(stop)
			break;
		
		if(!message_reader_new_message())
		{
			float s = get_wall_time() - demo_start_time;
			
			console_print("%u frames in %.2f seconds; %.2f fps\n", frame - demo_start_frame, 
				s, (double)(frame - demo_start_frame) / s);
			
			clear_game();
			game_conn = 0;
			
			if(cdemo->next)
				cdemo = cdemo->next;
			else
				cdemo = demo0;
			
			gzclose(gzdemo);
			gzdemo = gzopen(cdemo->filename->text, "rb");
			
			if(!gzdemo)
			{
				char *filename = br_strcat("demos/", cdemo->filename->text);
				gzdemo = gzopen(find_resource(filename), "rb");
				free(filename);
				
				if(!gzdemo)
				{
					console_print("File not found.\n");
					game_state = GAMESTATE_DEAD;
					return;
				}
			}
			
			game_state = GAMESTATE_DEMO;
			message_reader.gzdemo = gzdemo;
			message_reader.type = MESSAGE_READER_GZDEMO;
			demo_first_tick = 0;
			cgame_tick = 0;
			
			goto top;
		}
	}
	
	
	while(demo_last_tick <= tick)
	{
		cgame_time = (double)cgame_tick / 200.0;
		s_tick_entities(&centity0);
		process_tick_events(demo_last_tick);
		cgame_tick = ++demo_last_tick;		// cgame_tick is presumed to start at 0!!!
	}
}


/*

void update_game()
{
	update_tick_parameters();
	uint32_t tick = get_game_tick();
	uint32_t last_known_tick = min(last_event_tick, tick);
	
	cgame_tick = game_tick;
	centity0 = entity0;
	
	
//	if(event0)
	{
		if(cgame_tick <= last_known_tick)
		{
			while(cgame_tick < last_known_tick)
			{
				process_tick_events(cgame_tick);
				cgame_tick++;
				s_tick_entities(&centity0);
			}
			
			process_tick_events(cgame_tick);
	
			game_tick = cgame_tick;
			entity0 = centity0;
		}
		
		pgame_tick = game_tick;
		LL_REMOVE_ALL(struct entity_t, &pentity0);
		pentity0 = duplicate_entities(entity0);
	}
	
	
	cgame_tick = pgame_tick;
	centity0 = pentity0;
	

	process_tick_events_do_not_remove(cgame_tick);	// process remaining ooo streams
	
	while(cgame_tick < tick)
	{
		cgame_tick++;
		s_tick_entities(&centity0);
		process_tick_events_do_not_remove(cgame_tick);
	}
	
	pgame_tick = cgame_tick;
	pentity0 = centity0;
}



*/


void render_particle(float wx, float wy, uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue)
{
	int x, y;
	
	world_to_screen(wx, wy, &x, &y);

	struct blit_params_t params;
	params.red = red;
	params.green = green;
	params.blue = blue;
	params.dest = s_backbuffer;
	
	params.dest_x = x;
	params.dest_y = y;
	
	params.alpha = alpha;
	
	alpha_plot_pixel(&params);
	
	params.alpha >>= 1;
	
	params.dest_x++;
	alpha_plot_pixel(&params);
	
	params.dest_x--;
	params.dest_y++;
	alpha_plot_pixel(&params);
	
	params.dest_y -= 2;
	alpha_plot_pixel(&params);
	
	params.dest_x--;
	params.dest_y++;
	alpha_plot_pixel(&params);
}


void render_rail_trails()
{
	struct rail_trail_t *rail_trail = rail_trail0;
		
	double time = cgame_time;
	
	while(rail_trail)
	{
		double rail_time = time - rail_trail->start_time;
		
		if(rail_time > 2.5)
		{
			struct rail_trail_t *next = rail_trail->next;
			LL_REMOVE(struct rail_trail_t, &rail_trail0, rail_trail);
			rail_trail = next;
			continue;
		}
			
		
		double expand_time = min(rail_time, 0.5);
		
		double r = RAIL_TRAIL_INITIAL_EXPAND_VELOCITY * expand_time + 
			0.5 * RAIL_TRAIL_EXPAND_ACC * expand_time * expand_time;
		
		double deltax = rail_trail->x2 - rail_trail->x1;
		double deltay = rail_trail->y2 - rail_trail->y1;
		
		double length = hypot(deltax, deltay);
		
		deltax /= length;
		deltay /= length;
		
		int p, np = floor(length / 5);
		
		for(p = 0; p < np; p++)
		{
			double t = (double)p / (double)np;
			
			double cx = rail_trail->x1 + (rail_trail->x2 - rail_trail->x1) * t;
			double cy = rail_trail->y1 + (rail_trail->y2 - rail_trail->y1) * t;

			uint8_t alpha;

			if(rail_time < 0.5)
				alpha  = 0xff;
			else
				alpha  = (uint8_t)(255 - floor((rail_time - 0.5) / 2.0 * 255.0f));
			
			render_particle(cx, cy, alpha, rail_trail->inner_red, 
				rail_trail->inner_green, rail_trail->inner_blue);
			
			double theta = length / 30 * M_PI * t;
			
			double nr = cos(theta) * r;
			
			
			
			cx -= deltay * nr;
			cy += deltax * nr;
	
			if(rail_time < 0.5)
				alpha  = 0xff;
			else
				alpha  = (uint8_t)(255 - floor((rail_time - 0.5) / 2.0 * 255.0f));
			render_particle(cx, cy, alpha, rail_trail->outer_red, 
				rail_trail->outer_green, rail_trail->outer_blue);
		}
		
		rail_trail = rail_trail->next;
	}
}


void render_recording()
{
	if(recording)
	{
		blit_text_centered(((vid_width / 3) - (vid_width / 200)) / 2, vid_height / 6, 
			0xff, 0xff, 0xff, s_backbuffer, "recording %s", recording_filename->text);
	}
}


void render_teleporters()
{
	struct teleporter_t *cteleporter = teleporter0;
		
	while(cteleporter)
	{
		cteleporter->particle_power += frame_time * cteleporter->sparkles;
		
		int p, np = 0;
		
		while(cteleporter->particle_power >= 1.0)
		{
			cteleporter->particle_power -= 1.0;
			np++;
		}
	
		
		for(p = 0; p < np; p++)
		{
			double sin_theta, cos_theta;
			sincos(drand48() * 2 * M_PI, &sin_theta, &cos_theta);
			
			
			cteleporter->particles[cteleporter->next_particle].xpos = 0.0;
			cteleporter->particles[cteleporter->next_particle].ypos = 0.0;
			
			double r = drand48();
			
			cteleporter->particles[cteleporter->next_particle].xvel = -sin_theta * 1000.0 * r;
			cteleporter->particles[cteleporter->next_particle].yvel = cos_theta * 1000.0 * r;
			
			switch(game_state)
			{
			case GAMESTATE_PLAYING:
				cteleporter->particles[cteleporter->next_particle].creation = 
					cteleporter->particles[cteleporter->next_particle].last =
					get_time_from_game_tick((double)cgame_tick + 
					(double)(p + 1) / (double)np - 1.0);
				break;
			
			case GAMESTATE_DEMO:
				cteleporter->particles[cteleporter->next_particle].creation = 
					cteleporter->particles[cteleporter->next_particle].last =
					((double)cgame_tick + (double)(p + 1) / (double)np - 1.0) / 200.0;
				break;
			}
			
			++cteleporter->next_particle;
			cteleporter->next_particle %= 1000;
		}
		
		
		for(p = 0; p < 1000; p++)
		{
			double age = cgame_time - cteleporter->particles[p].creation;
			
			if(age <= 1.0f)
			{
				double particle_time = cgame_time - cteleporter->particles[p].last;
				
				double dampening = exp(-12.0f * particle_time);
				
				cteleporter->particles[p].xvel *= dampening;
				cteleporter->particles[p].yvel *= dampening;
	
				cteleporter->particles[p].xpos += cteleporter->particles[p].xvel * particle_time;
				cteleporter->particles[p].ypos += cteleporter->particles[p].yvel * particle_time;
				
				
				int alpha  = (uint8_t)(255 - floor(age * 255.0f));
				
				render_particle(cteleporter->x + cteleporter->particles[p].xpos, 
					cteleporter->y + cteleporter->particles[p].ypos, 
					alpha, 0xff, 0xff, 0xff);
			
				cteleporter->particles[p].last = cgame_time;
			}
		}
			
		
		cteleporter = cteleporter->next;
	}
}


void create_teleporter_sparkles()
{
	struct teleporter_t *cteleporter = teleporter0;
		
	while(cteleporter)
	{
		memset(cteleporter->particles, 0, sizeof(struct particle_t) * 1000);
		cteleporter->particle_power = 0.0;
		cteleporter->next_particle = 0;
		
		cteleporter = cteleporter->next;
	}
}	


void render_player_info()
{
	if(!player0)
		return;
	
	int y = vid_height / 6, w;
	struct blit_params_t params;
	
	struct player_t *cplayer = player0;
	
	while(cplayer)
	{
		w = blit_text_right_aligned(vid_width - 33, y, 0xff, 0x60, 0x0f, s_backbuffer, 
			cplayer->name->text);

		if(!match_begun)
		{
			if(cplayer->ready)
				blit_text_right_aligned(vid_width - 9, y, 0xff, 0x60, 0x0f, s_backbuffer, 
					"R");
		}
		else
		{
			blit_text_right_aligned(vid_width - 1, y, 0xff, 0x60, 0x0f, s_backbuffer, 
				"%i", cplayer->frags);
		}
		
		y += 14;
		cplayer = cplayer->next;
	}

	params.red = 0xff;
	params.green = 0xff;
	params.blue = 0;
	params.alpha = 0x7f;
	
	params.dest = s_backbuffer;
	params.dest_x = vid_width - 27;
	params.dest_y = vid_height / 6 - 2;
	params.width = 27;
	params.height = y - vid_height / 6 - 1;

	alpha_draw_rect(&params);

	
	params.red = 0xff;
	params.green = 0x8f;
	params.blue = 0x00;
	params.alpha = 0x7f;
	
	params.dest = s_backbuffer;
	params.x1 = vid_width - 31;
	params.y1 = vid_height / 6 - 2;
	params.x2 = params.x1;
	params.y2 = y - 4;

	draw_line(&params);
}


void render_match_time()
{
	if(match_begun)
	{
		int seconds;
		if(!match_over)
			seconds = (render_tick - match_start_tick) / 200;
		else
			seconds = (match_end_tick - match_start_tick) / 200;
			
		int minutes = seconds / 60;
		seconds -= minutes * 60;
		blit_text_right_aligned(vid_width, 0, 0xef, 0x6f, 0xff, s_backbuffer, 
			"%u:%02u", minutes, seconds);
	}
}


int display_help;

void render_match_info()
{
	if(server_discovery_started)
		return;
	
	if(display_help)
		return;
		
	if(!match_begun)
	{
		if(!ready)
		{
			blit_text_centered(vid_width / 2, vid_height / 6, 0xef, 0x6f, 0xff, 
				s_backbuffer, "The match has not yet started.");
			blit_text_centered(vid_width / 2, vid_height / 6 + 14, 0xef, 0x6f, 0xff, 
				s_backbuffer, "Press [Enter] when you are ready,");
			blit_text_centered(vid_width / 2, vid_height / 6 + 28, 0xef, 0x6f, 0xff, 
				s_backbuffer, "or wait for more people to join.");
		}
		else
		{
			blit_text_centered(vid_width / 2, vid_height / 6, 0xef, 0x6f, 0xff, 
				s_backbuffer, "The match will start when everyone is ready.");
		}
	}
	
	
	struct player_t *cplayer;
	struct string_t *s;
	
	if(match_over)
	{
		switch(winner_type)
		{
		case WINNER_YOU:
			blit_text_centered(vid_width / 2, vid_height / 6, 0xef, 0x6f, 0xff, 
				s_backbuffer, "You have won the match!");
			break;
		
		case WINNER_INDEX:
			
			cplayer = player0;
			
			while(cplayer)
			{
				if(cplayer->index == winner_index)
					break;
				
				cplayer = cplayer->next;
			}
			
			s = new_string_string(cplayer->name);
			string_cat_text(s, " has won the match!");
			
			blit_text_centered(vid_width / 2, vid_height / 6, 0xef, 0x6f, 0xff, 
				s_backbuffer, s->text);
			
			free_string(s);
			break;
		}
	}
}

void render_health_and_ammo()
{
	if(r_DrawConsole)
		return;
	
	blit_text_centered(vid_width / 2, vid_height * 5 / 6, 0xff, 0xff, 0xff, 
		s_backbuffer, "%.0f", round(cgame_state->craft_shield * 100.0));

	blit_text_centered(vid_width / 2, vid_height * 16 / 18, 0xff, 0, 0, 
		s_backbuffer, "%u", cgame_state->rails);

	blit_text_centered(vid_width / 2, vid_height * 17 / 18, 0xff, 0, 0, 
		s_backbuffer, "%u", cgame_state->mines);

	struct entity_t *craft = get_entity(centity0, cgame_state->follow_me);
		
	if(craft)
	{
	
		if(craft->craft_data.left_weapon)
		{
			blit_text_centered(vid_width * 8 / 18, vid_height * 5 / 6, 0xff, 0xff, 0xff, 
				s_backbuffer, "%.0f", round(cgame_state->left_shield * 100.0));
	
			blit_text_centered(vid_width * 8 / 18, vid_height * 16 / 18, 0xff, 0, 0, 
				s_backbuffer, "%u", cgame_state->left_ammo);
		}
	
		if(craft->craft_data.right_weapon)
		{
			blit_text_centered(vid_width * 10 / 18, vid_height * 5 / 6, 0xff, 0xff, 0xff, 
				s_backbuffer, "%.0f", round(cgame_state->right_shield * 100.0));
	
			blit_text_centered(vid_width * 10 / 18, vid_height * 16 / 18, 0xff, 0, 0, 
				s_backbuffer, "%u", cgame_state->right_ammo);
		}
	}
}


void render_help()
{
	if(server_discovery_started)
		return;
	
	if(!display_help)
	{
		blit_text_centered(vid_width / 2, 0, 0xff, 0xff, 0xff, 
			s_backbuffer, "Press F1 for controls");
		return;
	}
	
	blit_text_centered(vid_width / 3, vid_height / 6, 0xef, 0x6f, 0xff, 
		s_backbuffer, "mouse");
	blit_text_centered(vid_width / 3, vid_height / 6 + 14, 0xef, 0x6f, 0xff, 
		s_backbuffer, "left mouse button");
	blit_text_centered(vid_width / 3, vid_height / 6 + 28, 0xef, 0x6f, 0xff, 
		s_backbuffer, "right mouse button");
	blit_text_centered(vid_width / 3, vid_height / 6 + 42, 0xef, 0x6f, 0xff, 
		s_backbuffer, "w");
	blit_text_centered(vid_width / 3, vid_height / 6 + 56, 0xef, 0x6f, 0xff, 
		s_backbuffer, "s");
	blit_text_centered(vid_width / 3, vid_height / 6 + 70, 0xef, 0x6f, 0xff, 
		s_backbuffer, "a");
	blit_text_centered(vid_width / 3, vid_height / 6 + 84, 0xef, 0x6f, 0xff, 
		s_backbuffer, "d");
	blit_text_centered(vid_width / 3, vid_height / 6 + 98, 0xef, 0x6f, 0xff, 
		s_backbuffer, "space");

	
	blit_text_centered(vid_width / 2, vid_height / 6, 0xff, 0xff, 0xff, 
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 14, 0xff, 0xff, 0xff, 
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 28, 0xff, 0xff, 0xff, 
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 42, 0xff, 0xff, 0xff, 
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 56, 0xff, 0xff, 0xff, 
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 70, 0xff, 0xff, 0xff, 
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 84, 0xff, 0xff, 0xff,  
		s_backbuffer, "-");
	blit_text_centered(vid_width / 2, vid_height / 6 + 98, 0xff, 0xff, 0xff,  
		s_backbuffer, "-");

	
	blit_text_centered(vid_width / 3 * 2, vid_height / 6, 0xef, 0x6f, 0xff, 
		s_backbuffer, "rotate");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 14, 0xef, 0x6f, 0xff, 
		s_backbuffer, "fire railgun");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 28, 0xef, 0x6f, 0xff, 
		s_backbuffer, "brake");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 42, 0xef, 0x6f, 0xff, 
		s_backbuffer, "accelerate");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 56, 0xef, 0x6f, 0xff, 
		s_backbuffer, "brake");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 70, 0xef, 0x6f, 0xff, 
		s_backbuffer, "fire left weapon");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 84, 0xef, 0x6f, 0xff, 
		s_backbuffer, "fire right weapon");
	blit_text_centered(vid_width / 3 * 2, vid_height / 6 + 98, 0xef, 0x6f, 0xff, 
		s_backbuffer, "drop mine");

	blit_text_centered(vid_width / 2, vid_height / 6 + 126, 0xff, 0xff, 0xff, 
		s_backbuffer, "Press F1 to clear");
}


void toggle_help(int state)
{
	if(!state)
		return;
	
	display_help = !display_help;
}


void render_game()
{
	if(downloading_map)
		render_map_downloading();
	
	if(!game_rendering)
		return;
	
	pthread_mutex_lock(&gamestate_mutex);
	
	struct entity_t *entity;
	switch(game_state)
	{
	case GAMESTATE_PLAYING:
		update_game();
		entity = get_entity(centity0, cgame_state->follow_me);
		break;
	
	case GAMESTATE_DEMO:
		update_demo();
		entity = get_entity(centity0, demo_follow_me);
		break;
	
	default:
		pthread_mutex_unlock(&gamestate_mutex);
		return;
	}
	
	if(game_state == GAMESTATE_DEAD)
	{
		pthread_mutex_unlock(&gamestate_mutex);
		return;
	}
		
	if(!entity)
	{
		pthread_mutex_unlock(&gamestate_mutex);
		return;
	}
	
	if(entity->teleporting)
	{
		viewx = teleporting_start_x;
		viewy = teleporting_start_y;
	}
	else
	{
		viewx = entity->xdis;
		viewy = entity->ydis;
	}
	
//	add_offset_view(entity);
	add_moving_view();
	
	pthread_mutex_unlock(&gamestate_mutex);

	render_stars();
	render_floating_images();
	render_lower_particles();
	render_rail_trails();
	render_entities();
	render_upper_particles();
	render_map();
	render_teleporters();
	
	if(game_state != GAMESTATE_DEMO)
	{
		render_help();
		render_recording();
		render_player_info();
		render_match_time();
		render_match_info();
		render_health_and_ammo();
	}
	
//	blit_text(((vid_width * 2) / 3) + (vid_width / 200), 
//		vid_height / 6, 0xef, 0x6f, 0xff, s_backbuffer, "[virus] where are you?");

}


void qc_name(char *new_name)
{
	if(game_state == GAMESTATE_PLAYING || 
		game_state == GAMESTATE_SPECTATING)
	{
		net_emit_uint8(game_conn, EMMSG_NAMECNGE);
		net_emit_string(game_conn, new_name);
		net_emit_end_of_stream(game_conn);
	}		
}


void cf_connect(char *addr)
{
	em_connect_text(addr);
}


void cf_disconnect()
{
	if(game_conn)
		em_disconnect(game_conn);
}


void emit_colours_event()
{
	if(game_state != GAMESTATE_PLAYING)
		return;
	
	net_emit_uint8(game_conn, EMMSG_COLOURS);
	emit_colours();
	net_emit_end_of_stream(game_conn);
}


void qc_game_colour_cvar(int *var, int val)
{
	*var = (uint8_t)val;
	emit_colours_event();
}


void create_colour_cvars()
{
	create_cvar_int("rail_inner_red", &rail_inner_red, 0);
	create_cvar_int("rail_inner_green", &rail_inner_green, 0);
	create_cvar_int("rail_inner_blue", &rail_inner_blue, 0);
	create_cvar_int("rail_outer_red", &rail_outer_red, 0);
	create_cvar_int("rail_outer_green", &rail_outer_green, 0);
	create_cvar_int("rail_outer_blue", &rail_outer_blue, 0);

	create_cvar_int("magic_smoke", &magic_smoke, 0);

	create_cvar_int("smoke_start_red", &smoke_start_red, 0);
	create_cvar_int("smoke_start_green", &smoke_start_green, 0);
	create_cvar_int("smoke_start_blue", &smoke_start_blue, 0);
	create_cvar_int("smoke_end_red", &smoke_end_red, 0);
	create_cvar_int("smoke_end_green", &smoke_end_green, 0);
	create_cvar_int("smoke_end_blue", &smoke_end_blue, 0);

	create_cvar_int("plasma_red", &plasma_red, 0);
	create_cvar_int("plasma_green", &plasma_green, 0);
	create_cvar_int("plasma_blue", &plasma_blue, 0);

	create_cvar_int("shield_red", &shield_red, 0);
	create_cvar_int("shield_green", &shield_green, 0);
	create_cvar_int("shield_blue", &shield_blue, 0);
}


void init_game()
{
	set_ri_surface_multiplier((double)vid_width / 1600.0);
	
	init_stars();
	init_particles();
	
	create_cvar_command("status", cf_status);
	create_cvar_command("say", cf_say);
	create_cvar_command("play", cf_play);
	create_cvar_command("spectate", cf_spectate);
	
	create_cvar_command("record", cf_record);
	create_cvar_command("stop", cf_stop);
	create_cvar_command("demo", cf_demo);
	
	create_cvar_command("connect", cf_connect);
	create_cvar_command("disconnect", cf_disconnect);
	
	set_string_cvar_qc_function("name", qc_name);
	
	create_cvar_command("suicide", cf_suicide);
	
	set_int_cvar_qc_function_wv("magic_smoke", qc_game_colour_cvar);
	
	set_int_cvar_qc_function_wv("rail_inner_red", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("rail_inner_green", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("rail_inner_blue", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("rail_outer_red", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("rail_outer_green", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("rail_outer_blue", qc_game_colour_cvar);

	set_int_cvar_qc_function_wv("smoke_start_red", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("smoke_start_green", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("smoke_start_blue", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("smoke_end_red", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("smoke_end_green", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("smoke_end_blue", qc_game_colour_cvar);

	set_int_cvar_qc_function_wv("plasma_red", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("plasma_green", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("plasma_blue", qc_game_colour_cvar);

	set_int_cvar_qc_function_wv("shield_red", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("shield_green", qc_game_colour_cvar);
	set_int_cvar_qc_function_wv("shield_blue", qc_game_colour_cvar);
	

	ris_plasma = load_ri_surface(find_resource("stock-object-textures/plasma.png"));
	ris_craft_shield = load_ri_surface(find_resource("stock-object-textures/craft-shield.png"));
	ris_weapon_shield = load_ri_surface(find_resource("stock-object-textures/weapon-shield.png"));
	ris_shield_pickup = load_ri_surface(find_resource("stock-object-textures/shield-pickup.png"));
	ris_mine = load_ri_surface(find_resource("stock-object-textures/mine.png"));
	
	railgun_sample = load_sample(find_resource("stock-sounds/railgun.ogg"));
	teleporter_sample = load_sample(find_resource("stock-sounds/teleporter.ogg"));
	speedup_ramp_sample = load_sample(find_resource("stock-sounds/speedup-ramp.ogg"));
	explosion_sample = load_sample(find_resource("stock-sounds/explosion.ogg"));
	plasma_cannon_sample = load_sample(find_resource("stock-sounds/plasma-cannon.ogg"));
	rocket_sample = load_sample(find_resource("stock-sounds/rocket.ogg"));
}


void kill_game()
{
}
