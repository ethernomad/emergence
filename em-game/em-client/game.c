#ifdef LINUX
#define _GNU_SOURCE
#define _REENTRANT
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <memory.h>
#include <assert.h>

#include <zlib.h>

#include "../common/prefix.h"

#include "../common/types.h"
#include "../common/minmax.h"
#include "../common/llist.h"
#include "../common/stringbuf.h"
#include "../common/buffer.h"
#include "shared/rdtsc.h"
#include "shared/cvar.h"
#include "shared/network.h"
#include "particles.h"
#include "shared/sgame.h"
#include "rcon.h"
#include "map.h"
#include "console.h"
#include "../gsub/gsub.h"
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

#ifdef LINUX
#include "shared/timer.h"
#endif

#ifdef WIN32
#include "../common/win32/math.h"
#endif


struct event_craft_data_t
{
	float acc;
	float theta;
	int braking;
	uint32_t left_weapon_index;
	uint32_t right_weapon_index;
	float shield_flare;
	
	int carcass;	// handled sepatately
};


struct event_weapon_data_t
{
	int type;
	float theta;
	uint32_t craft_index;
	float shield_flare;
	int detached;
};


struct event_rocket_data_t
{
	float theta;
	uint32_t weapon_id;
};


struct event_plasma_data_t
{
	uint32_t weapon_id;
};


struct event_mine_data_t
{
	uint32_t craft_id;
};


struct event_t
{
	uint32_t tick;
	uint8_t type;
	int ooo;
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
				struct event_rocket_data_t rocket_data;
				struct event_plasma_data_t plasma_data;
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
			
		} railtrail_data;
		
		struct
		{
			float x, y;
			float size;
			
		} explosion_data;
		
		int frags;
	};
	
	struct event_t *next;
	
} *event0 = NULL;


struct message_reader_t
{
	uint8_t type;
	struct buffer_t *stream;
	gzFile *gzdemo;

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
float cgame_time;

struct entity_t *centity0;

double viewx = 0.0, viewy = 0.0;


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

int frags = 0;

struct game_state_t
{
	uint32_t tick;
	struct entity_t *entity0;
	uint32_t follow_me;
	int tainted;	
	
	struct game_state_t *next;
		
} *game_state0, last_known_game_state, *cgame_state;


uint32_t demo_first_tick;
uint32_t demo_last_tick;
uint32_t demo_follow_me;

gzFile gzdemo;

#define RAIL_TRAIL_EXPAND_TIME 0.5
#define RAIL_TRAIL_EXPAND_DISTANCE 10.0
#define RAIL_TRAIL_INITIAL_EXPAND_VELOCITY ((2 * RAIL_TRAIL_EXPAND_DISTANCE) / RAIL_TRAIL_EXPAND_TIME)
#define RAIL_TRAIL_EXPAND_ACC ((-RAIL_TRAIL_INITIAL_EXPAND_VELOCITY) / RAIL_TRAIL_EXPAND_TIME)




struct rail_trail_t
{
	float start_time;
	float x1, y1;
	float x2, y2;

	struct rail_trail_t *next;
		
} *rail_trail0;

struct sample_t *railgun_sample;
struct sample_t *teleporter_sample;
struct sample_t *speedup_ramp_sample;


struct demo_t
{
	struct string_t *filename;
	struct demo_t *next;
		
} *demo0, *cdemo;


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
	
	if(recording)
	{
		recording = 0;
		gzclose(gzrecording);
	}
	
	cgame_tick = 0;
	cgame_time = 0.0;
	
	memset(&message_reader, 0, sizeof(message_reader));
	
	centity0 = NULL;
	frags = 0;
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
	rocket_data->weapon_id = message_reader_read_uint32();
}


void read_plasma_data(struct event_plasma_data_t *plasma_data)
{
	plasma_data->weapon_id = message_reader_read_uint32();
}


void read_mine_data(struct event_mine_data_t *mine_data)
{
	mine_data->craft_id = message_reader_read_uint32();
}


