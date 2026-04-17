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

#ifdef WIN32
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include <zlib.h>

#include "prefix.h"

#include "types.h"
#include "minmax.h"
#include "cvar.h"
#include "llist.h"
#include "stringbuf.h"
#include "buffer.h"
#include "resource.h"
#include "parse.h"
#include "user.h"
#include "timer.h"
#include "network.h"
#include "sgame.h"
#include "objects.h"
#include "bsp.h"
#include "fileinfo.h"
#include "game.h"
#include "console.h"
#include "ping.h"
#include "main.h"
#include "entry.h"



struct pickup_spawn_point_t *pickup_spawn_point0 = NULL;


struct string_t *map_filename = NULL;
struct string_t *mapname = NULL;
	
int map_size;
uint8_t map_hash[FILEINFO_DIGEST_SIZE];


uint32_t game_tick;
uint32_t cgame_tick;

int game_state = GS_DEAD;

int num_players = 0;
int num_spectators = 0;
int max_players = 3;
int max_spectators = 5;
int spawn_points = 0;

int match_duration = 10;

struct player_t *player0 = NULL;
struct entity_t *entity0;

int match_begun = 0;
int match_over = 0;
uint32_t match_start_tick;
uint32_t match_end_tick;

uint32_t next_player_index = 0;

struct player_t *winner;

struct player_t *new_player()
{
	struct player_t *player;
	LL_CALLOC(struct player_t, &player0, &player);
	player->index = next_player_index++;
	return player;
}


void remove_player(uint32_t conn)
{
	struct player_t *temp, *player;

	if(!player0)
		return;

	if(player0->conn == conn)
	{
		temp = player0->next;
		free(player0);
		player0 = temp;

		return;
	}

	player = player0;
	
	while(player->next)
	{
		if(player->next->conn == conn)
		{
			temp = player->next->next;

			free(player->next);

			player->next = temp;

			break;
		}
	}
}


struct player_t *get_player_by_conn(uint32_t conn)
{
	struct player_t *player = player0;
	
	while(player)
	{
		if(player->conn == conn)
			return player;

		player = player->next;
	}

	return NULL;
}


struct player_t *get_player_by_craft(struct entity_t *craft)
{
	struct player_t *player = player0;
	
	while(player)
	{
		if(player->craft == craft)
			return player;

		player = player->next;
	}

	return NULL;
}


struct player_t *get_player_by_name(char *name)
{
	if(!name)
		return NULL;
	
	struct player_t *player = player0;
	
	while(player)
	{
		if(strcmp(player->name->text, name) == 0)
			return player;

		player = player->next;
	}

	return NULL;
}


void write_craft_data_to_net(uint32_t conn, struct entity_t *craft)
{
	net_emit_float(conn, craft->craft_data.acc);
	net_emit_float(conn, craft->craft_data.theta);
	net_emit_int(conn, craft->craft_data.braking);
	net_emit_int(conn, craft->craft_data.rolling_left);
	net_emit_int(conn, craft->craft_data.rolling_right);
	
	if(craft->craft_data.left_weapon)
		net_emit_uint32(conn, craft->craft_data.left_weapon->index);
	else
		net_emit_uint32(conn, NO_ENT_INDEX);
	
	if(craft->craft_data.right_weapon)
		net_emit_uint32(conn, craft->craft_data.right_weapon->index);
	else
		net_emit_uint32(conn, NO_ENT_INDEX);
	
	net_emit_float(conn, craft->craft_data.shield_flare);
}


void write_weapon_data_to_net(uint32_t conn, struct entity_t *weapon)
{
	net_emit_int(conn, weapon->weapon_data.type);
	net_emit_float(conn, weapon->weapon_data.theta);
	
	if(weapon->weapon_data.craft)
		net_emit_uint32(conn, weapon->weapon_data.craft->index);
	else
		net_emit_uint32(conn, NO_ENT_INDEX);
		
	net_emit_float(conn, weapon->weapon_data.shield_flare);
}


void write_plasma_data_to_net(uint32_t conn, struct entity_t *plasma)
{
	net_emit_uint8(conn, plasma->plasma_data.in_weapon);
	net_emit_uint32(conn, plasma->plasma_data.weapon_id);
}


void write_bullet_data_to_net(uint32_t conn, struct entity_t *bullet)
{
	net_emit_uint8(conn, bullet->bullet_data.in_weapon);
	net_emit_uint32(conn, bullet->bullet_data.weapon_id);
}


void write_rocket_data_to_net(uint32_t conn, struct entity_t *rocket)
{
	net_emit_float(conn, rocket->rocket_data.theta);
	net_emit_uint32(conn, rocket->rocket_data.start_tick);
	net_emit_uint8(conn, rocket->rocket_data.in_weapon);
	net_emit_uint32(conn, rocket->rocket_data.weapon_id);
}


void write_mine_data_to_net(uint32_t conn, struct entity_t *mine)
{
	net_emit_uint8(conn, mine->mine_data.under_craft);
	net_emit_uint32(conn, mine->mine_data.craft_id);
}


void write_rails_data_to_net(uint32_t conn, struct entity_t *rails)
{
	;
}


void write_shield_data_to_net(uint32_t conn, struct entity_t *shield)
{
	;
}


void propagate_entity(struct entity_t *entity)
{
	struct player_t *player = player0;

	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_UPDATE_ENT);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, entity->index);
			net_emit_uint8(player->conn, entity->type);
			net_emit_float(player->conn, entity->xdis);
			net_emit_float(player->conn, entity->ydis);
			net_emit_float(player->conn, entity->xvel);
			net_emit_float(player->conn, entity->yvel);
			net_emit_int(player->conn, entity->teleporting);
			net_emit_uint32(player->conn, entity->teleporting_tick);
			net_emit_uint32(player->conn, entity->teleport_spawn_index);
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				write_craft_data_to_net(player->conn, entity);
				break;
			
			case ENT_WEAPON:
				write_weapon_data_to_net(player->conn, entity);
				break;
			
			case ENT_PLASMA:
				write_plasma_data_to_net(player->conn, entity);
				break;
			
			case ENT_ROCKET:
				write_rocket_data_to_net(player->conn, entity);
				break;
			
			case ENT_MINE:
				write_mine_data_to_net(player->conn, entity);
				break;
			
			case ENT_RAILS:
				write_rails_data_to_net(player->conn, entity);
				break;
			
			case ENT_SHIELD:
				write_shield_data_to_net(player->conn, entity);
				break;
			}
		
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void load_map_on_player(struct player_t *player)
{
	net_emit_uint8(player->conn, EMMSG_LOADMAP);
	net_emit_string(player->conn, mapname->text);
	net_emit_uint32(player->conn, map_size);
	net_emit_buf(player->conn, map_hash, FILEINFO_DIGEST_SIZE);
}


void load_all_skins_on_player(struct player_t *player)
{
	net_emit_uint8(player->conn, EMMSG_LOADSKIN);
	net_emit_string(player->conn, "default");
	net_emit_uint32(player->conn, 0);			// index
}


void emit_spawn_entity(uint32_t conn, struct entity_t *entity)
{
	net_emit_uint8(conn, EMEVENT_SPAWN_ENT);
	net_emit_uint32(conn, game_tick);
	net_emit_uint32(conn, entity->index);
	net_emit_uint8(conn, entity->type);
	net_emit_uint32(conn, 0);		// skin
	net_emit_float(conn, entity->xdis);
	net_emit_float(conn, entity->ydis);
	net_emit_float(conn, entity->xvel);
	net_emit_float(conn, entity->yvel);
	net_emit_int(conn, entity->teleporting);
	net_emit_uint32(conn, entity->teleporting_tick);
	net_emit_uint32(conn, entity->teleport_spawn_index);
	
	switch(entity->type)
	{
	case ENT_CRAFT:
		write_craft_data_to_net(conn, entity);
	
		net_emit_uint8(conn, entity->craft_data.magic_smoke);

		net_emit_uint8(conn, entity->craft_data.smoke_start_red);
		net_emit_uint8(conn, entity->craft_data.smoke_start_green);
		net_emit_uint8(conn, entity->craft_data.smoke_start_blue);
	
		net_emit_uint8(conn, entity->craft_data.smoke_end_red);
		net_emit_uint8(conn, entity->craft_data.smoke_end_green);
		net_emit_uint8(conn, entity->craft_data.smoke_end_blue);
	
		net_emit_uint8(conn, entity->craft_data.shield_red);
		net_emit_uint8(conn, entity->craft_data.shield_green);
		net_emit_uint8(conn, entity->craft_data.shield_blue);
	
		net_emit_uint8(conn, entity->craft_data.plasma_red);
		net_emit_uint8(conn, entity->craft_data.plasma_green);
		net_emit_uint8(conn, entity->craft_data.plasma_blue);
	
		net_emit_uint8(conn, entity->craft_data.carcass);
		break;
	
	case ENT_WEAPON:
		write_weapon_data_to_net(conn, entity);
	
		net_emit_uint8(conn, entity->weapon_data.shield_red);
		net_emit_uint8(conn, entity->weapon_data.shield_green);
		net_emit_uint8(conn, entity->weapon_data.shield_blue);
	
		net_emit_uint8(conn, entity->weapon_data.detached);
		break;
	
	case ENT_PLASMA:
		write_plasma_data_to_net(conn, entity);
	
		net_emit_uint8(conn, entity->plasma_data.red);
		net_emit_uint8(conn, entity->plasma_data.green);
		net_emit_uint8(conn, entity->plasma_data.blue);
		break;
	
	case ENT_BULLET:
		write_bullet_data_to_net(conn, entity);
		break;
	
	case ENT_ROCKET:
		write_rocket_data_to_net(conn, entity);
	
		net_emit_uint8(conn, entity->rocket_data.magic_smoke);
	
		net_emit_uint8(conn, entity->rocket_data.smoke_start_red);
		net_emit_uint8(conn, entity->rocket_data.smoke_start_green);
		net_emit_uint8(conn, entity->rocket_data.smoke_start_blue);
	
		net_emit_uint8(conn, entity->rocket_data.smoke_end_red);
		net_emit_uint8(conn, entity->rocket_data.smoke_end_green);
		net_emit_uint8(conn, entity->rocket_data.smoke_end_blue);
		break;
	
	case ENT_MINE:
		write_mine_data_to_net(conn, entity);
		break;
	
	case ENT_RAILS:
		write_rails_data_to_net(conn, entity);
		break;
	
	case ENT_SHIELD:
		write_shield_data_to_net(conn, entity);
		break;
	}
}