void read_spawn_ent_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
	event->ent_data.type = message_reader_read_uint8();
	event->ent_data.skin = message_reader_read_uint32();
	event->ent_data.xdis = message_reader_read_float();
	event->ent_data.ydis = message_reader_read_float();
	event->ent_data.xvel = message_reader_read_float();
	event->ent_data.yvel = message_reader_read_float();
	
	switch(event->ent_data.type)
	{
	case ENT_CRAFT:
		read_craft_data(&event->ent_data.craft_data);
	
		// separate because carcass data is not sent in update
		event->ent_data.craft_data.carcass = message_reader_read_uint8();
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


void read_update_ent_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
	event->ent_data.type = message_reader_read_uint8();
	event->ent_data.xdis = message_reader_read_float();
	event->ent_data.ydis = message_reader_read_float();
	event->ent_data.xvel = message_reader_read_float();
	event->ent_data.yvel = message_reader_read_float();
	
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


void read_kill_ent_event(struct event_t *event)
{
	event->ent_data.index = message_reader_read_uint32();
}


void read_follow_me_event(struct event_t *event)
{
	event->follow_me_data.index = message_reader_read_uint32();
}


void read_carcass_event(struct event_t *event)
{
	event->carcass_data.index = message_reader_read_uint32();
}


void read_railtrail_event(struct event_t *event)
{
	event->railtrail_data.x1 = message_reader_read_float();
	event->railtrail_data.y1 = message_reader_read_float();
	event->railtrail_data.x2 = message_reader_read_float();
	event->railtrail_data.y2 = message_reader_read_float();
}


void read_frags_event(struct event_t *event)
{
	event->frags = message_reader_read_int();
}


void read_explosion_event(struct event_t *event)
{
	event->explosion_data.x = message_reader_read_float();
	event->explosion_data.y = message_reader_read_float();
	event->explosion_data.size = message_reader_read_float();
}


void read_event(struct event_t *event)
{
	event->tick = message_reader.event_tick;
	event->type = message_reader.message_type;
	
	switch(event->type)
	{
	case EMEVENT_SPAWN_ENT:
		read_spawn_ent_event(event);
		break;
	
	case EMEVENT_UPDATE_ENT:
		read_update_ent_event(event);
		break;
	
	case EMEVENT_KILL_ENT:
		read_kill_ent_event(event);
		break;
	
	case EMEVENT_FOLLOW_ME:
		read_follow_me_event(event);
		break;
	
	case EMEVENT_CARCASS:
		read_carcass_event(event);
		break;
	
	case EMEVENT_RAILTRAIL:
		read_railtrail_event(event);
		break;
	
	case EMEVENT_FRAGS:
		read_frags_event(event);
		break;
	
	case EMEVENT_EXPLOSION:
		read_explosion_event(event);
		break;
	}
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
	case GAMESTATE_DEMO:
		break;
	
	case GAMESTATE_CONNECTING:
	case GAMESTATE_SPECTATING:
	case GAMESTATE_PLAYING:
		em_disconnect(game_conn);
		break;
	}
	
	clear_game();
		
	
	game_state = GAMESTATE_CONNECTING;
	game_conn = conn;
	
	if(!recording)
		message_reader.type = MESSAGE_READER_STREAM;
	
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


int game_process_proto_ver()
{
	if(game_state != GAMESTATE_CONNECTING && game_state != GAMESTATE_DEMO)
		return 0;
	
	uint8_t proto_ver = message_reader_read_uint8();
	
	if(proto_ver == EM_PROTO_VER)
	{
		console_print("Correct protocol version\n");
		
		if(game_state != GAMESTATE_DEMO)
		{
			net_emit_uint8(game_conn, EMMSG_JOIN);
			net_emit_string(game_conn, get_cvar_string("name"));
			net_emit_end_of_stream(game_conn);
		}
	}
	else
	{
		console_print("Incorrect protocol version\n");
		game_state = GAMESTATE_DEAD;
		em_disconnect(game_conn);
	}
	
	return 1;
}


int game_process_playing()
{
	if(game_state == GAMESTATE_DEAD)
		return 0;
	
	
	game_state0 = calloc(1, sizeof(struct game_state_t));
	game_state0->tick = message_reader_read_uint32();		// oh dear
	
	last_known_game_state.tick = game_state0->tick;
	
	
//	game_tick = 
//	console_print("start tick: %u\n", game_tick);
	
	game_state = GAMESTATE_PLAYING;

	return 1;
}


int game_process_spectating()
{
	if(game_state == GAMESTATE_DEAD)
		return 0;
	
	game_state = GAMESTATE_SPECTATING;

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
	
		// separate because carcass data is not sent in update_event
		event->ent_data.craft_data.carcass = message_reader_read_uint8();
		break;
	
	case ENT_WEAPON:
		read_weapon_data(&event->ent_data.weapon_data);
		event->ent_data.weapon_data.detached = message_reader_read_uint8();
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
		entity->craft_data.left_weapon = get_entity(centity0, 
			event->ent_data.craft_data.left_weapon_index);
		entity->craft_data.right_weapon = get_entity(centity0, 
			event->ent_data.craft_data.right_weapon_index);
		entity->craft_data.shield_flare = event->ent_data.craft_data.shield_flare;
		entity->craft_data.carcass = event->ent_data.craft_data.carcass;
		entity->craft_data.skin = event->ent_data.skin;
		entity->craft_data.surface = skin_get_craft_surface(event->ent_data.skin);
		entity->craft_data.particle = 0.0;
		break;
	
	case ENT_WEAPON:
		entity->weapon_data.type = event->ent_data.weapon_data.type;
		entity->weapon_data.theta = event->ent_data.weapon_data.theta;
		entity->weapon_data.craft = get_entity(centity0, 
			event->ent_data.weapon_data.craft_index);
		entity->weapon_data.shield_flare = event->ent_data.weapon_data.shield_flare;
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
		entity->plasma_data.in_weapon = 1;
		entity->plasma_data.weapon_id = event->ent_data.plasma_data.weapon_id;
		break;
	
	case ENT_ROCKET:
		entity->rocket_data.theta = event->ent_data.rocket_data.theta;
		entity->rocket_data.start_tick = cgame_tick;
		entity->rocket_data.in_weapon = 1;
		entity->rocket_data.weapon_id = event->ent_data.rocket_data.weapon_id;
		break;
	
	case ENT_MINE:
		entity->mine_data.under_craft = 1;
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
	craft->craft_data.particle = 0.0;
	
	strip_weapons_from_craft(craft);
}


void add_railtrail_event(struct event_t *event)
{
	event->railtrail_data.x1 = message_reader_read_float();
	event->railtrail_data.y1 = message_reader_read_float();
	event->railtrail_data.x2 = message_reader_read_float();
	event->railtrail_data.y2 = message_reader_read_float();
}


void add_frags_event(struct event_t *event)
{
	event->frags = message_reader_read_int();
}


void process_railtrail_event(struct event_t *event)
{
	struct rail_trail_t rail_trail;
		
	rail_trail.start_time = cgame_time;
	rail_trail.x1 = event->railtrail_data.x1;
	rail_trail.y1 = event->railtrail_data.y1;
	rail_trail.x2 = event->railtrail_data.x2;
	rail_trail.y2 = event->railtrail_data.y2;
	
	LL_ADD(struct rail_trail_t, &rail_trail0, &rail_trail);
	start_sample(railgun_sample, event->tick);
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
	
	if(weapon->weapon_data.craft->craft_data.left_weapon == weapon)
		weapon->weapon_data.craft->craft_data.left_weapon = NULL;
	else
		weapon->weapon_data.craft->craft_data.right_weapon = NULL;
	
	weapon->weapon_data.craft = NULL;
}


void add_explosion_event(struct event_t *event)
{
	event->explosion_data.x = message_reader_read_float();
	event->explosion_data.y = message_reader_read_float();
	event->explosion_data.size = message_reader_read_float();
}


void process_explosion_event(struct event_t *event)
{
	explosion(event->explosion_data.x, event->explosion_data.y, 
		event->explosion_data.size);
}


void process_teleport_event(struct event_t *event)
{
	start_sample(teleporter_sample, event->tick);
}


void process_speedup_event(struct event_t *event)
{
	start_sample(speedup_ramp_sample, event->tick);
}


void process_frags_event(struct event_t *event)
{
	frags = event->frags;
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
			
			case EMEVENT_FRAGS:
				process_frags_event(event);
				break;
			
			case EMEVENT_EXPLOSION:
				process_explosion_event(event);
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
			
			case EMEVENT_FRAGS:
				process_frags_event(event);
				break;
			
			case EMEVENT_EXPLOSION:
				process_explosion_event(event);
				break;
			}
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
			console_print("ooo neutralized\n");
			
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
	
	case EMEVENT_FRAGS:
		add_frags_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
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
	
	case EMEVENT_FRAGS:
		add_frags_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
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
	
	case EMEVENT_FRAGS:
		add_frags_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
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
	
	case EMEVENT_FRAGS:
		add_frags_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
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
	
	case EMEVENT_FRAGS:
		add_frags_event(&event);
		break;
	
	case EMEVENT_EXPLOSION:
		add_explosion_event(&event);
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
	
	case EMNETMSG_PLAYING:
		return game_process_playing();
	
	case EMNETMSG_SPECTATING:
		return game_process_spectating();
	
	case EMNETMSG_INRCON:
		break;
	
	case EMNETMSG_NOTINRCON:
		break;
	
	case EMNETMSG_JOINED:
		break;
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
	set_ri_surface_multiplier((double)vid_width / 1600.0);
	reload_map();
	reload_skins();
	
	// getting entities' new surfaces
	
	struct game_state_t *game_state = game_state0;
		
	while(game_state)
	{
		struct entity_t *entity = game_state->entity0;
	
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
		
		game_state = game_state->next;
	}

	
	struct entity_t *entity = centity0;

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
	gzwrite(gzrecording, &plasma->plasma_data.weapon_id, 4);
}