void spawn_all_entities_on_player(struct player_t *player)
{
	struct entity_t *centity = entity0;

	while(centity)
	{
		emit_spawn_entity(player->conn, centity);
		centity = centity->next;
	}
	
	net_emit_end_of_stream(player->conn);
}


void spawn_entity_on_all_players(struct entity_t *entity)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			emit_spawn_entity(player->conn, entity);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void remove_entity_from_all_players(struct entity_t *entity)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_KILL_ENT);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, entity->index);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void make_carcass_on_all_players(struct entity_t *craft)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_CARCASS);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, craft->index);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void emit_teleport_to_all_players(struct entity_t *entity)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_TELEPORT);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, entity->index);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void emit_speedup_to_all_players(struct speedup_ramp_t *speedup_ramp)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_SPEEDUP);
			net_emit_uint32(player->conn, game_tick);
			net_emit_float(player->conn, speedup_ramp->x);
			net_emit_float(player->conn, speedup_ramp->y);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void emit_explosion(float xdis, float ydis, float xvel, float yvel, float size, uint8_t magic, 
	uint8_t start_red, uint8_t start_green, uint8_t start_blue,
	uint8_t end_red, uint8_t end_green, uint8_t end_blue)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_EXPLOSION);
			net_emit_uint32(player->conn, game_tick);
			net_emit_float(player->conn, xdis);
			net_emit_float(player->conn, ydis);
			net_emit_float(player->conn, xvel);
			net_emit_float(player->conn, yvel);
			net_emit_float(player->conn, size);
			net_emit_uint8(player->conn, magic);
			net_emit_uint8(player->conn, start_red);
			net_emit_uint8(player->conn, start_green);
			net_emit_uint8(player->conn, start_blue);
			net_emit_uint8(player->conn, end_red);
			net_emit_uint8(player->conn, end_green);
			net_emit_uint8(player->conn, end_blue);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void propagate_player_info()
{
	struct player_t *info_player = player0, *out_player;
		
	while(info_player)
	{
		if(info_player->propagate_info)
		{
			info_player->propagate_info = 0;
			out_player = player0;
			
			while(out_player)
			{
				net_emit_uint8(out_player->conn, EMNETMSG_PLAYER_INFO);
				net_emit_uint32(out_player->conn, info_player->index);
				net_emit_string(out_player->conn, info_player->name->text);
				net_emit_uint8(out_player->conn, info_player->ready);
				net_emit_int(out_player->conn, info_player->frags);
				net_emit_end_of_stream(out_player->conn);
				
				out_player = out_player->next;
			}
		}
		
		info_player = info_player->next;
	}
}


void propagate_player_info_to_all_players(struct player_t *info_player)
{
	struct player_t *out_player = player0;
		
	while(out_player)
	{
		net_emit_uint8(out_player->conn, EMNETMSG_PLAYER_INFO);
		net_emit_uint32(out_player->conn, info_player->index);
		net_emit_string(out_player->conn, info_player->name->text);
		net_emit_uint8(out_player->conn, info_player->ready);
		net_emit_int(out_player->conn, info_player->frags);
		net_emit_end_of_stream(out_player->conn);
		
		out_player = out_player->next;
	}
}


void propagate_all_players_info_to_player(struct player_t *out_player)
{
	struct player_t *info_player = player0;
		
	while(info_player)
	{
		net_emit_uint8(out_player->conn, EMNETMSG_PLAYER_INFO);
		net_emit_uint32(out_player->conn, info_player->index);
		net_emit_string(out_player->conn, info_player->name->text);
		net_emit_uint8(out_player->conn, info_player->ready);
		net_emit_int(out_player->conn, info_player->frags);
		
		info_player = info_player->next;
	}
	
	net_emit_end_of_stream(out_player->conn);
}


void remove_player_info(struct player_t *info_player)
{
	struct player_t *out_player = player0;
		
	while(out_player)
	{
		net_emit_uint8(out_player->conn, EMNETMSG_REMOVE_PLAYER_INFO);
		net_emit_uint32(out_player->conn, info_player->index);
		net_emit_end_of_stream(out_player->conn);
		
		out_player = out_player->next;
	}
}


void follow_me_on_player(struct player_t *player)
{
	net_emit_uint8(player->conn, EMEVENT_FOLLOW_ME);
	net_emit_uint32(player->conn, game_tick);
	net_emit_uint32(player->conn, player->craft->index);
	net_emit_end_of_stream(player->conn);
}


void print_on_player(struct player_t *player, char *text)
{
	net_emit_uint8(player->conn, EMNETMSG_PRINT);
	net_emit_string(player->conn, text);
	net_emit_end_of_stream(player->conn);
}


void print_on_all_players(const char *fmt, ...)
{
	char *msg;
	
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

	struct player_t *player = player0;
		
	while(player)
	{
		print_on_player(player, msg);
		player = player->next;
	}
	
	free(msg);
}


int game_process_status(struct player_t *player, struct buffer_t *buffer)
{
	struct string_t *string = new_string();

	struct player_t *cplayer = player0;

	while(cplayer)
	{
		string_cat_string(string, cplayer->name);
		string_cat_char(string, '\n');

		cplayer = cplayer->next;
	}

	net_emit_uint8(player->conn, EMNETMSG_PRINT);
	net_emit_string(player->conn, string->text);
	net_emit_end_of_stream(player->conn);

	free_string(string);
	
	return 1;
}


void spawn_rocket(struct entity_t *weapon, struct player_t *player)
{
	struct entity_t *rocket = new_entity(&entity0);
	
	rocket->type = ENT_ROCKET;
	
	rocket->xdis = weapon->xdis;
	rocket->ydis = weapon->ydis;
	
	rocket->xvel = weapon->weapon_data.craft->xvel;
	rocket->yvel = weapon->weapon_data.craft->yvel;
	
	rocket->rocket_data.theta = weapon->weapon_data.theta;
	rocket->rocket_data.start_tick = game_tick;
	rocket->rocket_data.in_weapon = 1;
	rocket->rocket_data.weapon_id = weapon->index;
	
	rocket->rocket_data.magic_smoke = weapon->weapon_data.magic_smoke;
	
	rocket->rocket_data.smoke_start_red = weapon->weapon_data.smoke_start_red;
	rocket->rocket_data.smoke_start_green = weapon->weapon_data.smoke_start_green;
	rocket->rocket_data.smoke_start_blue = weapon->weapon_data.smoke_start_blue;
	
	rocket->rocket_data.smoke_end_red = weapon->weapon_data.smoke_end_red;
	rocket->rocket_data.smoke_end_green = weapon->weapon_data.smoke_end_green;
	rocket->rocket_data.smoke_end_blue = weapon->weapon_data.smoke_end_blue;
	
	rocket->rocket_data.owner = player;
	
	spawn_entity_on_all_players(rocket);
}


void kick(char *args)
{
	char *token = strtok(args, " ");
	
	struct player_t *player = get_player_by_name(token);
	if(!player)
	{
		console_print("Kick who?\n");
		return;
	}

	print_on_player(player, "You have been kicked.\n");
	em_disconnect(player->conn);	// unimplemented
	remove_player(player->conn);
	
	struct string_t *s = new_string_text(args);
	string_cat_text(s, " was kicked\n");
	print_on_all_players(s->text);
	free_string(s);
	
	num_players--;
}


void craft_destroyed(struct entity_t *craft)
{
	;
}


void weapon_destroyed(struct entity_t *weapon)
{
	;
}


void craft_telefragged(struct player_t *victim, struct player_t *telefragger)
{
	print_on_all_players("%s was telefragged by %s.\n", victim->name->text, 
		telefragger->name->text);
}


void tick_player(struct player_t *player)
{
//	if(!player->craft && player->respawn_tick == game_tick)
//		spawn_player(player);
	
	// see if we need to send a pulse event
	
	if(player->next_pulse_event == game_tick)
	{
		net_emit_uint8(player->conn, EMEVENT_PULSE);
		net_emit_uint32(player->conn, game_tick);
		net_emit_end_of_stream(player->conn);

		player->next_pulse_event += 200;
	}
}