void write_rocket_data_to_demo(struct entity_t *rocket)
{
	gzwrite(gzrecording, &rocket->rocket_data.theta, 4);
	gzwrite(gzrecording, &rocket->rocket_data.weapon_id, 4);
}


void write_mine_data_to_demo(struct entity_t *mine)
{
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
	gzwrite(gzrecording, &cgame_tick, 4);
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
		gzwrite(gzrecording, &entity->craft_data.carcass, 1);
		break;
	
	case ENT_WEAPON:
		write_weapon_data_to_demo(entity);
		gzwrite(gzrecording, &entity->weapon_data.detached, 1);
		break;
	
	case ENT_PLASMA:
		write_plasma_data_to_demo(entity);
		break;
	
	case ENT_ROCKET:
		write_rocket_data_to_demo(entity);
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
	struct entity_t *centity = centity0;

	while(centity)
	{
		write_entity_to_demo(centity);
		centity = centity->next;
	}
}


void cf_record(char *c)
{
	if(recording)
		;
	
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
		
		msg = EMMSG_LOADSKIN;
		gzwrite(gzrecording, &msg, 1);
		
		struct string_t *string = new_string_text("default");
		gzwrite_string(message_reader.gzdemo, string);
		free_string(string);
		
		msg_32 = 0;
		gzwrite(gzrecording, &msg_32, 4);
		
		write_all_entities_to_demo();

		msg = EMEVENT_FOLLOW_ME;
		gzwrite(gzrecording, &msg, 1);
		gzwrite(gzrecording, &cgame_tick, 4);
		gzwrite(gzrecording, &cgame_state->follow_me, 4);
		
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
	
	if(gzdemo)
	{
		gzclose(gzdemo);
		gzdemo = NULL;
	}
	
	switch(game_state)
	{
	case GAMESTATE_DEAD:
		break;
	
	case GAMESTATE_DEMO:
		gzclose(gzdemo);
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
		console_print("File not found.\n");
		game_state = GAMESTATE_DEAD;
		return;
	}
	
	game_state = GAMESTATE_DEMO;
	message_reader.gzdemo = gzdemo;
	message_reader.type = MESSAGE_READER_GZDEMO;
	demo_first_tick = 0;
	cgame_tick = 0;
	cdemo = demo0;
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


void explosion(float x, float y, float size)
{
	int np, p;
	np = lrint(size / 5);
	
	
	struct particle_t particle;
	particle.xpos = x;
	particle.ypos = y;
	
	for(p = 0; p < np; p++)
	{
		double sin_theta, cos_theta;
		sincos(drand48() * 2 * M_PI, &sin_theta, &cos_theta);
		
		double r = drand48();
		
		particle.xvel = -sin_theta * size * r;
		particle.yvel = cos_theta * size * r;
		particle.creation = particle.last = cgame_time;
		create_upper_particle(&particle);
	}
}


void tick_craft(struct entity_t *craft, float xdis, float ydis)
{
	if(cgame_tick <= craft->craft_data.last_tick)
		return;
	
	if(craft->teleporting)
		return;
	
	craft->craft_data.last_tick = cgame_tick;
	
	int np = 0, p;
	double sin_theta, cos_theta;
	struct particle_t particle;


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
			
			double nxdis = craft->xdis + (xdis - craft->xdis) * (double)(p + 1) / (double)np;
			double nydis = craft->ydis + (ydis - craft->ydis) * (double)(p + 1) / (double)np;
			

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
			sincos(craft->craft_data.theta, &sin_theta, &cos_theta);
			
			double r = drand48();
			
			double nxdis = craft->xdis + (xdis - craft->xdis) * (double)(p + 1) / (double)np;
			double nydis = craft->ydis + (ydis - craft->ydis) * (double)(p + 1) / (double)np;
			
			
	
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
			
		
			particle.xpos = nxdis + sin_theta * 25;
			particle.ypos = nydis - cos_theta * 25;
	
			sincos(theta + M_PI / 2, &sin_theta, &cos_theta);
			
			particle.xpos -= sin_theta * 15;
			particle.ypos += cos_theta * 15;
			
			sincos(theta + (drand48() - 0.5) * 0.1, &sin_theta, &cos_theta);
			particle.xvel = craft->xvel + craft->craft_data.acc * sin_theta * 100000 * r;
			particle.yvel = craft->yvel + -craft->craft_data.acc * cos_theta * 100000 * r;
				
			create_lower_particle(&particle);
	
			particle.xpos = nxdis + sin_theta * 25;
			particle.ypos = nydis - cos_theta * 25;
	
			sincos(theta + M_PI / 2, &sin_theta, &cos_theta);
			
			particle.xpos += sin_theta * 15;
			particle.ypos -= cos_theta * 15;
			
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
		
		
		struct particle_t particle;

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

				params.red = params.green = params.blue = 0xff;
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
				
				params.red = params.green = params.blue = 0xff;
				params.alpha = lrint(entity->weapon_data.shield_flare * 255.0);
			
				alpha_blit_surface(&params);
			}
		
			break;
		
		case ENT_PLASMA:

			params.source = ris_plasma->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.red = 0x32;
			params.green = 0x73;
			params.blue = 0x71;
			
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
			break;
		
		case ENT_RAILS:
		case ENT_ROCKET:
			
			params.source = ris_plasma->surface;
		
			world_to_screen(entity->xdis, entity->ydis, &x, &y);
		
			params.red = 0x32;
			params.green = 0x73;
			params.blue = 0x71;
			
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
		
/*		case ENT_RAILS:
			break;
*/			
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
	
	
	struct entity_t *centity = old_game_state->entity0;
		
	while(centity)
	{
		LL_ADD(struct entity_t, &new_game_state->entity0, centity);
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
	// get render_tick and last_known_tick
	
	update_tick_parameters();
	
	uint32_t render_tick = get_game_tick();
	
	uint32_t new_io_tick = 0, new_ooo_tick = 0;
	uint32_t first_io_tick = 0, last_io_tick = 0, first_ooo_tick = 0;
	
	struct event_t *cevent = event0;
		
	while(cevent)
	{
		if(cevent->tick <= render_tick)
		{
			if(cevent->ooo)
			{
				if(!new_ooo_tick)
				{
					new_ooo_tick = 1;
					first_ooo_tick = cevent->tick;
				}
				else
				{
					first_ooo_tick = min(first_ooo_tick, cevent->tick);
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
					first_io_tick = min(first_io_tick, cevent->tick);
					last_io_tick = max(last_io_tick, cevent->tick);
				}
			}
		}
		
		cevent = cevent->next;
	}


	if(new_io_tick)
	{
		if(last_known_game_state.tick != first_io_tick)
		{
			if(!game_state0->tainted)
			{
				struct game_state_t *game_state = game_state0;
					
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
		if(game_state0->tainted)
		{
			free_game_state_list(&game_state0);
			game_state0 = malloc(sizeof(struct game_state_t));
			duplicate_game_state(&last_known_game_state, game_state0);
		}
	}

	
	struct game_state_t *game_state = game_state0;
		
	while(game_state->next)
	{
		if(new_ooo_tick && game_state->tick >= first_ooo_tick)
			break;
		
		if(game_state->next->tainted)
			break;
		
		game_state = game_state->next;
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
	uint32_t tick;
	
	top:
	
	if(demo_first_tick)		// i.e. not the first event
	{
		tick = get_tick_from_wall_time() + demo_first_tick;
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
				
				reset_tick_from_wall_time();	// demo_first_tick is being rendered now
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
				console_print("File not found.\n");
				game_state = GAMESTATE_DEAD;
				return;
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
			
			render_particle(cx, cy, alpha, 0xff, 0xff, 0xff);
			
			double theta = length / 30 * M_PI * t;
			
			double nr = cos(theta) * r;
			
			
			
			cx -= deltay * nr;
			cy += deltax * nr;
	
			if(rail_time < 0.5)
				alpha  = 0xff;
			else
				alpha  = (uint8_t)(255 - floor((rail_time - 0.5) / 2.0 * 255.0f));
			render_particle(cx, cy, alpha, 0, 7, 0xff);
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


void render_game()
{
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
		return;
	}
	
	
	if(entity)
	{
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
	}
	else
	{
		viewx = 0.0;
		viewy = 0.0;
	}
	
//	add_offset_view(entity);
	add_moving_view();
	
	render_stars();
	render_floating_images();
	render_lower_particles();
	render_rail_trails();
	render_entities();
	render_upper_particles();
	render_map();
	render_teleporters();
	render_recording();
	
	blit_text_centered(vid_width / 2, vid_height / 6, 0xff, 0x60, 0x0f, s_backbuffer, "%i", frags);
	
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
	em_connect(addr);
}


void cf_disconnect()
{
	em_disconnect(game_conn);
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
	
	ris_plasma = load_ri_surface(BR_DATADIR("/emergence/stock-object-textures/plasma.png"));
	ris_craft_shield = load_ri_surface(BR_DATADIR("/emergence/stock-object-textures/craft-shield.png"));
	ris_weapon_shield = load_ri_surface(BR_DATADIR("/emergence/stock-object-textures/weapon-shield.png"));
	ris_shield_pickup = load_ri_surface(BR_DATADIR("/emergence/stock-object-textures/shield-pickup.png"));
	ris_mine = load_ri_surface(BR_DATADIR("/emergence/stock-object-textures/mine.png"));
	
	railgun_sample = load_sample(BR_DATADIR("/emergence/stock-sounds/railgun.ogg"));
	teleporter_sample = load_sample(BR_DATADIR("/emergence/stock-sounds/teleporter.ogg"));
	speedup_ramp_sample = load_sample(BR_DATADIR("/emergence/stock-sounds/speedup-ramp.ogg"));
}


void kill_game()
{
}