void update_player_shield_strengths(struct player_t *player)
{
	net_emit_uint8(player->conn, EMEVENT_SHIELD_STRENGTHS);
	net_emit_uint32(player->conn, game_tick);
	
	if(player->craft)
		net_emit_float(player->conn, player->craft->craft_data.shield_strength);
	else
		net_emit_float(player->conn, 0.0f);

	if(player->craft->craft_data.left_weapon)
		net_emit_float(player->conn, 
			player->craft->craft_data.left_weapon->weapon_data.shield_strength);
	else
		net_emit_float(player->conn, 0.0f);
	
	if(player->craft->craft_data.right_weapon)
		net_emit_float(player->conn, 
			player->craft->craft_data.right_weapon->weapon_data.shield_strength);
	else
		net_emit_float(player->conn, 0.0f);
	
	net_emit_end_of_stream(player->conn);
}


void update_player_ammo_levels(struct player_t *player)
{
	net_emit_uint8(player->conn, EMEVENT_AMMO_LEVELS);
	net_emit_uint32(player->conn, game_tick);
	net_emit_int(player->conn, player->rails);
	net_emit_int(player->conn, player->mines);
	
	if(player->craft->craft_data.left_weapon)
		net_emit_int(player->conn, 
			player->craft->craft_data.left_weapon->weapon_data.ammo);
	else
		net_emit_int(player->conn, 0);
	
	if(player->craft->craft_data.right_weapon)
		net_emit_int(player->conn, 
			player->craft->craft_data.right_weapon->weapon_data.ammo);
	else
		net_emit_int(player->conn, 0);
	
	net_emit_end_of_stream(player->conn);
}


void spawn_player(struct player_t *player)
{
	player->rails = 10;
	player->mines = 4;
	
	struct spawn_point_t *spawn_point = spawn_point0;
	
	int respawn_index = 0;
	int num_spawn_points = 0;
	
	while(spawn_point)
	{
		if(!spawn_point->teleport_only)
		{
			num_spawn_points++;
			
			struct entity_t *entity = entity0;
			
			int occluded = 0;
			
			while(entity)
			{
				switch(entity->type)
				{
				case ENT_CRAFT:
					if(circles_intersect(spawn_point->x, spawn_point->y, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, CRAFT_RADIUS))
						occluded = 1;
					break;
					
				case ENT_WEAPON:
					if(circles_intersect(spawn_point->x, spawn_point->y, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, WEAPON_RADIUS))
						occluded = 1;
					break;
					
				case ENT_PLASMA:
					if(circles_intersect(spawn_point->x, spawn_point->y, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, PLASMA_RADIUS))
						occluded = 1;
					break;
					
				case ENT_BULLET:
					if(point_in_circle(entity->xdis, entity->ydis, 
						spawn_point->x, spawn_point->y, CRAFT_RADIUS))
						occluded = 1;
					break;
					
				case ENT_ROCKET:
					if(circles_intersect(spawn_point->x, spawn_point->y, 
						CRAFT_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
						occluded = 1;
					break;
					
				case ENT_MINE:
					if(circles_intersect(spawn_point->x, spawn_point->y, 
						CRAFT_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
						occluded = 1;
					break;
					
				case ENT_RAILS:
					if(circles_intersect(spawn_point->x, spawn_point->y, 
						CRAFT_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
						occluded = 1;
					break;
					
				case ENT_SHIELD:
					if(circles_intersect(spawn_point->x, spawn_point->y, 
						CRAFT_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
						occluded = 1;
					break;
				}

				if(occluded)
					break;
				
				entity = entity->next;
			}
			
			if(!occluded)
			{
				spawn_point->respawn_index = respawn_index++;
			}
			else
			{
				spawn_point->respawn_index = -1;
			}
		}
		
		spawn_point = spawn_point->next;
	}
	
	struct entity_t *old_craft = player->craft;
	
	player->craft = new_entity(&entity0);
	player->craft->type = ENT_CRAFT;
	player->craft->craft_data.shield_strength = 1.0;
	player->craft->craft_data.owner = player;
	
	player->craft->craft_data.magic_smoke = player->magic_smoke;
	player->craft->craft_data.smoke_start_red = player->smoke_start_red;
	player->craft->craft_data.smoke_start_green = player->smoke_start_green;
	player->craft->craft_data.smoke_start_blue = player->smoke_start_blue;
	player->craft->craft_data.smoke_end_red = player->smoke_end_red;
	player->craft->craft_data.smoke_end_green = player->smoke_end_green;
	player->craft->craft_data.smoke_end_blue = player->smoke_end_blue;
	player->craft->craft_data.shield_red = player->shield_red;
	player->craft->craft_data.shield_green = player->shield_green;
	player->craft->craft_data.shield_blue = player->shield_blue;
	player->craft->craft_data.plasma_red = player->plasma_red;
	player->craft->craft_data.plasma_green = player->plasma_green;
	player->craft->craft_data.plasma_blue = player->plasma_blue;
	
	if(old_craft)
	{
		player->craft->craft_data.acc = old_craft->craft_data.acc;
	}
		
	
	if(respawn_index)
	{
		int i = lrand48() % respawn_index;
		
		spawn_point = spawn_point0;
		
		while(spawn_point->respawn_index != i)
			spawn_point = spawn_point->next;
	}
	else
	{
		int i = lrand48() % num_spawn_points;
		
		spawn_point = spawn_point0;
		
		while(i--)
			spawn_point = spawn_point->next;
	}
	
	player->craft->xdis = spawn_point->x;
	player->craft->ydis = spawn_point->y;
	
	spawn_entity_on_all_players(player->craft);
	update_player_shield_strengths(player);
	update_player_ammo_levels(player);
	follow_me_on_player(player);
}


void return_to_lobby()
{
	struct player_t *cplayer = player0;
		
	while(cplayer)
	{
		cplayer->ready = 0;
		cplayer->propagate_info = 1;
		
		net_emit_uint8(cplayer->conn, EMNETMSG_LOBBY);
		net_emit_end_of_stream(cplayer->conn);
		
		if(!cplayer->craft)
		{
			spawn_player(cplayer);
		}
		else
		{
			if(cplayer != winner)
			{
				net_emit_uint8(cplayer->conn, EMEVENT_FOLLOW_ME);
				net_emit_uint32(cplayer->conn, game_tick);
				net_emit_uint32(cplayer->conn, cplayer->craft->index);
				net_emit_end_of_stream(cplayer->conn);
			}
		}
		
		cplayer = cplayer->next;
	}
	
	match_begun = 0;
	match_over = 0;

	propagate_player_info();
	
	console_print("The game has returned to the lobby.\n");
}


void respawn_craft(struct entity_t *craft, struct player_t *responsibility)
{
	if(match_begun && match_over && craft->craft_data.owner == winner)
	{
		craft->craft_data.owner->craft = NULL;
		return_to_lobby();
		return;
	}
	
	if(!match_over)
	{
		if(!responsibility)
			responsibility = craft->craft_data.owner;

		if(responsibility == craft->craft_data.owner)
			responsibility->frags--;
		else
			responsibility->frags++;
	
		responsibility->propagate_info = 1;
		
		spawn_player(craft->craft_data.owner);
	}
	else
	{
		craft->craft_data.owner->craft = NULL;
	}
}


void schedule_respawn(struct pickup_spawn_point_t *spawn_point)
{
	spawn_point->respawn = 1;
	spawn_point->respawn_tick = cgame_tick + spawn_point->respawn_delay / 5;	// terrible
}


struct entity_t *spawn_pickup(struct pickup_spawn_point_t *spawn_point)
{
	struct entity_t *entity;
	
	entity = new_entity(&entity0);
	entity->xdis = spawn_point->x;
	entity->ydis = spawn_point->y;
	entity->teleporting = TELEPORTING_APPEARING;
	entity->teleporting_tick = game_tick;

	switch(spawn_point->type)
	{
	case OBJECTTYPE_PLASMACANNON:
		entity->type = ENT_WEAPON;
		entity->weapon_data.theta = spawn_point->angle;
		entity->weapon_data.type = WEAPON_PLASMA_CANNON;
		entity->weapon_data.ammo = spawn_point->plasma_cannon_data.plasmas;
		entity->weapon_data.shield_strength = 1.0;
		entity->weapon_data.spawn_point = spawn_point;
	
		entity->weapon_data.smoke_start_red = 0xff;
		entity->weapon_data.smoke_start_green = 0xff;
		entity->weapon_data.smoke_start_blue = 0xff;

		entity->weapon_data.smoke_end_red = 0xff;
		entity->weapon_data.smoke_end_green = 0xff;
		entity->weapon_data.smoke_end_blue = 0xff;

		entity->weapon_data.shield_red = 0xff;
		entity->weapon_data.shield_green = 0xff;
		entity->weapon_data.shield_blue = 0xff;
		break;
	
	case OBJECTTYPE_MINIGUN:
		entity->type = ENT_WEAPON;
		entity->weapon_data.theta = spawn_point->angle;
		entity->weapon_data.type = WEAPON_MINIGUN;
		entity->weapon_data.ammo = spawn_point->minigun_data.bullets;
		entity->weapon_data.shield_strength = 1.0;
		entity->weapon_data.spawn_point = spawn_point;
	
		entity->weapon_data.smoke_start_red = 0xff;
		entity->weapon_data.smoke_start_green = 0xff;
		entity->weapon_data.smoke_start_blue = 0xff;

		entity->weapon_data.smoke_end_red = 0xff;
		entity->weapon_data.smoke_end_green = 0xff;
		entity->weapon_data.smoke_end_blue = 0xff;

		entity->weapon_data.shield_red = 0xff;
		entity->weapon_data.shield_green = 0xff;
		entity->weapon_data.shield_blue = 0xff;
		break;
	
	case OBJECTTYPE_ROCKETLAUNCHER:
		entity->type = ENT_WEAPON;
		entity->weapon_data.theta = spawn_point->angle;
		entity->weapon_data.type = WEAPON_ROCKET_LAUNCHER;
		entity->weapon_data.ammo = spawn_point->rocket_launcher_data.rockets;
		entity->weapon_data.shield_strength = 1.0;
		entity->weapon_data.spawn_point = spawn_point;
	
		entity->weapon_data.smoke_start_red = 0xff;
		entity->weapon_data.smoke_start_green = 0xff;
		entity->weapon_data.smoke_start_blue = 0xff;

		entity->weapon_data.smoke_end_red = 0xff;
		entity->weapon_data.smoke_end_green = 0xff;
		entity->weapon_data.smoke_end_blue = 0xff;

		entity->weapon_data.shield_red = 0xff;
		entity->weapon_data.shield_green = 0xff;
		entity->weapon_data.shield_blue = 0xff;
		break;
	
	case OBJECTTYPE_RAILS:
		entity->type = ENT_RAILS;
		entity->rails_data.theta = spawn_point->angle;
		entity->rails_data.quantity = spawn_point->rails_data.quantity;
		entity->rails_data.spawn_point = spawn_point;
		break;
	
	case OBJECTTYPE_SHIELDENERGY:
		entity->type = ENT_SHIELD;
		entity->shield_data.theta = spawn_point->angle;
		entity->shield_data.strength = spawn_point->shield_energy_data.shield_energy;
		entity->shield_data.spawn_point = spawn_point;
		break;
	}
	
	spawn_point->respawn = 0;
	
	return entity;
}


void propagate_colours(struct entity_t *entity)
{
	struct player_t *player = player0;
		
	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_COLOURS);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, entity->index);
			
			net_emit_uint8(player->conn, entity->type);
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				net_emit_uint8(player->conn, entity->craft_data.magic_smoke);
			
				net_emit_uint8(player->conn, entity->craft_data.smoke_start_red);
				net_emit_uint8(player->conn, entity->craft_data.smoke_start_green);
				net_emit_uint8(player->conn, entity->craft_data.smoke_start_blue);
			
				net_emit_uint8(player->conn, entity->craft_data.smoke_end_red);
				net_emit_uint8(player->conn, entity->craft_data.smoke_end_green);
				net_emit_uint8(player->conn, entity->craft_data.smoke_end_blue);
			
				net_emit_uint8(player->conn, entity->craft_data.shield_red);
				net_emit_uint8(player->conn, entity->craft_data.shield_green);
				net_emit_uint8(player->conn, entity->craft_data.shield_blue);
			
				net_emit_uint8(player->conn, entity->craft_data.plasma_red);
				net_emit_uint8(player->conn, entity->craft_data.plasma_green);
				net_emit_uint8(player->conn, entity->craft_data.plasma_blue);
				break;
			
			case ENT_WEAPON:
				net_emit_uint8(player->conn, entity->weapon_data.shield_red);
				net_emit_uint8(player->conn, entity->weapon_data.shield_green);
				net_emit_uint8(player->conn, entity->weapon_data.shield_blue);
				break;
			}
			
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void tick_map()
{
	struct pickup_spawn_point_t *spawn_point = pickup_spawn_point0;
		
	while(spawn_point)
	{
		if(spawn_point->respawn && spawn_point->respawn_tick == game_tick)
		{
			struct entity_t *entity = spawn_pickup(spawn_point);
			
			spawn_entity_on_all_players(entity);
		}
		
		spawn_point = spawn_point->next;
	}
}


void tick_game()
{
	// apply shared tick semantics and associated server-side callbacks
	s_tick_entities(&entity0);
		
	
	struct player_t *player = player0;
	while(player)
	{
		if(player->state == PLAYER_STATE_PLAYING)
			tick_player(player);
		
		player = player->next;
	}
	

	tick_map();
}


void propagate_entities()
{
	struct entity_t *entity = entity0;
		
	while(entity)
	{
		if(entity->propagate_me && entity->type != ENT_BULLET)
		{
			propagate_entity(entity);
			entity->propagate_me = 0;
		}
		
		entity = entity->next;
	}
}


void clear_game()
{
	struct player_t *cplayer = player0;
		
	while(cplayer)
	{
		struct entity_t *craft = cplayer->craft;
		remove_entity_from_all_players(craft);
		remove_entity(&entity0, craft);
		cplayer->craft = NULL;
		cplayer->frags = 0;
		cplayer->propagate_info = 1;
		cplayer = cplayer->next;
	}
	
	
	struct entity_t *centity = entity0;
		
	while(centity)
	{
		remove_entity_from_all_players(centity);
		centity = centity->next;
	}
	
	LL_REMOVE_ALL(struct entity_t, &entity0);
}


void begin_match()
{
	match_start_tick = game_tick;
	
	clear_game();
	

	// spawn objects
	
	struct pickup_spawn_point_t *pickup_spawn_point = pickup_spawn_point0;
		
	while(pickup_spawn_point)
	{
		struct entity_t *entity = spawn_pickup(pickup_spawn_point);
		spawn_entity_on_all_players(entity);
		pickup_spawn_point = pickup_spawn_point->next;
	}


	struct player_t *cplayer = player0;
		
	while(cplayer)
	{
		net_emit_uint8(cplayer->conn, EMNETMSG_MATCH_BEGUN);
		net_emit_uint32(cplayer->conn, match_start_tick);
		net_emit_end_of_stream(cplayer->conn);
		cplayer = cplayer->next;
	}


	cplayer = player0;
		
	while(cplayer)
	{
		spawn_player(cplayer);
		cplayer = cplayer->next;
	}
	
	match_begun = 1;
	match_over = 0;
	
	console_print("The match has begun.\n");
}


void end_match()
{
	assert(player0);
	
	
	// find winner
	
	winner = player0;
	struct player_t *cplayer = player0->next;
	
	while(cplayer)
	{
		if(cplayer->frags > winner->frags)
			winner = cplayer;
		
		cplayer = cplayer->next;
	}
	
	
	cplayer = player0;
	
	while(cplayer)
	{
		if(cplayer != winner)
		{
			if(cplayer->craft->craft_data.left_weapon)
				cplayer->craft->craft_data.left_weapon->weapon_data.firing = 0;

			if(cplayer->craft->craft_data.right_weapon)
				cplayer->craft->craft_data.right_weapon->weapon_data.firing = 0;
			
			cplayer->craft->craft_data.acc = 0;
			cplayer->craft->craft_data.rolling_left = 0;
			cplayer->craft->craft_data.rolling_right = 0;
			
			propagate_entity(cplayer->craft);
		}
			
		cplayer = cplayer->next;
	}


	cplayer = player0;
	
	while(cplayer)
	{
		net_emit_uint8(cplayer->conn, EMNETMSG_MATCH_OVER);
		net_emit_uint32(cplayer->conn, game_tick);
		
		if(cplayer == winner)
			net_emit_uint8(cplayer->conn, WINNER_YOU);
		else
		{
			net_emit_uint8(cplayer->conn, WINNER_INDEX);
			net_emit_uint32(cplayer->conn, winner->index);
				
			net_emit_uint8(cplayer->conn, EMEVENT_FOLLOW_ME);
			net_emit_uint32(cplayer->conn, game_tick);
			net_emit_uint32(cplayer->conn, winner->craft->index);
		}
		
		net_emit_end_of_stream(cplayer->conn);
		cplayer = cplayer->next;
	}
	
	match_over = 1;
	match_end_tick = game_tick;
	
	console_print("The match is over.\n");
	console_print("%s has won.\n", winner->name->text);
}


void update_game()
{
//	if(!(timer_count % 100))
	//	ping_all_clients();
	
	uint32_t tick = get_tick_from_wall_time();
	
	while(game_tick != tick)
	{
		game_tick++;
		cgame_tick = game_tick;
		tick_game();
	}
	
	propagate_entities();
	propagate_player_info();

	if(match_begun && !match_over && (tick - match_start_tick) / 200 >= match_duration * 60)
		end_match();
	
	if(match_begun && match_over && (tick - match_end_tick) / 200 >= 10)
		return_to_lobby();
}


int game_process_colours(struct player_t *player, struct buffer_t *stream)
{
	player->rail_inner_red = buffer_read_uint8(stream);
	player->rail_inner_green = buffer_read_uint8(stream);
	player->rail_inner_blue = buffer_read_uint8(stream);

	player->rail_outer_red = buffer_read_uint8(stream);
	player->rail_outer_green = buffer_read_uint8(stream);
	player->rail_outer_blue = buffer_read_uint8(stream);
	
	player->magic_smoke = buffer_read_uint8(stream);

	player->smoke_start_red = buffer_read_uint8(stream);
	player->smoke_start_green = buffer_read_uint8(stream);
	player->smoke_start_blue = buffer_read_uint8(stream);
	player->smoke_end_red = buffer_read_uint8(stream);
	player->smoke_end_green = buffer_read_uint8(stream);
	player->smoke_end_blue = buffer_read_uint8(stream);

	player->plasma_red = buffer_read_uint8(stream);
	player->plasma_green = buffer_read_uint8(stream);
	player->plasma_blue = buffer_read_uint8(stream);

	player->shield_red = buffer_read_uint8(stream);
	player->shield_green = buffer_read_uint8(stream);
	player->shield_blue = buffer_read_uint8(stream);
	
	if(player->craft)
	{
		player->craft->craft_data.magic_smoke = player->magic_smoke;
		player->craft->craft_data.smoke_start_red = player->smoke_start_red;
		player->craft->craft_data.smoke_start_green = player->smoke_start_green;
		player->craft->craft_data.smoke_start_blue = player->smoke_start_blue;
		player->craft->craft_data.smoke_end_red = player->smoke_end_red;
		player->craft->craft_data.smoke_end_green = player->smoke_end_green;
		player->craft->craft_data.smoke_end_blue = player->smoke_end_blue;
		player->craft->craft_data.shield_red = player->shield_red;
		player->craft->craft_data.shield_green = player->shield_green;
		player->craft->craft_data.shield_blue = player->shield_blue;
		player->craft->craft_data.plasma_red = player->plasma_red;
		player->craft->craft_data.plasma_green = player->plasma_green;
		player->craft->craft_data.plasma_blue = player->plasma_blue;
	}
	
	return 1;
}


void game_process_joined(uint32_t conn)
{
	if(game_state != GS_ALIVE)
	{
		net_emit_uint8(conn, EMNETMSG_PRINT);
		net_emit_string(conn, "This server is not active. Go Away!\n");
		net_emit_end_of_stream(conn);
		em_disconnect(conn);
		return;
	}

	if(match_begun)
	{
		net_emit_uint8(conn, EMNETMSG_PRINT);
		net_emit_string(conn, "There is already an active match on this server.\n");
		net_emit_end_of_stream(conn);
		em_disconnect(conn);
		return;
	}

	struct player_t *player = new_player();
	player->conn = conn;
}


void destroy_player(struct player_t *player)
{
	remove_player_info(player);
	struct entity_t *craft = player->craft;
	remove_player(player->conn);
	
	if(craft)
	{
		remove_entity_from_all_players(craft);
		remove_entity(&entity0, craft);
	}
}


void game_process_play(struct player_t *player)
{
	switch(player->state)
	{
	case PLAYER_STATE_ASLEEP:

		if(num_players >= max_players || num_players >= spawn_points || match_begun)
		{
		//	if(num_spectators >= max_spectators)
			{
				net_emit_uint8(player->conn, EMNETMSG_PRINT);
				net_emit_string(player->conn, "No room!\n");
				net_emit_end_of_stream(player->conn);
				em_disconnect(player->conn);
				return;
			}
			
		//	player->state = PLAYER_STATE_SPECTATING;
		//	num_spectators++;
		}
		
		player->state = PLAYER_STATE_PLAYING;
		propagate_player_info_to_all_players(player);
		propagate_all_players_info_to_player(player);
		num_players++;
		break;
	
	case PLAYER_STATE_PLAYING:
		return;
	
	case PLAYER_STATE_SPECTATING:
		
		if(num_players >= max_players || num_players >= spawn_points)
		{
			net_emit_uint8(player->conn, EMNETMSG_PRINT);
			net_emit_string(player->conn, "No room!\n");
			net_emit_end_of_stream(player->conn);
			return;
		}
		
		player->state = PLAYER_STATE_PLAYING;
		propagate_player_info_to_all_players(player);
		propagate_all_players_info_to_player(player);
		num_spectators--;
		num_players++;
		break;
	}
	
	
	struct string_t *s = new_string_string(player->name);

	if(player->state == PLAYER_STATE_PLAYING)
		string_cat_text(s, " joined the game\n");
	else
		string_cat_text(s, " joined the spectators\n");
		
	console_print(s->text);
	print_on_all_players(s->text);
	free_string(s);
	
	
//	print_on_player(player, "Welcome to this server.\n");
	
	load_map_on_player(player);
	net_emit_end_of_stream(player->conn);	// this is important if the client 
											// needs to download the map

	load_all_skins_on_player(player);
	
	if(player->state == PLAYER_STATE_PLAYING)
		net_emit_uint8(player->conn, EMMSG_PLAYING);
	else
		net_emit_uint8(player->conn, EMMSG_SPECTATING);
	
	net_emit_uint32(player->conn, game_tick);
	net_emit_end_of_stream(player->conn);
}


void game_process_spectate(struct player_t *player)
{
	switch(player->state)
	{
	case PLAYER_STATE_ASLEEP:
	//	if(num_spectators >= max_spectators)
		{
			net_emit_uint8(player->conn, EMNETMSG_PRINT);
			net_emit_string(player->conn, "No room!\n");
			net_emit_end_of_stream(player->conn);
			em_disconnect(player->conn);
			return;
		}
		
	//	player->state = PLAYER_STATE_SPECTATING;
	//	num_spectators++;
		break;
	
	case PLAYER_STATE_PLAYING:
	//	if(num_spectators >= max_spectators)
		{
			net_emit_uint8(player->conn, EMNETMSG_PRINT);
			net_emit_string(player->conn, "No room!\n");
			net_emit_end_of_stream(player->conn);
			return;
		}
		
	//	destroy_player(player);
	//	player->state = PLAYER_STATE_SPECTATING;
	//	num_players--;
	//	num_spectators++;
		break;
	
	case PLAYER_STATE_SPECTATING:
		return;
	}
	
	
	struct string_t *s = new_string_string(player->name);
	string_cat_text(s, " started spectating\n");
	print_on_all_players(s->text);
	console_print(s->text);
	free_string(s);
	
	load_map_on_player(player);
	load_all_skins_on_player(player);
	net_emit_uint8(player->conn, EMMSG_SPECTATING);
	net_emit_end_of_stream(player->conn);
}


void game_process_start(struct player_t *player)
{
	if(player->state == PLAYER_STATE_ASLEEP)
		return;

//	player->last_control_change = index;
	player->next_pulse_event = game_tick + 200;

	spawn_all_entities_on_player(player);
	
	player->propagate_events = 1;

	if(player->state == PLAYER_STATE_PLAYING)
		spawn_player(player);
}


void game_process_ready(struct player_t *player, struct buffer_t *stream)
{
	if(match_begun)
	{
		if(match_over)
			return_to_lobby();
		else
			return;
	}
	
	if(player->state != PLAYER_STATE_PLAYING)
		return;
	
	player->ready = buffer_read_uint8(stream);

	int all_ready = 1;
	struct player_t *cplayer = player0;
		
	while(cplayer)
	{
		if(!cplayer->ready)
		{
			all_ready = 0;
			break;
		}
		
		cplayer = cplayer->next;
	}
	
	if(all_ready)
		begin_match();
	else
		player->propagate_info = 1;
}


void game_process_disconnection(uint32_t conn)
{
	struct player_t *player = get_player_by_conn(conn);
	if(!player)
		return;
	
	switch(player->state)
	{
	case PLAYER_STATE_PLAYING:
		num_players--;
		break;
	
	case PLAYER_STATE_SPECTATING:
		num_spectators--;
		break;
	}
	
	struct string_t *s = new_string_string(player->name);
	string_cat_text(s, " left\n");
	destroy_player(player);
	
	console_print(s->text);
	print_on_all_players(s->text);
	
	if(num_players == 0 && match_begun)
		return_to_lobby();
}


void game_process_conn_lost(uint32_t conn)
{
	struct player_t *player = get_player_by_conn(conn);
	if(!player)
		return;
	
	switch(player->state)
	{
	case PLAYER_STATE_PLAYING:
		num_players--;
		break;
	
	case PLAYER_STATE_SPECTATING:
		num_spectators--;
		break;
	}
	
	struct string_t *s = new_string_string(player->name);
	string_cat_text(s, " lost connectivity\n");
	destroy_player(player);
	
	console_print(s->text);
	print_on_all_players(s->text);
	
	if(num_players == 0 && match_begun)
		return_to_lobby();
}


int game_process_say(struct player_t *player, struct buffer_t *stream)
{
	struct string_t *s = new_string_string(player->name);
	string_cat_text(s, ": ");
	struct string_t *says = buffer_read_string(stream);
	string_cat_string(s, says);	
	free_string(says);
	string_cat_text(s, "\n");
	
	console_print(s->text);
	print_on_all_players(s->text);
	free_string(s);
	return 1;
}


int game_process_name_change(struct player_t *player, struct buffer_t *stream)
{
	struct string_t *s = new_string_string(player->name);
		
	if(!player->name)
	{
		player->name = buffer_read_string(stream);
	}
	else
	{		
		free_string(player->name);
		player->name = buffer_read_string(stream);
	
		string_cat_text(s, " is now ");
		string_cat_string(s, player->name);
		string_cat_text(s, "\n");
	
		console_print(s->text);
		print_on_all_players(s->text);
		free_string(s);
	}
	
	player->propagate_info = 1;
	return 1;
}


int game_process_suicide(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	struct entity_t *old_craft = player->craft;
	respawn_craft(old_craft, player);
	explode_craft(old_craft, player);
	remove_entity(&entity0, old_craft);
	
	struct string_t *s = new_string_string(player->name);
		
	string_cat_text(s, " decided to end it all.\n");
	console_print(s->text);
	print_on_all_players(s->text);
	free_string(s);
	return 1;
}


int game_process_thrust(struct player_t *player, struct buffer_t *stream)
{
	float thrust = buffer_read_float(stream);

	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	thrust = max(thrust, 0.0);
	thrust = min(thrust, 1.0);
	
	player->craft->craft_data.acc = thrust * 0.025;
	
	propagate_entity(player->craft);
	
	return 1;
}


int game_process_brake(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	player->craft->craft_data.braking = 1;
	propagate_entity(player->craft);
	return 1;
}


int game_process_no_brake(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	player->craft->craft_data.braking = 0;
	propagate_entity(player->craft);
	return 1;
}


int game_process_roll(struct player_t *player, struct buffer_t *stream)
{
/*	if(index <= player->last_control_change)
		index += EMNETINDEX_MAX + 1;
	
	if(!(index < player->last_control_change + 100))
		return 0;

	player->last_control_change = index;
*/
	
	if(match_over && player != winner)
		return 0;
	
	if(!player->craft)
		return 0;
	
	if(!player->craft->teleporting)
	{
		float roll = buffer_read_float(stream);
		
	//	roll = max(roll, -1.0);
	//	roll = min(roll, 1.0);
		player->craft->craft_data.theta += -roll * 0.015;
		
		propagate_entity(player->craft);
		
		
		// dirty hack to fix inconsistencies
		
	//	if(player->craft->craft_data.left_weapon)
	//		propagate_entity(player->craft->craft_data.left_weapon);
		
	//	if(player->craft->craft_data.right_weapon)
	//		propagate_entity(player->craft->craft_data.right_weapon);
	}
	
	return 0;
}


int game_process_roll_left(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	player->craft->craft_data.rolling_left = 1;
	propagate_entity(player->craft);
	return 1;
}


int game_process_no_roll_left(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	player->craft->craft_data.rolling_left = 0;
	propagate_entity(player->craft);
	return 1;
}


int game_process_roll_right(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	player->craft->craft_data.rolling_right = 1;
	propagate_entity(player->craft);
	return 1;
}


int game_process_no_roll_right(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	player->craft->craft_data.rolling_right = 0;
	propagate_entity(player->craft);
	return 1;
}


void propagate_rail_trail(float x1, float y1, float x2, float y2, 
	struct player_t *firer)
{
	struct player_t *player = player0;

	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_RAILTRAIL);
			net_emit_uint32(player->conn, game_tick);
			net_emit_float(player->conn, x1);
			net_emit_float(player->conn, y1);
			net_emit_float(player->conn, x2);
			net_emit_float(player->conn, y2);
			
			net_emit_uint8(player->conn, firer->rail_inner_red);
			net_emit_uint8(player->conn, firer->rail_inner_green);
			net_emit_uint8(player->conn, firer->rail_inner_blue);
			net_emit_uint8(player->conn, firer->rail_outer_red);
			net_emit_uint8(player->conn, firer->rail_outer_green);
			net_emit_uint8(player->conn, firer->rail_outer_blue);
			
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


struct rail_entity_t
{
	struct entity_t *entity;
	float dist;
	float x, y;
	
	struct rail_entity_t *next;
		
} *rail_entity0;


void add_rail_entity(struct entity_t *entity, float dist, float x, float y)
{
	struct rail_entity_t *crail_entity, *temp;


	if(!rail_entity0)
	{
		rail_entity0 = malloc(sizeof(struct rail_entity_t));
		rail_entity0->entity = entity;
		rail_entity0->dist = dist;
		rail_entity0->x = x;
		rail_entity0->y = y;
		rail_entity0->next = NULL;
		return;
	}

	
	if(rail_entity0->dist > dist)
	{
		temp = rail_entity0;
		rail_entity0 = malloc(sizeof(struct rail_entity_t));
		rail_entity0->entity = entity;
		rail_entity0->dist = dist;
		rail_entity0->x = x;
		rail_entity0->y = y;
		rail_entity0->next = temp;
		return;
	}

	
	crail_entity = rail_entity0;
	
	while(crail_entity->next)
	{
		if(crail_entity->next->dist > dist)
		{
			temp = crail_entity->next;
			crail_entity->next = malloc(sizeof(struct rail_entity_t));
			crail_entity = crail_entity->next;
			crail_entity->entity = entity;
			crail_entity->dist = dist;
			crail_entity->x = x;
			crail_entity->y = y;
			crail_entity->next = temp;
			return;
		}

		crail_entity = crail_entity->next;
	}


	crail_entity->next = malloc(sizeof(struct rail_entity_t));
	crail_entity = crail_entity->next;
	crail_entity->entity = entity;
	crail_entity->dist = dist;
	crail_entity->x = x;
	crail_entity->y = y;
	crail_entity->next = NULL;
}


int game_process_fire_rail(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	if(!player->rails)
		return 1;
	
	player->rails--;
	update_player_ammo_levels(player);
	
	double sin_theta, cos_theta;
	sincos(player->craft->craft_data.theta, &sin_theta, &cos_theta);
	
	player->craft->xvel += 0.8 * sin_theta;
	player->craft->yvel -= 0.8 * cos_theta;
	
	propagate_entity(player->craft);
	
	float x1 = player->craft->xdis - CRAFT_RADIUS * sin_theta;
	float y1 = player->craft->ydis + CRAFT_RADIUS * cos_theta;
	float x2 = player->craft->xdis - (CRAFT_RADIUS + 4000) * sin_theta;
	float y2 = player->craft->ydis + (CRAFT_RADIUS + 4000) * cos_theta;
	
	rail_walk_bsp(x1, y1, x2, y2, &x2, &y2);
	
	struct entity_t *centity = entity0;
		
	while(centity)
	{
		if(centity == player->craft)
		{
			centity = centity->next;
			continue;
		}
		
		double radius;
		
		switch(centity->type)
		{
		case ENT_CRAFT:		radius = CRAFT_RADIUS;	break;
		case ENT_WEAPON:	radius = WEAPON_RADIUS;	break;
		case ENT_PLASMA:	radius = PLASMA_RADIUS;	break;
		case ENT_ROCKET:	radius = ROCKET_RADIUS;	break;
		case ENT_MINE:		radius = MINE_RADIUS;	break;
		case ENT_RAILS:		radius = RAILS_RADIUS;	break;
		case ENT_SHIELD:	radius = SHIELD_RADIUS;	break;
		default:			radius = 0.0;			break;
		}
		
		if(radius == 0.0)
		{
			centity = centity->next;
			continue;
		}
		
		float x, y;
		
		if(line_in_circle_with_coords(x1, y1, x2, y2, centity->xdis, centity->ydis, 
			radius, &x, &y))
		{
			float dist = hypot(x - x1, y - y1);
			add_rail_entity(centity, dist, x, y);
		}
		
		centity = centity->next;
	}
	
	struct rail_entity_t *crail_entity = rail_entity0;
	int kills = 0;
	
	while(crail_entity)
	{
		int destroyed = 0;
		
		switch(crail_entity->entity->type)
		{
		case ENT_CRAFT:
			destroyed = craft_force(crail_entity->entity, RAIL_DAMAGE, player);
			if(destroyed)
				kills++;
			else
				propagate_entity(crail_entity->entity);
			break;
		
		case ENT_WEAPON:
			destroyed = weapon_force(crail_entity->entity, RAIL_DAMAGE, player);
			if(!destroyed)
				propagate_entity(crail_entity->entity);
			break;
		
		case ENT_PLASMA:
			destroyed = plasma_rail_hit(crail_entity->entity);
			break;
		
		case ENT_ROCKET:
			destroyed = rocket_force(crail_entity->entity, RAIL_DAMAGE);
			break;
		
		case ENT_MINE:
			destroyed = mine_force(crail_entity->entity, RAIL_DAMAGE);
			break;
		
		case ENT_RAILS:
			destroyed = rails_force(crail_entity->entity, RAIL_DAMAGE, player);
			break;
		
		case ENT_SHIELD:
			destroyed = shield_force(crail_entity->entity, RAIL_DAMAGE);
			break;
		}
		
		if(!destroyed)
			break;
		
		crail_entity = crail_entity->next;
	}
	
	LL_REMOVE_ALL(struct rail_entity_t, &rail_entity0);
		
	
	// calculate extra frags for multiple kills
	// bear in mind that we have already got one frag per kill
	
	if(!match_over)
	{
		while(kills--)
			player->frags += kills;
	}

	
	if(crail_entity)
		propagate_rail_trail(x1, y1, crail_entity->x, crail_entity->y, player);
	else
		propagate_rail_trail(x1, y1, x2, y2, player);
	
	player->propagate_info = 1;
	
	propagate_player_info();

	return 1;
}


void respawn_weapon(struct entity_t *weapon)
{
	if(weapon->weapon_data.respawned)
		return;
	
	schedule_respawn(weapon->weapon_data.spawn_point);
	weapon->weapon_data.respawned = 1;
}


void detach_weapon(struct entity_t *weapon)
{
	weapon->weapon_data.detached = 1;
	weapon->weapon_data.firing = 0;
	
	weapon->xvel = weapon->weapon_data.craft->xvel;
	weapon->yvel = weapon->weapon_data.craft->yvel;
	
	if(weapon->weapon_data.craft->craft_data.left_weapon == weapon)
		weapon->weapon_data.craft->craft_data.left_weapon = NULL;
	else
		weapon->weapon_data.craft->craft_data.right_weapon = NULL;
	
	weapon->weapon_data.shield_strength = min(weapon->weapon_data.shield_strength, 0.035);
	weapon->weapon_data.craft = NULL;
	
	respawn_weapon(weapon);
	
	
	struct player_t *player = player0;

	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_DETACH);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, weapon->index);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void propagate_weapon_start_firing(struct entity_t *entity)
{
	struct player_t *player = player0;

	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_WEAPON_START_FIRING);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, entity->index);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


void propagate_weapon_stop_firing(struct entity_t *entity)
{
	struct player_t *player = player0;

	while(player)
	{
		if(player->propagate_events)
		{
			net_emit_uint8(player->conn, EMEVENT_WEAPON_STOP_FIRING);
			net_emit_uint32(player->conn, game_tick);
			net_emit_uint32(player->conn, entity->index);
			net_emit_end_of_stream(player->conn);
		}
		
		player = player->next;
	}
}


int game_process_fire_left(struct player_t *player, struct buffer_t *stream)
{
	if(match_over && player != winner)
		return 0;
	
	if(!player->craft)
		return 0;
	
	if(!player->craft->craft_data.left_weapon)
		return 0;
	
	uint8_t state = buffer_read_uint8(stream);
	
	if(state && !player->craft->craft_data.left_weapon->weapon_data.ammo)
	{
		detach_weapon(player->craft->craft_data.left_weapon);
		return 1;
	}
	
	switch(player->craft->craft_data.left_weapon->weapon_data.type)
	{
	case WEAPON_PLASMA_CANNON:
	case WEAPON_MINIGUN:
		
		if(player->craft->craft_data.left_weapon->weapon_data.firing == state)
			break;

		player->craft->craft_data.left_weapon->weapon_data.firing = state;
	
		if(state)
		{
			if(player->craft->craft_data.left_weapon->weapon_data.ammo)
			{
				player->craft->craft_data.left_weapon->weapon_data.firing_start = game_tick + 1;
				player->craft->craft_data.left_weapon->weapon_data.fired = 0;
				propagate_weapon_start_firing(player->craft->craft_data.left_weapon);
			}
		}
		else
		{
			propagate_weapon_stop_firing(player->craft->craft_data.left_weapon);
		}

		break;
	
	case WEAPON_ROCKET_LAUNCHER:
		
		if(state && player->craft->craft_data.left_weapon->weapon_data.ammo)
		{
			spawn_rocket(player->craft->craft_data.left_weapon, player);
			player->craft->craft_data.left_weapon->weapon_data.ammo--;
			update_player_ammo_levels(player);
		}
		
		break;
	}
	
	return 1;
}


int game_process_fire_right(struct player_t *player, struct buffer_t *stream)
{
	if(match_over && player != winner)
		return 0;
	
	if(!player->craft)
		return 0;
	
	if(!player->craft->craft_data.right_weapon)
		return 0;
	
	uint8_t state = buffer_read_uint8(stream);
	
	if(state && !player->craft->craft_data.right_weapon->weapon_data.ammo)
	{
		detach_weapon(player->craft->craft_data.right_weapon);
		return 1;
	}
	
	switch(player->craft->craft_data.right_weapon->weapon_data.type)
	{
	case WEAPON_PLASMA_CANNON:
	case WEAPON_MINIGUN:
		
		if(player->craft->craft_data.right_weapon->weapon_data.firing == state)
			break;
		
		player->craft->craft_data.right_weapon->weapon_data.firing = state;
	
		if(state)
		{
			if(player->craft->craft_data.right_weapon->weapon_data.ammo)
			{
				player->craft->craft_data.right_weapon->weapon_data.firing_start = game_tick + 1;
				player->craft->craft_data.right_weapon->weapon_data.fired = 0;
				propagate_weapon_start_firing(player->craft->craft_data.right_weapon);
			}
		}
		else
		{
			propagate_weapon_stop_firing(player->craft->craft_data.right_weapon);
		}

		break;
	
	case WEAPON_ROCKET_LAUNCHER:
		
		if(state && player->craft->craft_data.right_weapon->weapon_data.ammo)
		{
			spawn_rocket(player->craft->craft_data.right_weapon, player);
			player->craft->craft_data.right_weapon->weapon_data.ammo--;
			update_player_ammo_levels(player);
		}
		
		break;
	}
	
	return 1;
}


int game_process_drop_mine(struct player_t *player)
{
	if(match_over && player != winner)
		return 1;
	
	if(!player->craft)
		return 1;
	
	if(!player->mines)
		return 1;
	
	player->mines--;
	update_player_ammo_levels(player);
	
	struct entity_t *mine = new_entity(&entity0);
	
	mine->type = ENT_MINE;
	
	mine->xdis = player->craft->xdis;
	mine->ydis = player->craft->ydis;
	
	mine->xvel = player->craft->xvel;
	mine->yvel = player->craft->yvel;
	
	mine->mine_data.under_craft = 1;
	mine->mine_data.craft_id = player->craft->index;
	
	mine->mine_data.owner = player;
	
	mine->mine_data.magic_smoke = player->craft->craft_data.magic_smoke;
	
	mine->mine_data.smoke_start_red = player->craft->craft_data.smoke_start_red;
	mine->mine_data.smoke_start_green = player->craft->craft_data.smoke_start_green;
	mine->mine_data.smoke_start_blue = player->craft->craft_data.smoke_start_blue;

	mine->mine_data.smoke_end_red = player->craft->craft_data.smoke_end_red;
	mine->mine_data.smoke_end_green = player->craft->craft_data.smoke_end_green;
	mine->mine_data.smoke_end_blue = player->craft->craft_data.smoke_end_blue;

	spawn_entity_on_all_players(mine);
	
	return 1;
}


void game_process_stream(uint32_t conn, uint32_t index, struct buffer_t *stream)
{
	update_game();
	
	struct player_t *player = get_player_by_conn(conn);
	assert(player);

	while(buffer_more(stream))
	{		
		switch(buffer_read_uint8(stream))
		{
		case EMMSG_PLAY:
			game_process_play(player);
			break;
		
		case EMMSG_SPECTATE:
			game_process_spectate(player);
			break;
		
		case EMMSG_STATUS:
			if(!game_process_status(player, stream))
				return;
			break;
		
		case EMMSG_START:
			game_process_start(player);
			break;
		
		case EMMSG_SAY:
			if(!game_process_say(player, stream))
				return;
			break;
		
		case EMMSG_READY:
			game_process_ready(player, stream);
			break;
		
		case EMMSG_THRUST:
			if(!game_process_thrust(player, stream))
				return;
			break;
		
		case EMMSG_BRAKE:
			if(!game_process_brake(player))
				return;
			break;
		
		case EMMSG_NOBRAKE:
			if(!game_process_no_brake(player))
				return;
			break;
		
		case EMMSG_ROLL:
			if(!game_process_roll(player, stream))
				return;
			break;
		
		case EMMSG_ROLL_LEFT:
			if(!game_process_roll_left(player))
				return;
			break;
		
		case EMMSG_NOROLL_LEFT:
			if(!game_process_no_roll_left(player))
				return;
			break;
		
		case EMMSG_ROLL_RIGHT:
			if(!game_process_roll_right(player))
				return;
			break;
		
		case EMMSG_NOROLL_RIGHT:
			if(!game_process_no_roll_right(player))
				return;
			break;
		
		case EMMSG_FIRERAIL:
			if(!game_process_fire_rail(player))
				return;
			break;
		
		case EMMSG_FIRELEFT:
			if(!game_process_fire_left(player, stream))
				return;
			break;
		
		case EMMSG_FIRERIGHT:
			if(!game_process_fire_right(player, stream))
				return;
			break;
		
		case EMMSG_DROPMINE:
			if(!game_process_drop_mine(player))
				return;
			break;
		
		case EMMSG_ENTERRCON:
			break;
		
		case EMMSG_LEAVERCON:
			break;
		
		case EMMSG_RCONMSG:
			break;
		
		case EMMSG_NAMECNGE:
			if(!game_process_name_change(player, stream))
				return;
			break;

		case EMMSG_SUICIDE:
			if(!game_process_suicide(player))
				return;
			break;

		case EMMSG_COLOURS:
			game_process_colours(player, stream);
			if(player->craft)
				propagate_colours(player->craft);
			break;

		default:
			return;
		}
	}
}


void game_process_stream_ooo(uint32_t conn, uint32_t index, struct buffer_t *stream)
{
	update_game();
	
	struct player_t *player = get_player_by_conn(conn);
	assert(player);

	while(buffer_more(stream))
	{		
		switch(buffer_read_uint8(stream))
		{
		default:
			return;
		}
	}
}


void game_process_stream_timed(uint32_t conn, uint32_t index, uint64_t *stamp, struct buffer_t *stream)
{
	game_process_stream(conn, index, stream);
}


void game_process_stream_untimed(uint32_t conn, uint32_t index, struct buffer_t *stream)
{
	game_process_stream(conn, index, stream);
}


void game_process_stream_timed_ooo(uint32_t conn, uint32_t index, uint64_t *stamp, struct buffer_t *stream)
{
	game_process_stream_ooo(conn, index, stream);
}


void game_process_stream_untimed_ooo(uint32_t conn, uint32_t index, struct buffer_t *stream)
{
	game_process_stream_ooo(conn, index, stream);
}


int read_plasma_cannon(gzFile file)
{
	struct pickup_spawn_point_t pickup_spawn_point;
	
	pickup_spawn_point.type = OBJECTTYPE_PLASMACANNON;
	
	if(gzread(file, &pickup_spawn_point.x, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.y, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.plasma_cannon_data.plasmas, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.angle, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.respawn_delay, 4) != 4)
		goto error;
	
	LL_ADD(struct pickup_spawn_point_t, &pickup_spawn_point0, &pickup_spawn_point);
	
	return 1;
	
error:
	
	return 0;
}


int read_minigun(gzFile file)
{
	struct pickup_spawn_point_t pickup_spawn_point;
	
	pickup_spawn_point.type = OBJECTTYPE_MINIGUN;
	
	if(gzread(file, &pickup_spawn_point.x, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.y, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.minigun_data.bullets, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.angle, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.respawn_delay, 4) != 4)
		goto error;
	
	LL_ADD(struct pickup_spawn_point_t, &pickup_spawn_point0, &pickup_spawn_point);
	
	return 1;
	
error:
	
	return 0;
}


int read_rocket_launcher(gzFile file)
{
	struct pickup_spawn_point_t pickup_spawn_point;
	
	pickup_spawn_point.type = OBJECTTYPE_ROCKETLAUNCHER;
	
	if(gzread(file, &pickup_spawn_point.x, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.y, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.rocket_launcher_data.rockets, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.angle, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.respawn_delay, 4) != 4)
		goto error;
	
	LL_ADD(struct pickup_spawn_point_t, &pickup_spawn_point0, &pickup_spawn_point);
	
	return 1;
	
error:
	
	return 0;
}


int read_rails(gzFile file)
{
	struct pickup_spawn_point_t pickup_spawn_point;
	
	pickup_spawn_point.type = OBJECTTYPE_RAILS;
	
	if(gzread(file, &pickup_spawn_point.x, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.y, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.rails_data.quantity, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.angle, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.respawn_delay, 4) != 4)
		goto error;
	
	LL_ADD(struct pickup_spawn_point_t, &pickup_spawn_point0, &pickup_spawn_point);
	
	return 1;
	
error:
	
	return 0;
}


int read_shield_energy(gzFile file)
{
	struct pickup_spawn_point_t pickup_spawn_point;
	
	pickup_spawn_point.type = OBJECTTYPE_SHIELDENERGY;
	
	if(gzread(file, &pickup_spawn_point.x, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.y, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.shield_energy_data.shield_energy, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.angle, 4) != 4)
		goto error;
	
	if(gzread(file, &pickup_spawn_point.respawn_delay, 4) != 4)
		goto error;
	
	LL_ADD(struct pickup_spawn_point_t, &pickup_spawn_point0, &pickup_spawn_point);
	
	return 1;
	
error:
	
	return 0;
}


int read_object(gzFile file)
{
	uint8_t type;

	if(gzread(file, &type, 1) != 1)
		goto error;
	
	switch(type)
	{
		case OBJECTTYPE_PLASMACANNON:
			if(!read_plasma_cannon(file))
				goto error;
			break;
		
		case OBJECTTYPE_MINIGUN:
			if(!read_minigun(file))
				goto error;
			break;
		
		case OBJECTTYPE_ROCKETLAUNCHER:
			if(!read_rocket_launcher(file))
				goto error;
			break;
		
		case OBJECTTYPE_RAILS:
			if(!read_rails(file))
				goto error;
			break;
		
		case OBJECTTYPE_SHIELDENERGY:
			if(!read_shield_energy(file))
				goto error;
			break;
		
		case OBJECTTYPE_SPAWNPOINT:
			if(!read_spawn_point(file))
				goto error;
			spawn_points++;
			break;
			
		case OBJECTTYPE_SPEEDUPRAMP:
			if(!read_speedup_ramp(file))
				goto error;
			break;
			
		case OBJECTTYPE_TELEPORTER:
			if(!read_teleporter(file))
				goto error;
			break;
			
		case OBJECTTYPE_GRAVITYWELL:
			if(!read_gravity_well(file))
				goto error;
			break;
	}

	return 1;
	
error:
	
	return 0;
}


void map(char *args)
{
	spawn_points = 0;
	
	if(game_state == GS_ALIVE)
	{
		struct player_t *player = player0, *next;
			
		while(player)
		{
			em_disconnect(player->conn);
			next = player->next;
			destroy_player(player);
			num_players--;
			player = next;
		}
			
		clear_game();
		LL_REMOVE_ALL(struct pickup_spawn_point_t, &pickup_spawn_point0);
		LL_REMOVE_ALL(struct spawn_point_t, &spawn_point0);
		LL_REMOVE_ALL(struct teleporter_t, &teleporter0);
		LL_REMOVE_ALL(struct speedup_ramp_t, &speedup_ramp0);
		LL_REMOVE_ALL(struct gravity_well_t, &gravity_well0);
	}
	
	mapname = new_string_text(args);
	
	struct string_t *filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/maps/");
	string_cat_text(filename, args);
	string_cat_text(filename, ".cmap");
	
	
	if(!get_file_info(filename->text, &map_size, map_hash))
	{
		free_string(filename);
		struct string_t *temp = new_string_text("stock-maps/");
		string_cat_text(temp, args);
		string_cat_text(temp, ".cmap");
		
		filename = new_string_text(find_resource(temp->text));
		free_string(temp);
		
		if(!get_file_info(filename->text, &map_size, map_hash))
		{
			console_print("Could not load map: %s\n", args);
			free_string(filename);
			return;
		}
	}

	gzFile file = gzopen(filename->text, "rb");
	
	console_print(filename->text);
	console_print("\n");
	
	free_string(filename);
	
	uint16_t format_id;
	
	if(gzread(file, &format_id, 2) != 2)
		goto error;
	
	if(format_id != EMERGENCE_COMPILEDFORMATID)
		goto error;
	
	if(!load_bsp_tree(file))
		goto error;
	
	uint32_t num_objects;
	
	if(gzread(file, &num_objects, 4) != 4)
		goto error;
	
	int o;
	for(o = 0; o < num_objects; o++)
	{
		if(!read_object(file))
		goto error;
	}
	
	gzclose(file);
	
	
	// spawn objects
	
	struct pickup_spawn_point_t *pickup_spawn_point = pickup_spawn_point0;
		
	while(pickup_spawn_point)
	{
		spawn_pickup(pickup_spawn_point);
		
		pickup_spawn_point = pickup_spawn_point->next;
	}
	
	
	game_state = GS_ALIVE;
	console_print("Map loaded.\n");
	return;

error:
	
	gzclose(file);
	console_print("Map file is corrupt!\n");
}


void status()
{
	;
}


void leave()
{
	;
}


void init_game()
{
	create_cvar_string("password", "", 0);
	create_cvar_string("admin_password", "", 0);
	create_cvar_int("num_players", &num_players, CVAR_PROTECTED);
	create_cvar_int("num_spectators", &num_spectators, CVAR_PROTECTED);
	create_cvar_int("max_players", &max_players, 0);
	create_cvar_int("max_spectators", &max_spectators, 0);
	create_cvar_int("match_duration", &match_duration, 0);
	create_cvar_command("status", status);
	create_cvar_command("map", map);
	create_cvar_command("leave", leave);
	create_cvar_command("kick", kick);
	
	map_filename = new_string();
}


void kill_game()
{
	;
}

















/*
int process_enterrcon(struct player_t *player, struct buffer_t *buffer)
{
	if(player->in_rcon)
		return;

	player->in_rcon = 1;

//	net_write_int(EMNETMSG_INRCON);
//	net_finished_writing(player->conn);
}


int process_leavercon(struct player_t *player, struct buffer_t *buffer)
{
	if(!player->in_rcon)
		return;

	player->in_rcon = 0;

//	net_write_int(EMNETMSG_NOTINRCON);
//	net_finished_writing(player->conn);
}


int process_rconmsg(struct player_t *player, struct buffer_t *buffer)
{
	if(!player->in_rcon)
		return;

	console_rcon = player;

	struct string_t *string = buffer_read_string(buffer);

	parse_command(string->text);

	free(string);

	console_rcon = NULL;
}



void leave(char *args)
{
	if(console_rcon)
	{
//		net_write_int(EMNETMSG_NOTINRCON);
//		net_finished_writing(console_rcon->conn);
		console_rcon->in_rcon = 0;
	}
	else
	{
		console_print("you are not in rcon!\n");
	}
}



*/
