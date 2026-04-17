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
#include <stdint.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include <zlib.h>

#include <execinfo.h>

#include "types.h"
#include "minmax.h"
#include "llist.h"
#include "stringbuf.h"
#include "buffer.h"
#include "bsp.h"
#include "objects.h"


#ifdef EMSERVER
#include "sgame.h"
#include "game.h"
#include "console.h"
#endif

#ifdef EMCLIENT
#include "particles.h"
#include "sgame.h"
#endif


#ifdef EMCLIENT
#include "game.h"
#include "console.h"
#include "sound.h"
#endif

struct spawn_point_t *spawn_point0 = NULL;
struct teleporter_t *teleporter0 = NULL;
struct speedup_ramp_t *speedup_ramp0 = NULL;
struct gravity_well_t *gravity_well0 = NULL;

struct entity_t **sentity0 = NULL;
	
int nextentity = 0;

#define SPACE_FRICTION		0.99935
#define BRAKE_FRICTION		0.98
#define COLLISION_FRICTION	0.75


#define ROCKET_FORCE_THRESHOLD	50.0
#define MINE_FORCE_THRESHOLD	0.4
#define RAILS_FORCE_THRESHOLD	0.4
#define SHIELD_FORCE_THRESHOLD	0.2

#define CRAFT_SPLASH_FORCE	200.0
#define WEAPON_SPLASH_FORCE	200.0
#define ROCKET_SPLASH_FORCE	4000.0
#define MINE_SPLASH_FORCE	12000.0
#define RAILS_SPLASH_FORCE	5.0

#define CRAFT_EXPLOSION_SIZE	2000.0
#define WEAPON_EXPLOSION_SIZE	1000.0
#define ROCKET_EXPLOSION_SIZE	500.0
#define MINE_EXPLOSION_SIZE		600.0
#define RAILS_EXPLOSION_SIZE	50.0

#define VELOCITY_FORCE_MULTIPLIER	0.005
#define FORCE_VELOCITY_MULTIPLIER	40.0

#define ROLLING_SPEED	0.024

#define IDEAL_CRAFT_WEAPON_DIST			100.0
#define MAX_CRAFT_WEAPON_DIST			150.0
#define MAX_WEAPON_ROTATE_SPEED			0.075
#define MAX_WEAPON_MOVE_IN_OUT_SPEED	0.4

#define ROCKET_THRUST_TIME 400


void clear_sgame()
{
	LL_REMOVE_ALL(struct spawn_point_t, &spawn_point0);
	LL_REMOVE_ALL(struct teleporter_t, &teleporter0);
	LL_REMOVE_ALL(struct speedup_ramp_t, &speedup_ramp0);
	LL_REMOVE_ALL(struct gravity_well_t, &gravity_well0);
	nextentity = 0;
}


int read_spawn_point(gzFile file)
{
	struct spawn_point_t spawn_point;
	
	if(gzread(file, &spawn_point.x, 4) != 4)
		goto error;
	
	if(gzread(file, &spawn_point.y, 4) != 4)
		goto error;
	
	if(gzread(file, &spawn_point.angle, 4) != 4)
		goto error;
	
	if(gzread(file, &spawn_point.teleport_only, 4) != 4)
		goto error;
	
	if(gzread(file, &spawn_point.index, 4) != 4)
		goto error;
	
	LL_ADD(struct spawn_point_t, &spawn_point0, &spawn_point);
	
	return 1;
	
error:
	
	return 0;
}


int read_speedup_ramp(gzFile file)
{
	struct speedup_ramp_t speedup_ramp;
		
	if(gzread(file, &speedup_ramp.x, 4) != 4)
		goto error;
	
	if(gzread(file, &speedup_ramp.y, 4) != 4)
		goto error;
	
	if(gzread(file, &speedup_ramp.theta, 4) != 4)
		goto error;
	
	if(gzread(file, &speedup_ramp.width, 4) != 4)
		goto error;
	
	if(gzread(file, &speedup_ramp.boost, 4) != 4)
		goto error;
	
	speedup_ramp.last_boost_tick = 0;
	
	LL_ADD(struct speedup_ramp_t, &speedup_ramp0, &speedup_ramp);
	
	return 1;
	
error:
	
	return 0;
}


int read_teleporter(gzFile file)
{
	struct teleporter_t teleporter;
		
	if(gzread(file, &teleporter.x, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.y, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.radius, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.sparkles, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.spawn_index, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.rotation_type, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.rotation_angle, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.divider, 4) != 4)
		goto error;
	
	if(gzread(file, &teleporter.divider_angle, 4) != 4)
		goto error;

	LL_ADD(struct teleporter_t, &teleporter0, &teleporter);
	
	return 1;
	
error:
	
	return 0;
}


int read_gravity_well(gzFile file)
{
	struct gravity_well_t gravity_well;
		
	if(gzread(file, &gravity_well.x, 4) != 4)
		goto error;
	
	if(gzread(file, &gravity_well.y, 4) != 4)
		goto error;
	
	if(gzread(file, &gravity_well.strength, 4) != 4)
		goto error;
	
	if(gzread(file, &gravity_well.confined, 4) != 4)
		goto error;
	
	LL_ADD(struct gravity_well_t, &gravity_well0, &gravity_well);
	
	return 1;
	
error:
	
	return 0;
}


struct entity_t *new_entity(struct entity_t **entity0)
{
	assert(entity0);
	
	struct entity_t *entity;
	
	LL_CALLOC(struct entity_t, entity0, &entity);
		
	#ifdef EMSERVER
	entity->index = nextentity++;
	#endif
	
	#ifdef EMCLIENT
	entity->index = NO_ENT_INDEX;
	#endif
	
	return entity;
}


void remove_entity(struct entity_t **entity0, struct entity_t *entity)
{
	assert(entity);
	
	switch(entity->type)
	{
	case ENT_CRAFT:
		if(entity->craft_data.left_weapon)
			entity->craft_data.left_weapon->weapon_data.craft = NULL;
		
		if(entity->craft_data.right_weapon)
			entity->craft_data.right_weapon->weapon_data.craft = NULL;
		
		break;
		
	case ENT_WEAPON:
		if(entity->weapon_data.craft)
		{
			if(entity->weapon_data.craft->craft_data.left_weapon == entity)
				entity->weapon_data.craft->craft_data.left_weapon = NULL;
			
			if(entity->weapon_data.craft->craft_data.right_weapon == entity)
				entity->weapon_data.craft->craft_data.right_weapon = NULL;
		}
		
		break;
	}
	
	LL_REMOVE(struct entity_t, entity0, entity);
}


struct entity_t *get_entity(struct entity_t *entity0, uint32_t index)
{
	if(index == NO_ENT_INDEX)
		return NULL;
	
	struct entity_t *entity = entity0;
	
	while(entity)
	{
		if(entity->index == index)
			return entity;

		entity = entity->next;
	}

	return NULL;
}

#ifdef EMSERVER
struct player_t *get_weapon_owner(struct entity_t *weapon)
{
	assert(weapon);
	
	if(weapon->weapon_data.craft)
		return weapon->weapon_data.craft->craft_data.owner;
	else
		return NULL;
}
#endif


int point_in_circle(double px, double py, double cx, double cy, double cr)
{
	double xdist = px - cx;
	double ydist = py - cy;
	
	return xdist * xdist + ydist * ydist < cr * cr;
}


int line_in_circle(double lx1, double ly1, double lx2, double ly2, 
	double cx, double cy, double cr)
{
	double x1 = lx1 - cx;
	double y1 = ly1 - cy;
	
	if(x1 * x1 + y1 * y1 < cr * cr)
		return 1;
	
	double x2 = lx2 - cx;
	double y2 = ly2 - cy;
	
	if(x2 * x2 + y2 * y2 < cr * cr)
		return 1;

	double dx = x2 - x1;
	double dy = y2 - y1;
	double dr2 = dx * dx + dy * dy;
	double D = x1 * y2 - x2 * y1;
	double discr = cr * cr * dr2 - D * D;

	if(discr >= 0.0)
	{
		if(fabs(dx) > fabs(dy))
		{
			double thing = dx * sqrt(discr);
	
			double x = (D * dy + thing) / dr2;
			double t = (x + cx - lx1) / (lx2 - lx1);
			
			if(t > 0.0 && t < 1.0)
				return 1;
			
			x = (D * dy - thing) / dr2;
			t = (x + cx - lx1) / (lx2 - lx1);
			
			if(t > 0.0 && t < 1.0)
				return 1;
		}
		else
		{
			double thing = dy * sqrt(discr);
	
			double y = -(D * dx + thing) / dr2;
			double t = (y + cy - ly1) / (ly2 - ly1);
			
			if(t > 0.0 && t < 1.0)
				return 1;
			
			y = -(D * dx - thing) / dr2;
			t = (y + cy - ly1) / (ly2 - ly1);
			
			if(t > 0.0 && t < 1.0)
				return 1;
		}
	}
	
	return 0;
}	


int line_in_circle_with_coords(double lx1, double ly1, double lx2, double ly2, 
	double cx, double cy, double cr, float *out_x, float *out_y)
{
	double x1 = lx1 - cx;
	double y1 = ly1 - cy;
	
	if(x1 * x1 + y1 * y1 < cr * cr)
	{
		*out_x = x1;
		*out_y = y1;
		return 1;
	}
	
	double x2 = lx2 - cx;
	double y2 = ly2 - cy;
	
	if(x2 * x2 + y2 * y2 < cr * cr)
	{
		*out_x = x2;
		*out_y = y2;
		return 1;
	}

	double dx = x2 - x1;
	double dy = y2 - y1;
	double dr2 = dx * dx + dy * dy;
	double D = x1 * y2 - x2 * y1;
	double discr = cr * cr * dr2 - D * D;

	if(discr >= 0.0)
	{
		if(fabs(dx) > fabs(dy))
		{
			double thing = dx * sqrt(discr);
	
			double x = (D * dy - thing) / dr2;
			double t = (x + cx - lx1) / (lx2 - lx1);
			
			if(t > 0.0 && t < 1.0)
			{
				*out_x = x + cx;
				*out_y = ly1 + t * (ly2 - ly1);
				return 1;
			}
			
			x = (D * dy + thing) / dr2;
			t = (x + cx - lx1) / (lx2 - lx1);
			
			if(t > 0.0 && t < 1.0)
			{
				*out_x = x + cx;
				*out_y = ly1 + t * (ly2 - ly1);
				return 1;
			}
		}
		else
		{
			double thing = dy * sqrt(discr);
	
			double y = -(D * dx + thing) / dr2;
			double t = (y + cy - ly1) / (ly2 - ly1);
			
			if(t > 0.0 && t < 1.0)
			{
				*out_x = lx1 + t * (lx2 - lx1);
				*out_y = y + cy;
				return 1;
			}
			
			y = -(D * dx - thing) / dr2;
			t = (y + cy - ly1) / (ly2 - ly1);
			
			if(t > 0.0 && t < 1.0)
			{
				*out_x = lx1 + t * (lx2 - lx1);
				*out_y = y + cy;
				return 1;
			}
		}
	}
	
	return 0;
}	


int circles_intersect(double x1, double y1, double r1, double x2, double y2, double r2)
{
	double xdist = x2 - x1;
	double ydist = y2 - y1;
	double maxdist = r1 + r2;
	
	return xdist * xdist + ydist * ydist < maxdist * maxdist;
}


int circle_in_circle(double x1, double y1, double r1, double x2, double y2, double r2)
{
	double xdist = x2 - x1;
	double ydist = y2 - y1;
	return sqrt(xdist * xdist + ydist * ydist) + r1 < r2;
}


void apply_gravity_acceleration(struct entity_t *entity)
{
	double xacc = 0.0, yacc = 0.0;
	
	struct gravity_well_t *gravity_well = gravity_well0;
	
	while(gravity_well)
	{
		if(gravity_well->confined)
		{
			if(line_walk_bsp_tree(gravity_well->x, gravity_well->y, entity->xdis, entity->ydis))
			{
				gravity_well = gravity_well->next;
				continue;
			}
		}
		
		double xdist = entity->xdis - gravity_well->x;
		double ydist = entity->ydis - gravity_well->y;
		double dist_squared = xdist * xdist + ydist * ydist;
		double dist = sqrt(dist_squared);
		double mag = gravity_well->strength / dist_squared;
		
		xacc += (xdist / dist) * -mag;
		yacc += (ydist / dist) * -mag;
		
		gravity_well = gravity_well->next;
	}
	
	entity->xvel += xacc;
	entity->yvel += yacc;
}


void equal_bounce(struct entity_t *entity1, struct entity_t *entity2)
{
/*	double temp = entity1->xvel;
	entity1->xvel = entity2->xvel * COLLISION_FRICTION;
	entity2->xvel = temp * COLLISION_FRICTION;
	
	temp = entity1->yvel;
	entity1->yvel = entity2->yvel * COLLISION_FRICTION;
	entity2->yvel = temp * COLLISION_FRICTION;
	*/
	
	entity1->xvel *= COLLISION_FRICTION;
	entity1->yvel *= COLLISION_FRICTION;
	entity2->xvel *= COLLISION_FRICTION;
	entity2->yvel *= COLLISION_FRICTION;
	
	double distx = entity2->xdis - entity1->xdis;
	double disty = entity2->ydis - entity1->ydis;
	double dist = hypot(distx, disty);
	distx /= dist;
	disty /= dist;
	
	double a1 = entity1->xvel * distx + entity1->yvel * disty;
	double a2 = entity2->xvel * distx + entity2->yvel * disty;
	
	double op = (2.0 * (a1 - a2)) / (2.0);
	
	entity1->xvel -= op * distx;
	entity1->yvel -= op * disty;
	entity2->xvel += op * distx;
	entity2->yvel += op * disty;
}

	
void craft_weapon_bounce(struct entity_t *craft, struct entity_t *weapon)
{
	craft->xvel *= COLLISION_FRICTION;
	craft->yvel *= COLLISION_FRICTION;
	weapon->xvel *= COLLISION_FRICTION;
	weapon->yvel *= COLLISION_FRICTION;
	
	double distx = weapon->xdis - craft->xdis;
	double disty = weapon->ydis - craft->ydis;
	double dist = hypot(distx, disty);
	distx /= dist;
	disty /= dist;
	
	double a1 = craft->xvel * distx + craft->yvel * disty;
	double a2 = weapon->xvel * distx + weapon->yvel * disty;
	
	double op = (2.0 * (a1 - a2)) / (CRAFT_MASS + WEAPON_MASS);
	
	craft->xvel -= op * WEAPON_MASS * distx;
	craft->yvel -= op * WEAPON_MASS * disty;
	weapon->xvel += op * CRAFT_MASS * distx;
	weapon->yvel += op * CRAFT_MASS * disty;
}


void weapon_rails_bounce(struct entity_t *weapon, struct entity_t *rails)
{
	weapon->xvel *= COLLISION_FRICTION;
	weapon->yvel *= COLLISION_FRICTION;
	rails->xvel *= COLLISION_FRICTION;
	rails->yvel *= COLLISION_FRICTION;
	
	double distx = rails->xdis - weapon->xdis;
	double disty = rails->ydis - weapon->ydis;
	double dist = hypot(distx, disty);
	distx /= dist;
	disty /= dist;
	
	double a1 = weapon->xvel * distx + weapon->yvel * disty;
	double a2 = rails->xvel * distx + rails->yvel * disty;
	
	double op = (2.0 * (a1 - a2)) / (CRAFT_MASS + RAILS_MASS);
	
	weapon->xvel -= op * WEAPON_MASS * distx;
	weapon->yvel -= op * WEAPON_MASS * disty;
	rails->xvel += op * CRAFT_MASS * distx;
	rails->yvel += op * CRAFT_MASS * disty;
}


void entity_wall_bounce(struct entity_t *entity, struct bspnode_t *node)
{
	double wallx, wally;
	
	wallx = node->x2 - node->x1;
	wally = node->y2 - node->y1;
	
	double l = sqrt(wallx * wallx + wally * wally);
	double normalx = wally / l;
	double normaly = -wallx / l;
	
	double cos_theta = (normalx * entity->xvel + normaly * entity->yvel) / 
		(l * sqrt(entity->xvel * entity->xvel + entity->yvel * entity->yvel));
	
	if(cos_theta > 0.0)
	{
		normalx = -normalx;
		normaly = -normaly;
	}
	
	double d = fabs(wally * entity->xvel - wallx * entity->yvel) / l;
	
	entity->xvel = (entity->xvel + normalx * d * 2.0) * COLLISION_FRICTION;
	entity->yvel = (entity->yvel + normaly * d * 2.0) * COLLISION_FRICTION;
}


void slow_entity(struct entity_t *entity)
{
	entity->xvel *= SPACE_FRICTION;
	entity->yvel *= SPACE_FRICTION;
}


void strip_weapons_from_craft(struct entity_t *craft)
{
	assert(craft);
	
	if(craft->craft_data.left_weapon)
	{
		craft->craft_data.left_weapon->weapon_data.craft = NULL;
		craft->craft_data.left_weapon = NULL;
	}
	
	if(craft->craft_data.right_weapon)
	{
		craft->craft_data.right_weapon->weapon_data.craft = NULL;
		craft->craft_data.right_weapon = NULL;
	}
}

	
void strip_craft_from_weapon(struct entity_t *weapon)
{
	assert(weapon);
	
	if(weapon->weapon_data.craft)
	{
		if(weapon->weapon_data.craft->craft_data.left_weapon == weapon)
			weapon->weapon_data.craft->craft_data.left_weapon = NULL;
		
		if(weapon->weapon_data.craft->craft_data.right_weapon == weapon)
			weapon->weapon_data.craft->craft_data.right_weapon = NULL;
		
		weapon->weapon_data.craft = NULL;
	}
}


void craft_flare(struct entity_t *craft, double force)
{
	craft->craft_data.shield_flare += force * 40.0;
	craft->craft_data.shield_flare = min(craft->craft_data.shield_flare, 1.0);
}


#ifdef EMCLIENT
void craft_force(struct entity_t *craft, double force)
{
	if(!craft->craft_data.carcass)
	{
		if(game_state != GAMESTATE_DEMO)
		{
			if(cgame_state->follow_me == craft->index)
				cgame_state->craft_shield = max(cgame_state->craft_shield - force, 0.0);
		}
		
		craft_flare(craft, force);
	}
}
#endif


#ifdef EMSERVER
void explode_craft(struct entity_t *craft, struct player_t *responsibility);
int craft_force(struct entity_t *craft, double force, struct player_t *responsibility)
{
	assert(!craft->kill_me);
	
	craft->craft_data.shield_strength -= force;
	if(craft->craft_data.shield_strength < 0.0)
	{
		if(!craft->craft_data.carcass)
		{
			respawn_craft(craft, responsibility);
			
			if(craft->craft_data.shield_strength > -0.25)
			{
				craft->craft_data.carcass = 1;
				craft->craft_data.braking = 0;
				strip_weapons_from_craft(craft);
				make_carcass_on_all_players(craft);
				return 0;
			}
			else
			{
				explode_craft(craft, responsibility);
				return 1;
			}
		}
		else
		{
			if(craft->craft_data.shield_strength < -0.25)
			{
				explode_craft(craft, responsibility);
				return 1;
			}
		}
	}
	else
	{
		craft_flare(craft, force);
	}
	
	return 0;
}
#endif


void weapon_flare(struct entity_t *weapon, double force)
{
	weapon->weapon_data.shield_flare += force * 40.0;
	weapon->weapon_data.shield_flare = min(weapon->weapon_data.shield_flare, 1.0);
}


#ifdef EMCLIENT
void weapon_force(struct entity_t *weapon, double force)
{
	if(game_state != GAMESTATE_DEMO)
	{
		if(weapon->weapon_data.craft)
		{
			if(cgame_state->follow_me == weapon->weapon_data.craft->index)
			{
				if(weapon->weapon_data.craft->craft_data.left_weapon == weapon)
					cgame_state->left_shield = max(cgame_state->left_shield - force, 0.0);
				else
					cgame_state->right_shield = max(cgame_state->right_shield - force, 0.0);
			}
		}
	}
		
	weapon_flare(weapon, force);
}
#endif


#ifdef EMSERVER
void explode_weapon(struct entity_t *weapon, struct player_t *responsibility);
int weapon_force(struct entity_t *weapon, double force, struct player_t *responsibility)
{
	weapon->weapon_data.shield_strength -= force;
	if(weapon->weapon_data.shield_strength < 0.0)
	{
		explode_weapon(weapon, responsibility);
		return 1;
	}
	else
	{
		weapon_flare(weapon, force);
	}
	
	return 0;
}
#endif


#ifdef EMSERVER

int plasma_rail_hit(struct entity_t *plasma)
{
	remove_entity_from_all_players(plasma);
	remove_entity(sentity0, plasma);

	return 1;
}


void destroy_rocket(struct entity_t *rocket);
int rocket_force(struct entity_t *rocket, double force)
{
	if(force > ROCKET_FORCE_THRESHOLD)
	{
		destroy_rocket(rocket);
		return 1;
	}
	
	return 0;
}


void destroy_mine(struct entity_t *mine);
int mine_force(struct entity_t *mine, double force)
{
	if(force > MINE_FORCE_THRESHOLD)
	{
		remove_entity_from_all_players(mine);
		destroy_mine(mine);
		return 1;
	}
	
	return 0;
}


void explode_rails(struct entity_t *rails, struct player_t *responsibility);
int rails_force(struct entity_t *rails, double force, struct player_t *responsibility)
{
	if(force > RAILS_FORCE_THRESHOLD)
	{
		explode_rails(rails, responsibility);
		return 1;
	}
	
	return 0;
}


void destroy_shield(struct entity_t *shield);
int shield_force(struct entity_t *shield, double force)
{
	if(force > SHIELD_FORCE_THRESHOLD)
	{
		remove_entity_from_all_players(shield);
		destroy_shield(shield);
		return 1;
	}
	
	return 0;
}


void splash_force(float x, float y, float force, struct player_t *responsibility)
{
	struct entity_t *entity = *sentity0;
	
	while(entity)
	{
		if(entity->type == ENT_BULLET || entity->teleporting || entity->kill_me)
		{
			entity = entity->next;
			continue;
		}
			
		if(line_walk_bsp_tree(x, y, entity->xdis, entity->ydis))
		{
			entity = entity->next;
			continue;
		}
		
		double xdist = entity->xdis - x;
		double ydist = entity->ydis - y;
		double dist_squared = xdist * xdist + ydist * ydist;
		double dist = sqrt(dist_squared);
		double att_force = force / dist_squared;
		
		entity->xvel += (xdist / dist) * att_force * FORCE_VELOCITY_MULTIPLIER;
		entity->yvel += (ydist / dist) * att_force * FORCE_VELOCITY_MULTIPLIER;

		int in_tick = entity->in_tick;
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			craft_force(entity, att_force, responsibility);
		
			if(!entity->kill_me)
				update_player_shield_strengths(entity->craft_data.owner);
			
			break;
		
		case ENT_WEAPON:
			weapon_force(entity, att_force, responsibility);
		
			if(!entity->kill_me && entity->weapon_data.craft)
				update_player_shield_strengths(entity->weapon_data.craft->craft_data.owner);
			
			break;
		
		case ENT_ROCKET:
			rocket_force(entity, att_force);
			break;
		
		case ENT_MINE:
			mine_force(entity, att_force);
			break;
		
		case ENT_RAILS:
			rails_force(entity, att_force, responsibility);
			break;
		
		case ENT_SHIELD:
			shield_force(entity, att_force);
			break;
		}
		
		struct entity_t *next = entity->next;
		
		if(!in_tick && entity->kill_me)
			remove_entity(sentity0, entity);
		else
		{
			entity->propagate_me = 1;
			entity->in_tick = in_tick;
		}
		
		entity = next;
	}
}


void explode_craft(struct entity_t *craft, struct player_t *responsibility)
{
	craft->kill_me = 1;

	strip_weapons_from_craft(craft);
	remove_entity_from_all_players(craft);
	emit_explosion(craft->xdis, craft->ydis, craft->xvel, craft->yvel, 
		CRAFT_EXPLOSION_SIZE, craft->craft_data.magic_smoke, 
		craft->craft_data.smoke_start_red, craft->craft_data.smoke_start_green, 
		craft->craft_data.smoke_start_blue, craft->craft_data.smoke_end_red, 
		craft->craft_data.smoke_end_green, craft->craft_data.smoke_end_blue);
	splash_force(craft->xdis, craft->ydis, CRAFT_SPLASH_FORCE, responsibility);

	if(!craft->in_tick)
		remove_entity(sentity0, craft);
}


void explode_weapon(struct entity_t *weapon, struct player_t *responsibility)
{
	weapon->kill_me = 1;
	strip_craft_from_weapon(weapon);
	remove_entity_from_all_players(weapon);
	emit_explosion(weapon->xdis, weapon->ydis, weapon->xvel, weapon->yvel, 
		WEAPON_EXPLOSION_SIZE, weapon->weapon_data.magic_smoke, 
		weapon->weapon_data.smoke_start_red, weapon->weapon_data.smoke_start_green, 
		weapon->weapon_data.smoke_start_blue, weapon->weapon_data.smoke_end_red, 
		weapon->weapon_data.smoke_end_green, weapon->weapon_data.smoke_end_blue);
	splash_force(weapon->xdis, weapon->ydis, WEAPON_SPLASH_FORCE, responsibility);
	
	respawn_weapon(weapon);
	
	if(!weapon->in_tick)
		remove_entity(sentity0, weapon);
}


void explode_rails(struct entity_t *rails, struct player_t *responsibility)
{
	rails->kill_me = 1;
	
	remove_entity_from_all_players(rails);
	emit_explosion(rails->xdis, rails->ydis, rails->xvel, rails->yvel, 
		RAILS_EXPLOSION_SIZE, 0, 0xff, 0, 0, 0xff, 0xff, 0xff);
	splash_force(rails->xdis, rails->ydis, RAILS_SPLASH_FORCE, responsibility);
	schedule_respawn(rails->rails_data.spawn_point);
	
	if(!rails->in_tick)
		remove_entity(sentity0, rails);
}
#endif


void destroy_rocket(struct entity_t *rocket)
{
	rocket->kill_me = 1;
	
	#ifdef EMSERVER
	emit_explosion(rocket->xdis, rocket->ydis, rocket->xvel, rocket->yvel, 
		ROCKET_EXPLOSION_SIZE, rocket->rocket_data.magic_smoke, 
		rocket->rocket_data.smoke_start_red, rocket->rocket_data.smoke_start_green, 
		rocket->rocket_data.smoke_start_blue, rocket->rocket_data.smoke_end_red, 
		rocket->rocket_data.smoke_end_green, rocket->rocket_data.smoke_end_blue);
	splash_force(rocket->xdis, rocket->ydis, ROCKET_SPLASH_FORCE, rocket->rocket_data.owner);
	#endif
	
	#ifdef EMCLIENT
	stop_sample(rocket->rocket_data.sample);
	#endif
	
	if(!rocket->in_tick)
		remove_entity(sentity0, rocket);
}


void destroy_mine(struct entity_t *mine)
{
	mine->kill_me = 1;
	
	#ifdef EMSERVER
	emit_explosion(mine->xdis, mine->ydis, mine->xvel, mine->yvel, 
		MINE_EXPLOSION_SIZE, mine->mine_data.magic_smoke, 
		mine->mine_data.smoke_start_red, mine->mine_data.smoke_start_green, 
		mine->mine_data.smoke_start_blue, mine->mine_data.smoke_end_red, 
		mine->mine_data.smoke_end_green, mine->mine_data.smoke_end_blue);
	splash_force(mine->xdis, mine->ydis, MINE_SPLASH_FORCE, mine->mine_data.owner);
	#endif
	
	if(!mine->in_tick)
		remove_entity(sentity0, mine);
}


void destroy_shield(struct entity_t *shield)
{
	shield->kill_me = 1;
	
	#ifdef EMSERVER
	schedule_respawn(shield->shield_data.spawn_point);
	#endif
	
	if(!shield->in_tick)
		remove_entity(sentity0, shield);
}


void craft_craft_collision(struct entity_t *craft1, struct entity_t *craft2)
{
	#ifdef EMSERVER
	craft_force(craft1, hypot(craft2->xvel, craft2->yvel) * VELOCITY_FORCE_MULTIPLIER, 
		craft2->craft_data.owner);
	
	if(!craft2->kill_me)
		craft_force(craft2, hypot(craft1->xvel, craft1->yvel) * VELOCITY_FORCE_MULTIPLIER,
			craft1->craft_data.owner);
	#endif
	
	#ifdef EMCLIENT
	craft_force(craft1, hypot(craft2->xvel, craft2->yvel) * VELOCITY_FORCE_MULTIPLIER);
	craft_force(craft2, hypot(craft1->xvel, craft1->yvel) * VELOCITY_FORCE_MULTIPLIER);
	#endif
	
	if(!craft1->kill_me && !craft2->kill_me)
		equal_bounce(craft1, craft2);
}


void craft_weapon_collision(struct entity_t *craft, struct entity_t *weapon)
{
	#ifdef EMSERVER
	craft_force(craft, hypot(weapon->xvel, weapon->yvel) * (WEAPON_MASS / CRAFT_MASS) * 
		VELOCITY_FORCE_MULTIPLIER, get_weapon_owner(weapon));
	
	if(!weapon->kill_me)
		weapon_force(weapon, hypot(craft->xvel, craft->yvel) * (CRAFT_MASS / WEAPON_MASS) * 
			VELOCITY_FORCE_MULTIPLIER, craft->craft_data.owner);
	#endif
	
	#ifdef EMCLIENT
	craft_force(craft, hypot(weapon->xvel, weapon->yvel) * (WEAPON_MASS / CRAFT_MASS) * 
		VELOCITY_FORCE_MULTIPLIER);
	weapon_force(weapon, hypot(craft->xvel, craft->yvel) * (CRAFT_MASS / WEAPON_MASS) * 
			VELOCITY_FORCE_MULTIPLIER);
	#endif
	
	if(!craft->kill_me && !weapon->kill_me)
		craft_weapon_bounce(craft, weapon);
}


void craft_plasma_collision(struct entity_t *craft, struct entity_t *plasma)
{
	#ifdef EMSERVER
	craft_force(craft, PLASMA_DAMAGE, plasma->plasma_data.owner);
	#endif
	
	#ifdef EMCLIENT
	craft_force(craft, PLASMA_DAMAGE);
	#endif
	
	plasma->kill_me = 1;
}


void craft_bullet_collision(struct entity_t *craft, struct entity_t *bullet)
{
	#ifdef EMSERVER
	craft_force(craft, BULLET_DAMAGE, bullet->bullet_data.owner);
	#endif
	
	#ifdef EMCLIENT
	craft_force(craft, BULLET_DAMAGE);
	#endif
	
	bullet->kill_me = 1;
}


void craft_rails_collision(struct entity_t *craft, struct entity_t *rails)
{
	#ifdef EMSERVER
	craft->craft_data.owner->rails = min(CRAFT_MAX_RAILS, 
		craft->craft_data.owner->rails + rails->rails_data.quantity);
	update_player_ammo_levels(craft->craft_data.owner);
	
	schedule_respawn(rails->rails_data.spawn_point);
	#endif
	
	rails->kill_me = 1;
}


void craft_shield_collision(struct entity_t *craft, struct entity_t *shield)
{
	#ifdef EMSERVER
	if(!craft->craft_data.carcass)
	{
		craft->craft_data.shield_strength = min(1.0, 
			craft->craft_data.shield_strength + shield->shield_data.strength);
		update_player_shield_strengths(craft->craft_data.owner);
	}
	#endif
	
	destroy_shield(shield);
}


void weapon_weapon_collision(struct entity_t *weapon1, struct entity_t *weapon2)
{
	#ifdef EMSERVER
	weapon_force(weapon1, hypot(weapon2->xvel, weapon2->yvel) * VELOCITY_FORCE_MULTIPLIER, 
		get_weapon_owner(weapon2));
	
	if(!weapon2->kill_me)
		weapon_force(weapon2, hypot(weapon1->xvel, weapon1->yvel) * VELOCITY_FORCE_MULTIPLIER,
			get_weapon_owner(weapon1));
	#endif

	#ifdef EMCLIENT
	weapon_force(weapon1, hypot(weapon2->xvel, weapon2->yvel) * VELOCITY_FORCE_MULTIPLIER);
	weapon_force(weapon2, hypot(weapon1->xvel, weapon1->yvel) * VELOCITY_FORCE_MULTIPLIER);
	#endif

	if(!weapon1->kill_me && !weapon2->kill_me)
		equal_bounce(weapon1, weapon2);
}


void weapon_plasma_collision(struct entity_t *weapon, struct entity_t *plasma)
{
	#ifdef EMSERVER
	weapon_force(weapon, PLASMA_DAMAGE, plasma->plasma_data.owner);
	#endif
	
	#ifdef EMCLIENT
	weapon_force(weapon, PLASMA_DAMAGE);
	#endif
	
	plasma->kill_me = 1;
}


void weapon_bullet_collision(struct entity_t *weapon, struct entity_t *bullet)
{
	#ifdef EMSERVER
	weapon_force(weapon, BULLET_DAMAGE, bullet->bullet_data.owner);
	#endif
	
	#ifdef EMCLIENT
	weapon_force(weapon, BULLET_DAMAGE);
	#endif
	
	bullet->kill_me = 1;
}


void weapon_rails_collision(struct entity_t *weapon, struct entity_t *rails)
{
	#ifdef EMSERVER
	weapon_force(weapon, hypot(rails->xvel, rails->yvel) * (RAILS_MASS / WEAPON_MASS) * 
		VELOCITY_FORCE_MULTIPLIER, NULL);
	
	if(!rails->kill_me)
		rails_force(rails, hypot(weapon->xvel, weapon->yvel) * (WEAPON_MASS / RAILS_MASS) * 
			VELOCITY_FORCE_MULTIPLIER, get_weapon_owner(weapon));
	#endif
	
	#ifdef EMCLIENT
	weapon_force(weapon, hypot(rails->xvel, rails->yvel) * (RAILS_MASS / WEAPON_MASS) * 
		VELOCITY_FORCE_MULTIPLIER);
	#endif
	
	if(!weapon->kill_me && !rails->kill_me)
		weapon_rails_bounce(weapon, rails);
}


void weapon_shield_collision(struct entity_t *weapon, struct entity_t *shield)
{
	#ifdef EMSERVER
	weapon->weapon_data.shield_strength = min(1.0, 
		weapon->weapon_data.shield_strength + shield->shield_data.strength);
	if(weapon->weapon_data.craft)
		update_player_shield_strengths(weapon->weapon_data.craft->craft_data.owner);
	#endif
	
	destroy_shield(shield);
}


void plasma_rocket_collision(struct entity_t *plasma, struct entity_t *rocket)
{
	plasma->kill_me = 1;
	destroy_rocket(rocket);
}


void plasma_mine_collision(struct entity_t *plasma, struct entity_t *mine)
{
	plasma->kill_me = 1;
	destroy_mine(mine);
}


void plasma_rails_collision(struct entity_t *plasma, struct entity_t *rails)
{
	plasma->kill_me = 1;
	
	#ifdef EMSERVER
	rails_force(rails, PLASMA_DAMAGE, plasma->plasma_data.owner);
	#endif
}


void plasma_shield_collision(struct entity_t *plasma, struct entity_t *shield)
{
	plasma->kill_me = 1;
	destroy_shield(shield);
}


void bullet_rocket_collision(struct entity_t *bullet, struct entity_t *rocket)
{
	bullet->kill_me = 1;
	destroy_rocket(rocket);
}


void bullet_mine_collision(struct entity_t *bullet, struct entity_t *mine)
{
	bullet->kill_me = 1;
	destroy_mine(mine);
}


void bullet_rails_collision(struct entity_t *bullet, struct entity_t *rails)
{
	bullet->kill_me = 1;
	
	#ifdef EMSERVER
	rails_force(rails, BULLET_DAMAGE, bullet->bullet_data.owner);
	#endif
}


void rocket_rocket_collision(struct entity_t *rocket1, struct entity_t *rocket2)
{
	destroy_rocket(rocket1);
	if(!rocket2->kill_me)
		destroy_rocket(rocket2);
}


void rocket_mine_collision(struct entity_t *rocket, struct entity_t *mine)
{
	destroy_rocket(rocket);
	if(!mine->kill_me)
		destroy_mine(mine);
}


void rocket_rails_collision(struct entity_t *rocket, struct entity_t *rails)
{
	destroy_rocket(rocket);
}


void mine_mine_collision(struct entity_t *mine1, struct entity_t *mine2)
{
	destroy_mine(mine1);
	if(!mine2->kill_me)
		destroy_mine(mine2);
}


void mine_rails_collision(struct entity_t *mine, struct entity_t *rails)
{
	destroy_mine(mine);
}


void rails_rails_collision(struct entity_t *rails1, struct entity_t *rails2)
{
	#ifdef EMSERVER
	rails_force(rails1, hypot(rails2->xvel, rails2->yvel) * VELOCITY_FORCE_MULTIPLIER, 
		get_weapon_owner(rails2));
	
	if(!rails2->kill_me)
		rails_force(rails2, hypot(rails1->xvel, rails1->yvel) * VELOCITY_FORCE_MULTIPLIER,
			get_weapon_owner(rails1));
	#endif

	if(!rails1->kill_me && !rails2->kill_me)
		equal_bounce(rails1, rails2);
}


void shield_shield_collision(struct entity_t *shield1, struct entity_t *shield2)
{
	#ifdef EMSERVER
	shield1->shield_data.strength += shield2->shield_data.strength;
	#endif
	
	destroy_shield(shield2);
}


void get_teleporter_spawn_point(struct teleporter_t *teleporter, float *x, float *y)
{
	struct spawn_point_t *spawn_point = spawn_point0;
		
	while(spawn_point)
	{
		if(spawn_point->index == teleporter->spawn_index)
		{
			*x = spawn_point->x;
			*y = spawn_point->y;
			return;
		}
		
		spawn_point = spawn_point->next;
	}
}


void get_spawn_point_coords(uint32_t index, float *x, float *y)
{
	struct spawn_point_t *spawn_point = spawn_point0;
		
	while(spawn_point)
	{
		if(spawn_point->index == index)
		{
			*x = spawn_point->x;
			*y = spawn_point->y;
			return;
		}
		
		spawn_point = spawn_point->next;
	}
}


int check_weapon_placement(float xdis, float ydis, struct entity_t *weapon)
{
	// check for collision with wall
	
	if(circle_walk_bsp_tree(xdis, ydis, WEAPON_RADIUS))
		return 0;
	
	
	// check for collision with other entities
	
	struct entity_t *entity = *sentity0;
	while(entity)
	{
		if(entity == weapon || entity->teleporting)
		{
			entity = entity->next;
			continue;
		}
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, CRAFT_RADIUS))
				return 0;
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, WEAPON_RADIUS))
				return 0;
			break;
			
	/*	case ENT_ROCKET:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, ROCKET_RADIUS))
				return 0;
			break;
	*/		
		case ENT_MINE:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, MINE_RADIUS))
				return 0;
			break;
			
		case ENT_RAILS:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, RAILS_RADIUS))
				return 0;
			break;
		}
		
		entity = entity->next;
	}
	
	return 1;
}


void set_weapon_initial_try_velocity(struct entity_t *weapon)
{
	struct entity_t *craft = weapon->weapon_data.craft;
		
	#ifdef EMSERVER
	//assert(craft);
	if(!craft)
		return;
	#endif
	
	#ifdef EMCLIENT
	if(!craft)
		return;		// inconsistency
	#endif
	
	
	// rotate on spot
	
	double delta = weapon->weapon_data.theta - craft->craft_data.theta;
	
	while(delta >= M_PI)
		delta -= M_PI * 2.0;
	while(delta < -M_PI)
		delta += M_PI * 2.0;
	
	if(fabs(delta) < MAX_WEAPON_ROTATE_SPEED)
		weapon->weapon_data.theta = craft->craft_data.theta;
	else
		weapon->weapon_data.theta -= copysign(MAX_WEAPON_ROTATE_SPEED, delta);
	
	while(weapon->weapon_data.theta >= M_PI)
		weapon->weapon_data.theta -= M_PI * 2.0;
	while(weapon->weapon_data.theta < -M_PI)
		weapon->weapon_data.theta += M_PI * 2.0;
	
	
	// rotate around craft

	double xdis = weapon->xdis + craft->xvel;
	double ydis = weapon->ydis + craft->yvel;
	
	double theta_side = -M_PI / 4.0;	// inconsistency visualization
	
	if(craft->craft_data.left_weapon == weapon)
		theta_side = M_PI / 2.0;
	else if(craft->craft_data.right_weapon == weapon)
		theta_side = -M_PI / 2.0;
	
	
	double theta = atan2(-(xdis - craft->xdis), ydis - craft->ydis);
	delta = (craft->craft_data.theta + theta_side) - theta;
	
	while(delta >= M_PI)
		delta -= M_PI * 2.0;
	while(delta < -M_PI)
		delta += M_PI * 2.0;
	
	if(fabs(delta) < MAX_WEAPON_ROTATE_SPEED)
		theta = craft->craft_data.theta + theta_side;
	else
		theta += copysign(MAX_WEAPON_ROTATE_SPEED, delta);
	
	double dist = hypot(xdis - craft->xdis, ydis - craft->ydis);
	
	double sin_theta, cos_theta;
	sincos(theta, &sin_theta, &cos_theta);
	
	xdis = -dist * sin_theta;
	ydis = dist * cos_theta;

	
	// move in/out from craft
	
	delta = dist - IDEAL_CRAFT_WEAPON_DIST;
	
	if(abs(delta) < MAX_WEAPON_MOVE_IN_OUT_SPEED)
		dist = IDEAL_CRAFT_WEAPON_DIST;
	else
		dist -= copysign(MAX_WEAPON_MOVE_IN_OUT_SPEED, delta);
	
	xdis = craft->xdis + -dist * sin_theta;
	ydis = craft->ydis + dist * cos_theta;
	
	
	if(check_weapon_placement(xdis, ydis, weapon))
	{
		weapon->xvel = xdis - weapon->xdis;
		weapon->yvel = ydis - weapon->ydis;
	}
	else
	{
		weapon->xvel = 0.0;
		weapon->yvel = 0.0;
	}
}


int try_advance_craft(struct entity_t *craft, float old_xdis, float old_ydis)
{
	int restart = 0;
	
	// check for wall collision
	
	struct bspnode_t *node = circle_walk_bsp_tree(craft->xdis, craft->ydis, CRAFT_RADIUS);
		
	if(!node)
		node = line_walk_bsp_tree(old_xdis, old_ydis, craft->xdis, craft->ydis);
	
	if(node)
	{
		#ifdef EMSERVER
		craft_force(craft, hypot(craft->xvel, craft->yvel) * VELOCITY_FORCE_MULTIPLIER,
			craft->craft_data.owner);
		
		if(craft->kill_me)
			return 1;
		#endif
		
		#ifdef EMCLIENT
		craft_force(craft, hypot(craft->xvel, craft->yvel) * VELOCITY_FORCE_MULTIPLIER);
		#endif
		
		entity_wall_bounce(craft, node);
		
		restart = 1;
	}
	
	
	// check for collision with other entities
	
	struct entity_t *entity = *sentity0;
	while(entity)
	{
		if(entity == craft || entity->teleporting || entity->kill_me)
		{
			entity = entity->next;
			continue;
		}
		
		int entity_in_tick = entity->in_tick;
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				craft_craft_collision(craft, entity);
				restart = 1;
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				craft_weapon_collision(craft, entity);
				restart = 1;
			}
			break;
			
		case ENT_PLASMA:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, PLASMA_RADIUS))
			{
				craft_plasma_collision(craft, entity);
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, 
				craft->xdis, craft->ydis, CRAFT_RADIUS))
			{
				craft_bullet_collision(craft, entity);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				destroy_rocket(entity);
				restart = 1;
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, MINE_RADIUS))
			{
				if(!entity->mine_data.under_craft || 
					entity->mine_data.craft_id != craft->index)
				{
					destroy_mine(entity);
					restart = 1;
				}
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				craft_rails_collision(craft, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
				entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				craft_shield_collision(craft, entity);
			}
			break;
		}
		
		struct entity_t *next = entity->next;
		
		entity->in_tick = entity_in_tick;
		
		if(!entity->in_tick && entity->kill_me)
			remove_entity(sentity0, entity);

		#ifdef EMSERVER
		if(craft->kill_me)
			return 1;
		#endif
		
		entity = next;
	}
	
	
	#ifdef EMSERVER
	
	// check for collision with speedup ramp
	
	int s = 0;
	
	struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
	while(speedup_ramp)
	{
		if(cgame_tick - speedup_ramp->last_boost_tick < 200)
		{
			LL_NEXT(speedup_ramp);
			continue;
		}
		
		double sin_theta, cos_theta;
		sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
		
		if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
			speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
			speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
			speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
			craft->xdis, craft->ydis, CRAFT_RADIUS))
		{
			s = 1;
			
			if(!craft->speeding_up)
			{
				craft->xvel -= speedup_ramp->boost * sin_theta;
				craft->yvel += speedup_ramp->boost * cos_theta;
				
				restart = 1;
				craft->propagate_me = 1;
				craft->speeding_up = 1;
				
				
				// try to prevent anomalies
				
				if(craft->craft_data.left_weapon)
					craft->craft_data.left_weapon->propagate_me = 1;
				
				if(craft->craft_data.right_weapon)
					craft->craft_data.right_weapon->propagate_me = 1;
				
				speedup_ramp->last_boost_tick = cgame_tick;
				
				emit_speedup_to_all_players(speedup_ramp);
				break;
			}
		}
		
		LL_NEXT(speedup_ramp);
	}
	
	craft->speeding_up = s;
	
	
	
	// check for collision with teleporter
	
	struct teleporter_t *teleporter = teleporter0;
	while(teleporter)
	{
		if(circle_in_circle(craft->xdis, craft->ydis, CRAFT_RADIUS, 
			teleporter->x, teleporter->y, teleporter->radius))
		{
			if(craft->craft_data.left_weapon)
			{
				craft->craft_data.left_weapon->propagate_me = 1;
				craft->craft_data.left_weapon->weapon_data.craft = NULL;
				craft->craft_data.left_weapon = NULL;
			}
			
			if(craft->craft_data.right_weapon)
			{
				craft->craft_data.right_weapon->propagate_me = 1;
				craft->craft_data.right_weapon->weapon_data.craft = NULL;
				craft->craft_data.right_weapon = NULL;
			}
			
			craft->teleport_spawn_index = teleporter->spawn_index;
			
			craft->teleporting = TELEPORTING_DISAPPEARING;
			craft->teleporting_tick = cgame_tick;
			
			craft->propagate_me = 1;
			
			emit_teleport_to_all_players(craft);
			break;
		}
		
		teleporter = teleporter->next;
	}
	
	#endif
	
	return !restart;
}


int try_advance_weapon(struct entity_t *weapon, float old_xdis, float old_ydis)
{
	int restart = 0;
	
	#ifdef EMSERVER
	struct player_t *owner = get_weapon_owner(weapon);
	#endif

	
	// check for collision against wall
	
	struct bspnode_t *node = circle_walk_bsp_tree(weapon->xdis, weapon->ydis, WEAPON_RADIUS);
		
	if(!node)
		node = line_walk_bsp_tree(old_xdis, old_ydis, weapon->xdis, weapon->ydis);
	
	if(node)
	{
		#ifdef EMSERVER
		weapon_force(weapon, hypot(weapon->xvel, weapon->yvel) * VELOCITY_FORCE_MULTIPLIER, 
			owner);
		
		if(weapon->kill_me)
			return 1;
		#endif
		
		#ifdef EMCLIENT
		weapon_force(weapon, hypot(weapon->xvel, weapon->yvel) * VELOCITY_FORCE_MULTIPLIER);
		#endif
		
		entity_wall_bounce(weapon, node);
	}
	
	
	// check for collision with other entities
	
	struct entity_t *entity = *sentity0;
	while(entity)
	{
		if(entity == weapon || entity->teleporting || entity->kill_me)
		{
			entity = entity->next;
			continue;
		}
	
		int entity_in_tick = entity->in_tick;
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				craft_weapon_collision(entity, weapon);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				weapon_weapon_collision(weapon, entity);
			}
			break;
			
		case ENT_PLASMA:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, PLASMA_RADIUS))
			{
				if(!entity->plasma_data.in_weapon || 
					entity->plasma_data.weapon_id != weapon->index)
				{
					weapon_plasma_collision(weapon, entity);
				}
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, 
				weapon->xdis, weapon->ydis, WEAPON_RADIUS))
			{
				if(!entity->bullet_data.in_weapon || 
					entity->bullet_data.weapon_id != weapon->index)
				{
					weapon_bullet_collision(weapon, entity);
				}
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				if(!entity->rocket_data.in_weapon || 
					entity->rocket_data.weapon_id != weapon->index)
				{
					destroy_rocket(entity);
				}
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, MINE_RADIUS))
			{
				destroy_mine(entity);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				weapon_rails_collision(weapon, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
				entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				weapon_shield_collision(weapon, entity);
			}
			break;
		}
		
		struct entity_t *next = entity->next;
		
		entity->in_tick = entity_in_tick;
		
		if(!entity->in_tick && entity->kill_me)
			remove_entity(sentity0, entity);
		
		#ifdef EMSERVER
		if(weapon->kill_me)
			return 1;
		#endif
		
		entity = next;
	}
	

	#ifdef EMSERVER
	
	
	// check for collision with speedup ramp
	
/*	int s = 0;
	
	struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
	while(speedup_ramp)
	{
		if(cgame_tick - speedup_ramp->last_boost_tick < 200)
		{
			LL_NEXT(speedup_ramp);
			continue;
		}
		
		
		double sin_theta, cos_theta;
		sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
		
		if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
			speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
			speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
			speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
			weapon->xdis, weapon->ydis, CRAFT_RADIUS))
		{
			s = 1;
			
			if(!weapon->speeding_up)
			{
				weapon->xvel -= speedup_ramp->boost * sin_theta;
				weapon->yvel += speedup_ramp->boost * cos_theta;
				
				restart = 1;
				weapon->propagate_me = 1;
				weapon->speeding_up = 1;
				
				
				// try to prevent anomalies
				
				if(weapon->weapon_data.craft)
					weapon->weapon_data.craft->propagate_me = 1;
				
				speedup_ramp->last_boost_tick = cgame_tick;
				
				emit_speedup_to_all_players(speedup_ramp);
				break;
			}
		}			
		
		LL_NEXT(speedup_ramp);
	}
	
	weapon->speeding_up = s;
	*/
	
	// check for collision with teleporter
	
	struct teleporter_t *teleporter = teleporter0;
	while(teleporter)
	{
		if(circle_in_circle(weapon->xdis, weapon->ydis, WEAPON_RADIUS, 
			teleporter->x, teleporter->y, teleporter->radius))
		{
			weapon->weapon_data.craft->propagate_me = 1;
			strip_craft_from_weapon(weapon);
			
			weapon->teleport_spawn_index = teleporter->spawn_index;
			
			weapon->teleporting = TELEPORTING_DISAPPEARING;
			weapon->teleporting_tick = cgame_tick;
			
			weapon->propagate_me = 1;
			emit_teleport_to_all_players(weapon);
			
//			restart = 1;
			return 1;
		}
		
		teleporter = teleporter->next;
	}
	
	#endif

	return !restart;
}


void s_tick_craft(struct entity_t *craft)
{
	craft->craft_data.shield_flare = max(0.0, craft->craft_data.shield_flare - 0.005);
	
	if(craft->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - craft->teleporting_tick) / 200.0;

		switch(craft->teleporting)
		{
		case TELEPORTING_DISAPPEARING:
			if(time > TELEPORT_FADE_TIME)
			{
				craft->teleporting = TELEPORTING_TRAVELLING;
				craft->teleporting_tick = cgame_tick;
			}
			break;
		
		case TELEPORTING_TRAVELLING:
			if(time > TELEPORT_TRAVEL_TIME - TELEPORT_FADE_TIME * 2.0)
			{
				craft->teleporting = TELEPORTING_APPEARING;
				craft->teleporting_tick = cgame_tick;
				
				get_spawn_point_coords(craft->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				craft->xdis = teleporter_x - craft->xvel * TELEPORT_FADE_TIME * 200.0;
				craft->ydis = teleporter_y - craft->yvel * TELEPORT_FADE_TIME * 200.0;
			}
			break;

		case TELEPORTING_APPEARING:
			if(time > TELEPORT_FADE_TIME)
				craft->teleporting = TELEPORTING_FINISHED;
			break;
		}

		if(craft->teleporting == TELEPORTING_DISAPPEARING ||
			craft->teleporting == TELEPORTING_APPEARING)
		{
			craft->xdis += craft->xvel;
			craft->ydis += craft->yvel;
			
			
			#ifdef EMSERVER
			
			// check for telefragging
			
			struct entity_t *entity = *sentity0;
			while(entity)
			{
				if(entity == craft || entity->teleporting)
				{
					entity = entity->next;
					continue;
				}
				
				entity->in_tick = 1;
				
				switch(entity->type)
				{
				case ENT_CRAFT:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, CRAFT_RADIUS))
					{
						respawn_craft(entity, craft->craft_data.owner);
						explode_craft(entity, craft->craft_data.owner);
						craft_telefragged(entity->craft_data.owner, craft->craft_data.owner);
					}
					break;
					
				case ENT_WEAPON:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, WEAPON_RADIUS))
						explode_weapon(entity, craft->craft_data.owner);
					break;
					
				case ENT_PLASMA:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, PLASMA_RADIUS))
						entity->kill_me = 1;
					break;
				
				case ENT_BULLET:
					if(point_in_circle(entity->xdis, entity->ydis, craft->xdis, 
						craft->ydis, CRAFT_RADIUS))
						entity->kill_me = 1;
					break;
					
				case ENT_ROCKET:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, ROCKET_RADIUS))
						destroy_rocket(entity);
					break;
					
				case ENT_MINE:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, MINE_RADIUS))
					{
						if(!entity->mine_data.under_craft || 
							entity->mine_data.craft_id != craft->index)
							destroy_mine(entity);
					}
					break;
					
				case ENT_RAILS:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, RAILS_RADIUS))
						explode_rails(entity, craft->craft_data.owner);
					break;
					
				case ENT_SHIELD:
					if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, 
						entity->xdis, entity->ydis, SHIELD_RADIUS))
						destroy_shield(entity);
					break;
				}
				
				struct entity_t *next = entity->next;
				
				if(entity->kill_me)
					remove_entity(sentity0, entity);
				else
					entity->in_tick = 0;
	
				entity = next;
			}
			
			#endif
		}
		
		if(craft->teleporting)
			return;
	}
	
	while(craft->craft_data.theta >= M_PI)
		craft->craft_data.theta -= 2 * M_PI;
	
	while(craft->craft_data.theta < -M_PI)
		craft->craft_data.theta += 2 * M_PI;

	#ifdef EMCLIENT
	if(craft->craft_data.rolling_left || craft->craft_data.rolling_right)
		craft->craft_data.old_theta = craft->craft_data.theta;
	#endif
	
	if(craft->craft_data.rolling_left)
		craft->craft_data.theta += ROLLING_SPEED;
	
	if(craft->craft_data.rolling_right)
		craft->craft_data.theta -= ROLLING_SPEED;
	
	if(!craft->craft_data.carcass)
	{
		double sin_theta, cos_theta;
		sincos(craft->craft_data.theta, &sin_theta, &cos_theta);
		
		craft->xvel -= craft->craft_data.acc * sin_theta;
		craft->yvel += craft->craft_data.acc * cos_theta;
	}

	apply_gravity_acceleration(craft);
	slow_entity(craft);

	
	if(craft->craft_data.braking)
	{
		craft->xvel *= BRAKE_FRICTION;
		craft->yvel *= BRAKE_FRICTION;
	}
	
	
	struct entity_t *left_weapon = craft->craft_data.left_weapon;
	struct entity_t *right_weapon = craft->craft_data.right_weapon;
	
	int restarted = 0;
	int craft_advanced = 0, left_weapon_advanced = 0, right_weapon_advanced = 0;

	double old_craft_xdis = craft->xdis;
	double old_craft_ydis = craft->ydis;
	
	double old_left_weapon_xdis = 0.0, old_left_weapon_ydis = 0.0, 
		old_right_weapon_xdis = 0.0, old_right_weapon_ydis = 0.0;

	
	craft->xdis += craft->xvel;
	craft->ydis += craft->yvel;
	
	if(left_weapon)
	{
		set_weapon_initial_try_velocity(left_weapon);
		old_left_weapon_xdis = left_weapon->xdis;
		old_left_weapon_ydis = left_weapon->ydis;
		left_weapon->in_tick = 1;
	}
	
	
	if(right_weapon)
	{
		set_weapon_initial_try_velocity(right_weapon);
		old_right_weapon_xdis = right_weapon->xdis;
		old_right_weapon_ydis = right_weapon->ydis;
		right_weapon->in_tick = 1;
	}
	
	craft->xdis = old_craft_xdis;
	craft->ydis = old_craft_ydis;
	
	while(1)
	{
		// see if our iterative collison technique has locked
		
		if(restarted)
		{
			if(!craft_advanced && craft->xdis == old_craft_xdis && craft->ydis == old_craft_ydis)
			{
				craft->xdis = old_craft_xdis;
				craft->ydis = old_craft_ydis;
				
				#ifdef EMSERVER
				respawn_craft(craft, craft->craft_data.owner);
				explode_craft(craft, craft->craft_data.owner);
			/*	char *msg = "infinite iteration craft collison broken\n";
				console_print(msg);
				print_on_all_players(msg);
			*/	#endif
				
				#ifdef EMCLIENT
				// try to prevent this happening again until the server 
				// explodes the craft or history gets rewritten
				craft->xvel = craft->yvel = 0.0;
				craft->craft_data.acc = 0.0;
				#endif
	
				craft_advanced = 1;
			}
			
			if(left_weapon && !left_weapon_advanced)
			{
				if(left_weapon->xvel == 0.0 && left_weapon->yvel == 0.0)
				{
					left_weapon->xdis = old_left_weapon_xdis;
					left_weapon->ydis = old_left_weapon_ydis;
					
					#ifdef EMSERVER
					explode_weapon(left_weapon, get_weapon_owner(left_weapon));
				/*	char *msg = "infinite iteration weapon collison broken\n";
					console_print(msg);
					print_on_all_players(msg);
				*/	#endif
					
					#ifdef EMCLIENT
					// try to prevent this happening again until the server 
					// explodes the weapon or history gets rewritten
					left_weapon->xvel = left_weapon->yvel = 0.0;
					#endif
					
					left_weapon_advanced = 1;
				}
			}
			
			if(right_weapon && !right_weapon_advanced)
			{
				if(right_weapon->xvel == 0.0 && right_weapon->yvel == 0.0)
				{
					right_weapon->xdis = old_right_weapon_xdis;
					right_weapon->ydis = old_right_weapon_ydis;
					
					#ifdef EMSERVER
					explode_weapon(right_weapon, get_weapon_owner(right_weapon));
				/*	char *msg = "infinite iteration weapon collison broken\n";
					console_print(msg);
					print_on_all_players(msg);
				*/	#endif
					
					#ifdef EMCLIENT
					// try to prevent this happening again until the server 
					// explodes the weapon or history gets rewritten
					right_weapon->xvel = right_weapon->yvel = 0.0;
					#endif
					
					right_weapon_advanced = 1;
				}
			}
			
			
			if(!craft_advanced)
			{
				craft->xdis = old_craft_xdis;
				craft->ydis = old_craft_ydis;
			}
			
			if(left_weapon && !left_weapon_advanced)
			{
				left_weapon->xdis = old_left_weapon_xdis;
				left_weapon->ydis = old_left_weapon_ydis;
			}
			
			if(right_weapon && !right_weapon_advanced)
			{
				right_weapon->xdis = old_right_weapon_xdis;
				right_weapon->ydis = old_right_weapon_ydis;
			}
		}
		
		restarted = 1;
		
		if(!craft_advanced)
		{
			craft->xdis += craft->xvel;
			craft->ydis += craft->yvel;
		}
		
		if(left_weapon && !left_weapon_advanced)
		{
			left_weapon->xdis += left_weapon->xvel;
			left_weapon->ydis += left_weapon->yvel;
		}
		
		if(right_weapon && !right_weapon_advanced)
		{
			right_weapon->xdis += right_weapon->xvel;
			right_weapon->ydis += right_weapon->yvel;
		}
		
		if(!craft_advanced)
		{
			craft_advanced = try_advance_craft(craft, old_craft_xdis, old_craft_ydis);
		}
		
		if(left_weapon)
			if(left_weapon->kill_me)
				left_weapon_advanced = 1;
		
		if(right_weapon)
			if(right_weapon->kill_me)
				right_weapon_advanced = 1;
		
		if(left_weapon && !left_weapon_advanced)
		{
			left_weapon_advanced = try_advance_weapon(left_weapon, 
				old_left_weapon_xdis, old_left_weapon_ydis);
		}
		
		if(craft->kill_me)
			craft_advanced = 1;
		
		if(right_weapon)
			if(right_weapon->kill_me)
				right_weapon_advanced = 1;
		
		if(right_weapon && !right_weapon_advanced)
		{
			right_weapon_advanced = try_advance_weapon(right_weapon, 
				old_right_weapon_xdis, old_right_weapon_ydis);
		}
	
		if(craft->kill_me)
			craft_advanced = 1;
		
		if(right_weapon)
			if(right_weapon->kill_me)
				right_weapon_advanced = 1;
		
		if(!craft_advanced || (left_weapon && !left_weapon_advanced) || 
			(right_weapon && !right_weapon_advanced))
		{
			continue;
		}
		
		
		#ifdef EMCLIENT
		if(!craft->kill_me)
			tick_craft(craft, old_craft_xdis, old_craft_ydis);
		#endif
		
		if(left_weapon)
		{
			if(left_weapon->kill_me)
				remove_entity(sentity0, left_weapon);
			else
				left_weapon->in_tick = 0;
		}
		
		if(right_weapon)
		{
			if(right_weapon->kill_me)
				remove_entity(sentity0, right_weapon);
			else
				right_weapon->in_tick = 0;
		}
		
		break;
	}
}


void spawn_plasma(struct entity_t *weapon)
{
	struct entity_t *plasma = new_entity(sentity0);
	
	plasma->type = ENT_PLASMA;
	
	plasma->xdis = weapon->xdis;
	plasma->ydis = weapon->ydis;
	
	double sin_theta, cos_theta;
	sincos(weapon->weapon_data.theta, &sin_theta, &cos_theta);
	
	plasma->xvel = weapon->weapon_data.craft->xvel - sin_theta * 8.0;
	plasma->yvel = weapon->weapon_data.craft->yvel + cos_theta * 8.0;
	
	plasma->plasma_data.in_weapon = 1;
	plasma->plasma_data.weapon_id = weapon->index;
	
	plasma->plasma_data.red = weapon->weapon_data.plasma_red;
	plasma->plasma_data.green = weapon->weapon_data.plasma_green;
	plasma->plasma_data.blue = weapon->weapon_data.plasma_blue;
	
	#ifdef EMSERVER
	plasma->plasma_data.owner = get_weapon_owner(weapon);
	#endif
}


void spawn_bullet(struct entity_t *weapon)
{
	struct entity_t *bullet = new_entity(sentity0);
	
	bullet->type = ENT_BULLET;
	
	bullet->xdis = weapon->xdis;
	bullet->ydis = weapon->ydis;
	
	#ifdef EMCLIENT
	bullet->bullet_data.old_xdis = bullet->xdis;
	bullet->bullet_data.old_ydis = bullet->ydis;
	#endif
	
	double sin_theta, cos_theta;
	sincos(weapon->weapon_data.theta, &sin_theta, &cos_theta);
	
	bullet->xvel = weapon->weapon_data.craft->xvel - sin_theta * 24.0;
	bullet->yvel = weapon->weapon_data.craft->yvel + cos_theta * 24.0;
	
	bullet->bullet_data.in_weapon = 1;
	bullet->bullet_data.weapon_id = weapon->index;
	
	#ifdef EMSERVER
	bullet->plasma_data.owner = get_weapon_owner(weapon);
	#endif
}


void s_tick_weapon(struct entity_t *weapon)
{
	weapon->weapon_data.shield_flare = max(0.0, weapon->weapon_data.shield_flare - 0.005);
	
	if(weapon->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - weapon->teleporting_tick) / 200.0;
		if(time > TELEPORT_FADE_TIME)
		{
			switch(weapon->teleporting)
			{
			case TELEPORTING_DISAPPEARING:
				weapon->teleporting = TELEPORTING_APPEARING;
				weapon->teleporting_tick = cgame_tick;
			
				get_spawn_point_coords(weapon->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				weapon->xdis = teleporter_x - weapon->xvel * TELEPORT_FADE_TIME * 200.0;
				weapon->ydis = teleporter_y - weapon->yvel * TELEPORT_FADE_TIME * 200.0;
				break;
			
			case TELEPORTING_APPEARING:
				weapon->teleporting = TELEPORTING_FINISHED;
				break;
			}
		}
		
		if(weapon->teleporting)
		{
			weapon->xdis += weapon->xvel;
			weapon->ydis += weapon->yvel;
			return;
		}
	}

		
	int fire;
	
	if(weapon->weapon_data.firing && weapon->weapon_data.craft)
	{
		#ifdef EMSERVER
		if(weapon->weapon_data.ammo)
		#endif
		{
			switch(weapon->weapon_data.type)
			{
			case WEAPON_PLASMA_CANNON:
				fire = ((cgame_tick - weapon->weapon_data.firing_start) * 20) / 200 
					- weapon->weapon_data.fired;
				
				if(fire > 0)
				{
					#ifdef EMCLIENT
					start_entity_sample(plasma_cannon_sample, weapon->index, cgame_tick);
					
					if(game_state != GAMESTATE_DEMO)
					{
						if(cgame_state->follow_me == weapon->weapon_data.craft->index && 
							weapon->weapon_data.craft->craft_data.left_weapon == weapon)
						{
							if(cgame_state->left_ammo > 0)
								cgame_state->left_ammo--;
						}
						else
						{
							if(cgame_state->right_ammo > 0)
								cgame_state->right_ammo--;
						}
					}
					#endif
					
					spawn_plasma(weapon);
					weapon->weapon_data.fired += fire;
					
					#ifdef EMSERVER
					weapon->weapon_data.ammo--;
					#endif
				}
				break;
				
			case WEAPON_MINIGUN:
				fire = ((cgame_tick - weapon->weapon_data.firing_start) * 50) / 200 
					- weapon->weapon_data.fired;
				
				if(fire > 0)
				{
					#ifdef EMCLIENT
					start_entity_sample(plasma_cannon_sample, weapon->index, cgame_tick);
					
					if(game_state != GAMESTATE_DEMO)
					{
						if(cgame_state->follow_me == weapon->weapon_data.craft->index && 
							weapon->weapon_data.craft->craft_data.left_weapon == weapon)
						{
							if(cgame_state->left_ammo > 0)
								cgame_state->left_ammo--;
						}
						else
						{
							if(cgame_state->right_ammo > 0)
								cgame_state->right_ammo--;
						}
					}
					#endif
					
					spawn_bullet(weapon);
					weapon->weapon_data.fired += fire;
					
					#ifdef EMSERVER
					weapon->weapon_data.ammo--;
					#endif
				}
				break;
			}
		}
		
		#ifdef EMSERVER
		else
		{
			weapon->weapon_data.firing = 0;
			propagate_weapon_stop_firing(weapon);
		}
		#endif
	}
	

	if(weapon->weapon_data.craft)
	{
		struct entity_t *craft = weapon->weapon_data.craft;
		
		if(!point_in_circle(weapon->xdis, weapon->ydis, 
			craft->xdis, craft->ydis, MAX_CRAFT_WEAPON_DIST))
		{
			#ifdef EMSERVER
			if(!weapon->weapon_data.ammo)
			{
				detach_weapon(weapon);
			}
			else
			#endif
			
			{
				if(craft->craft_data.left_weapon == weapon)
					craft->craft_data.left_weapon = NULL;
				else
					craft->craft_data.right_weapon = NULL;
				
				weapon->xvel = weapon->weapon_data.craft->xvel;
				weapon->yvel = weapon->weapon_data.craft->yvel;
				weapon->weapon_data.craft = NULL;
				weapon->weapon_data.firing = 0;
			}
		}
		else
		{
			if(craft->craft_data.left_weapon == weapon)
			{
				double delta = atan2(-(weapon->xdis - craft->xdis), weapon->ydis - craft->ydis) 
					- craft->craft_data.theta;
				
				while(delta >= M_PI)
					delta -= M_PI * 2.0;
				while(delta < -M_PI)
					delta += M_PI * 2.0;
				
				if(delta < 0.0)
				{
					#ifdef EMSERVER
					if(!weapon->weapon_data.ammo)
					{
						detach_weapon(weapon);
					}
					else
					#endif
					
					{
						craft->craft_data.left_weapon = NULL;
						weapon->xvel = weapon->weapon_data.craft->xvel;
						weapon->yvel = weapon->weapon_data.craft->yvel;
						weapon->weapon_data.craft = NULL;
						weapon->weapon_data.firing = 0;
					}
				}
			}
			else
			{
				double delta = atan2(-(weapon->xdis - craft->xdis), weapon->ydis - craft->ydis) 
					- craft->craft_data.theta;
				
				while(delta >= M_PI)
					delta -= M_PI * 2.0;
				while(delta < -M_PI)
					delta += M_PI * 2.0;
				
				if(delta > 0.0)
				{
					#ifdef EMSERVER
					if(!weapon->weapon_data.ammo)
					{
						detach_weapon(weapon);
					}
					else
					#endif
					
					{
						craft->craft_data.right_weapon = NULL;
						weapon->xvel = weapon->weapon_data.craft->xvel;
						weapon->yvel = weapon->weapon_data.craft->yvel;
						weapon->weapon_data.craft = NULL;
						weapon->weapon_data.firing = 0;
					}
				}
			}
		}
		
		return;
	}
	

	#ifdef EMSERVER
	struct player_t *owner = get_weapon_owner(weapon);
	#endif

	apply_gravity_acceleration(weapon);
	slow_entity(weapon);
	

	int restart = 0;
	double xdis;
	double ydis;
	
	while(1)
	{
		// see if our iterative collison technique has broken
		
		if(restart && weapon->xdis == xdis && weapon->ydis == ydis)
		{
			#ifdef EMSERVER
			explode_weapon(weapon, owner);
		//	char *msg = "infinite iteration weapon collison broken\n";
		//	console_print(msg);
		//	print_on_all_players(msg);
			#endif
			
			#ifdef EMCLIENT
			// try to prevent this happening again until the server 
			// explodes the weapon or history gets rewritten
			weapon->xvel = weapon->yvel = 0.0;
			#endif
			
			return;
		}
		
		xdis = weapon->xdis + weapon->xvel;
		ydis = weapon->ydis + weapon->yvel;
		
		restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, WEAPON_RADIUS);
			
		if(!node)
			node = line_walk_bsp_tree(weapon->xdis, weapon->ydis, xdis, ydis);
		
		if(node)
		{
			#ifdef EMSERVER
			weapon_force(weapon, hypot(weapon->xvel, weapon->yvel) * VELOCITY_FORCE_MULTIPLIER, 
				owner);
			
			if(weapon->kill_me)
				return;
			#endif
			
			#ifdef EMCLIENT
			weapon_force(weapon, hypot(weapon->xvel, weapon->yvel) * VELOCITY_FORCE_MULTIPLIER);
			#endif
			
			entity_wall_bounce(weapon, node);
			restart = 1;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = *sentity0;
		while(entity)
		{
			if(entity == weapon || entity->teleporting)
			{
				entity = entity->next;
				continue;
			}
		
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_weapon_collision(entity, weapon);
					restart = 1;
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					weapon_weapon_collision(weapon, entity);
					restart = 1;
				}
				break;
				
			case ENT_PLASMA:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, PLASMA_RADIUS))
				{
					if(!entity->plasma_data.in_weapon || 
						entity->plasma_data.weapon_id != weapon->index)
					{
						weapon_plasma_collision(weapon, entity);
						restart = 1;
					}
				}
				break;
				
			case ENT_BULLET:
				if(point_in_circle(entity->xdis, entity->ydis, 
					xdis, ydis, WEAPON_RADIUS))
				{
					if(!entity->bullet_data.in_weapon || 
						entity->bullet_data.weapon_id != weapon->index)
					{
						weapon_bullet_collision(weapon, entity);
						restart = 1;
					}
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					if(!entity->rocket_data.in_weapon || 
						entity->rocket_data.weapon_id != weapon->index)
					{
						destroy_rocket(entity);
						restart = 1;
					}
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, MINE_RADIUS))
				{
					destroy_mine(entity);
					restart = 1;
				}
				break;
				
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					weapon_rails_collision(weapon, entity);
					restart = 1;
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, 
					entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					weapon_shield_collision(weapon, entity);
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(sentity0, entity);
			else
				entity->in_tick = 0;
			
			#ifdef EMSERVER
			if(weapon->kill_me)
				return;
			#endif
			
			entity = next;
		}
		

		#ifdef EMSERVER
		
		
		// check for collision with speedup ramp
		
		int s = 0;
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			if(cgame_tick - speedup_ramp->last_boost_tick < 200)
			{
				LL_NEXT(speedup_ramp);
				continue;
			}
		
		
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, WEAPON_RADIUS))
			{
				s = 1;
				
				if(!weapon->speeding_up)
				{
					weapon->xvel -= speedup_ramp->boost * sin_theta;
					weapon->yvel += speedup_ramp->boost * cos_theta;
					
					speedup_ramp->last_boost_tick = cgame_tick;
					restart = 1;
					weapon->propagate_me = 1;
					weapon->speeding_up = 1;
					emit_speedup_to_all_players(speedup_ramp);
					break;
				}
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		weapon->speeding_up = s;
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, WEAPON_RADIUS, 
				teleporter->x, teleporter->y, teleporter->radius))
			{
				weapon->teleport_spawn_index = teleporter->spawn_index;
				
				weapon->teleporting = TELEPORTING_DISAPPEARING;
				weapon->teleporting_tick = cgame_tick;
				
				weapon->propagate_me = 1;
				emit_teleport_to_all_players(weapon);
				
//				restart = 1;
				return;
			}
			
			teleporter = teleporter->next;
		}
		
		#endif

		
		if(restart)
			continue;
		
		
		weapon->xdis = xdis;
		weapon->ydis = ydis;
		
		break;
	}
	
	if(weapon->weapon_data.detached)
		return;
	
	struct entity_t *craft = weapon->weapon_data.craft;
	
	struct entity_t *entity = *sentity0;
	while(entity)
	{
		if(entity->type == ENT_CRAFT && !entity->craft_data.carcass && 
			!entity->teleporting && entity->craft_data.left_weapon != weapon && 
			entity->craft_data.right_weapon != weapon)
		{
			if(point_in_circle(weapon->xdis, weapon->ydis, 
				entity->xdis, entity->ydis, MAX_CRAFT_WEAPON_DIST))
			{
				double delta = atan2(-(weapon->xdis - entity->xdis), 
					weapon->ydis - entity->ydis) - entity->craft_data.theta;
				
				while(delta >= M_PI)
					delta -= M_PI * 2.0;
				while(delta < -M_PI)
					delta += M_PI * 2.0;
				
				if(delta > 0.0 && !entity->craft_data.left_weapon)
				{
					entity->craft_data.left_weapon = weapon;
					weapon->weapon_data.craft = entity;
					craft = entity;
					weapon->weapon_data.firing = 0;
					
					if(!weapon->weapon_data.original_ownership_defined)
					{
						weapon->weapon_data.shield_red = 
							craft->craft_data.shield_red;
						weapon->weapon_data.shield_green = 
							craft->craft_data.shield_green;
						weapon->weapon_data.shield_blue = 
							craft->craft_data.shield_blue;
						
						if(weapon->weapon_data.type == WEAPON_PLASMA_CANNON)
						{
							weapon->weapon_data.plasma_red = 
								craft->craft_data.plasma_red;
							weapon->weapon_data.plasma_green = 
								craft->craft_data.plasma_green;
							weapon->weapon_data.plasma_blue = 
								craft->craft_data.plasma_blue;
						}

						weapon->weapon_data.magic_smoke = 
							craft->craft_data.magic_smoke;
						
						weapon->weapon_data.smoke_start_red = 
							craft->craft_data.smoke_start_red;
						weapon->weapon_data.smoke_start_green = 
							craft->craft_data.smoke_start_green;
						weapon->weapon_data.smoke_start_blue = 
							craft->craft_data.smoke_start_blue;
						
						weapon->weapon_data.smoke_end_red = 
							craft->craft_data.smoke_end_red;
						weapon->weapon_data.smoke_end_green = 
							craft->craft_data.smoke_end_green;
						weapon->weapon_data.smoke_end_blue = 
							craft->craft_data.smoke_end_blue;
					
						weapon->weapon_data.original_ownership_defined = 1;
					}
					
					#ifdef EMSERVER
					update_player_shield_strengths(craft->craft_data.owner);
					update_player_ammo_levels(craft->craft_data.owner);
					#endif
					
					break;
				}
				
				if(delta < 0.0 && !entity->craft_data.right_weapon)
				{
					entity->craft_data.right_weapon = weapon;
					weapon->weapon_data.craft = entity;
					craft = entity;
					weapon->weapon_data.firing = 0;
					
					if(!weapon->weapon_data.original_ownership_defined)
					{
						weapon->weapon_data.shield_red = 
							craft->craft_data.shield_red;
						weapon->weapon_data.shield_green = 
							craft->craft_data.shield_green;
						weapon->weapon_data.shield_blue = 
							craft->craft_data.shield_blue;
						
						if(weapon->weapon_data.type == WEAPON_PLASMA_CANNON)
						{
							weapon->weapon_data.plasma_red = 
								craft->craft_data.plasma_red;
							weapon->weapon_data.plasma_green = 
								craft->craft_data.plasma_green;
							weapon->weapon_data.plasma_blue = 
								craft->craft_data.plasma_blue;
						}

						weapon->weapon_data.magic_smoke = 
							craft->craft_data.magic_smoke;
						
						weapon->weapon_data.smoke_start_red = 
							craft->craft_data.smoke_start_red;
						weapon->weapon_data.smoke_start_green = 
							craft->craft_data.smoke_start_green;
						weapon->weapon_data.smoke_start_blue = 
							craft->craft_data.smoke_start_blue;
						
						weapon->weapon_data.smoke_end_red = 
							craft->craft_data.smoke_end_red;
						weapon->weapon_data.smoke_end_green = 
							craft->craft_data.smoke_end_green;
						weapon->weapon_data.smoke_end_blue = 
							craft->craft_data.smoke_end_blue;
						
						weapon->weapon_data.original_ownership_defined = 1;
					}
					
					#ifdef EMSERVER
					update_player_shield_strengths(craft->craft_data.owner);
					update_player_ammo_levels(craft->craft_data.owner);
					#endif
					
					break;
				}
			}
		}
		
		entity = entity->next;
	}
}


void s_tick_plasma(struct entity_t *plasma)
{
	if(plasma->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - plasma->teleporting_tick) / 200.0;
		if(time > TELEPORT_FADE_TIME)
		{
			switch(plasma->teleporting)
			{
			case TELEPORTING_DISAPPEARING:
				plasma->teleporting = TELEPORTING_APPEARING;
				plasma->teleporting_tick = cgame_tick;
			
				get_spawn_point_coords(plasma->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				plasma->xdis = teleporter_x - plasma->xvel * TELEPORT_FADE_TIME * 200.0;
				plasma->ydis = teleporter_y - plasma->yvel * TELEPORT_FADE_TIME * 200.0;
				break;
			
			case TELEPORTING_APPEARING:
				plasma->teleporting = TELEPORTING_FINISHED;
				break;
			}
		}
		
		if(plasma->teleporting)
		{
			plasma->xdis += plasma->xvel;
			plasma->ydis += plasma->yvel;
			return;
		}
	}
	
	// see if we have left the cannon
	
	if(plasma->plasma_data.in_weapon)
	{
		struct entity_t *weapon = get_entity(*sentity0, plasma->plasma_data.weapon_id);
			
		if(!weapon)
			plasma->plasma_data.in_weapon = 0;
		else
			if(!circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, weapon->xdis, weapon->ydis, WEAPON_RADIUS))
			{
				plasma->plasma_data.in_weapon = 0;
			}
	}
			
	

	apply_gravity_acceleration(plasma);
	
	
	double xdis = plasma->xdis + plasma->xvel;
	double ydis = plasma->ydis + plasma->yvel;
	
	// check for collision against wall
	
	struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, PLASMA_RADIUS);
			
	if(!node)
		node = line_walk_bsp_tree(plasma->xdis, plasma->ydis, xdis, ydis);
		
	if(node)
	{
		plasma->kill_me = 1;
		return;
	}
	
	plasma->xdis = xdis;
	plasma->ydis = ydis;
	
	
	// check for collision with other entities
	
	struct entity_t *entity = *sentity0;
	while(entity)
	{
		if(entity == plasma || entity->teleporting)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				craft_plasma_collision(entity, plasma);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				if(!plasma->plasma_data.in_weapon || 
					plasma->plasma_data.weapon_id != entity->index)
					weapon_plasma_collision(entity, plasma);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				plasma_rocket_collision(plasma, entity);
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
			{
				plasma_mine_collision(plasma, entity);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				plasma_rails_collision(plasma, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(plasma->xdis, plasma->ydis, 
				PLASMA_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				plasma_shield_collision(plasma, entity);
			}
			break;
		}
		
		struct entity_t *next = entity->next;
		
		if(entity->kill_me)
			remove_entity(sentity0, entity);
		else
			entity->in_tick = 0;
		
		if(plasma->kill_me)
			return;
		
		entity = next;
	}
	

	// check for collision with teleporter
	
	struct teleporter_t *teleporter = teleporter0;
	while(teleporter)
	{
		if(circle_in_circle(plasma->xdis, plasma->ydis, 
			PLASMA_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
		{
			plasma->teleport_spawn_index = teleporter->spawn_index;
			
			plasma->teleporting = TELEPORTING_DISAPPEARING;
			plasma->teleporting_tick = cgame_tick;
			
			#ifdef EMSERVER
			emit_teleport_to_all_players(plasma);
			#endif
			break;
		}
		
		teleporter = teleporter->next;
	}
}


void s_tick_bullet(struct entity_t *bullet)
{
	apply_gravity_acceleration(bullet);
	
	float xdis = bullet->xdis + bullet->xvel;
	float ydis = bullet->ydis + bullet->yvel;
	
	
	// check for collision against wall
	
	struct bspnode_t *node = line_walk_bsp_tree(bullet->xdis, bullet->ydis, xdis, ydis);
	if(node)
	{
		bullet->kill_me = 1;
		return;
	}
	
	
	// check for collision with other entities
	
	struct entity_t *entity = *sentity0;
	while(entity)
	{
		if(entity == bullet || entity->teleporting)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
				entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				craft_bullet_collision(entity, bullet);
			}
			break;
			
		case ENT_WEAPON:
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
				entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				if(!bullet->bullet_data.in_weapon || 
					bullet->bullet_data.weapon_id != entity->index)
					weapon_bullet_collision(entity, bullet);
			}
			break;
			
		case ENT_ROCKET:
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
				entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				bullet_rocket_collision(bullet, entity);
			}
			break;
			
		case ENT_MINE:
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
				entity->xdis, entity->ydis, MINE_RADIUS))
			{
				bullet_mine_collision(bullet, entity);
			}
			break;
			
		case ENT_RAILS:
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
				entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				bullet_rails_collision(bullet, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
				entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				destroy_shield(entity);
			}
			break;
		}
		
		struct entity_t *next = entity->next;
			
		if(entity->kill_me)
			remove_entity(sentity0, entity);
		else
			entity->in_tick = 0;
		
		if(bullet->kill_me)
			return;
		
		entity = next;
	}
	
	
	// check for collision with teleporter
	
/*	struct teleporter_t *teleporter = teleporter0;
	while(teleporter)
	{
		if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, 
			teleporter->x, teleporter->y, teleporter->radius))
		{
			get_teleporter_spawn_point(teleporter, &xdis, &ydis);
			
			#ifdef EMCLIENT
			bullet->bullet_data.old_xdis = xdis;
			bullet->bullet_data.old_ydis = ydis;
			#endif
			
			break;
		}
		
		teleporter = teleporter->next;
	}
	*/
	
	bullet->xdis = xdis;
	bullet->ydis = ydis;
}


void s_tick_rocket(struct entity_t *rocket)
{
	if(rocket->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - rocket->teleporting_tick) / 200.0;
		if(time > TELEPORT_FADE_TIME)
		{
			switch(rocket->teleporting)
			{
			case TELEPORTING_DISAPPEARING:
				rocket->teleporting = TELEPORTING_APPEARING;
				rocket->teleporting_tick = cgame_tick;

				get_spawn_point_coords(rocket->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				rocket->xdis = teleporter_x - rocket->xvel * TELEPORT_FADE_TIME * 200.0;
				rocket->ydis = teleporter_y - rocket->yvel * TELEPORT_FADE_TIME * 200.0;
				break;
			
			case TELEPORTING_APPEARING:
				rocket->teleporting = TELEPORTING_FINISHED;
				break;
			}
		}
		
		if(rocket->teleporting)
		{
			rocket->xdis += rocket->xvel;
			rocket->ydis += rocket->yvel;
			return;
		}
	}
	
	
	// see if we have left the launcher
	
	if(rocket->rocket_data.in_weapon)
	{
		struct entity_t *weapon = get_entity(*sentity0, rocket->rocket_data.weapon_id);
			
		if(!weapon)
			rocket->rocket_data.in_weapon = 0;
		else
			if(!circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, 
				weapon->xdis, weapon->ydis, WEAPON_RADIUS))
			{
				rocket->rocket_data.in_weapon = 0;
			}
	}

	
	
	double sin_theta, cos_theta;
	sincos(rocket->rocket_data.theta, &sin_theta, &cos_theta);
	
	if(cgame_tick < rocket->rocket_data.start_tick + ROCKET_THRUST_TIME)
	{
		rocket->xvel -= 0.20 * sin_theta;
		rocket->yvel += 0.20 * cos_theta;
	}
	
	apply_gravity_acceleration(rocket);
	
	slow_entity(rocket);
	

	while(1)
	{
		double xdis = rocket->xdis + rocket->xvel;
		double ydis = rocket->ydis + rocket->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, ROCKET_RADIUS);
			
		if(!node)
			node = line_walk_bsp_tree(rocket->xdis, rocket->ydis, xdis, ydis);
		
		if(node)
		{
			destroy_rocket(rocket);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = *sentity0;
		while(entity)
		{
			if(entity == rocket || entity->teleporting)
			{
				entity = entity->next;
				continue;
			}
		
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					destroy_rocket(rocket);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					if(!rocket->rocket_data.in_weapon || 
						rocket->rocket_data.weapon_id != entity->index)
					{
						destroy_rocket(rocket);
					}
				}
				break;
				
			case ENT_PLASMA:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, PLASMA_RADIUS))
				{
					plasma_rocket_collision(entity, rocket);
				}
				break;
				
			case ENT_BULLET:
				if(point_in_circle(entity->xdis, entity->ydis, 
					xdis, ydis, ROCKET_RADIUS))
				{
					bullet_rocket_collision(entity, rocket);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					rocket_rocket_collision(rocket, entity);
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, MINE_RADIUS))
				{
					rocket_mine_collision(rocket, entity);
				}
				break;
				
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					rocket_rails_collision(rocket, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, 
					entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					destroy_shield(entity);
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			if(entity->kill_me)
				remove_entity(sentity0, entity);
			else
				entity->in_tick = 0;
			
			if(rocket->kill_me)
				return;
			
			entity = next;
		}
		
		
		
		// check for collision with speedup ramp
		
		int s = 0;
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			if(cgame_tick - speedup_ramp->last_boost_tick < 200)
			{
				LL_NEXT(speedup_ramp);
				continue;
			}
		
		
		//	double sin_theta, cos_theta;
			sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, CRAFT_RADIUS))
			{
				s = 1;
				
				if(!rocket->speeding_up)
				{
					rocket->xvel -= speedup_ramp->boost * sin_theta;
					rocket->yvel += speedup_ramp->boost * cos_theta;
					
					speedup_ramp->last_boost_tick = cgame_tick;
					restart = 1;
					rocket->speeding_up = 1;
					#ifdef EMSERVER
					emit_speedup_to_all_players(speedup_ramp);
					#endif
					break;
				}
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		rocket->speeding_up = s;
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, ROCKET_RADIUS, 
				teleporter->x, teleporter->y, teleporter->radius))
			{
				rocket->teleport_spawn_index = teleporter->spawn_index;
				
				rocket->teleporting = TELEPORTING_DISAPPEARING;
				rocket->teleporting_tick = cgame_tick;
				
				#ifdef EMSERVER
				emit_teleport_to_all_players(rocket);
				#endif
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		
		
		
		if(restart)
			continue;

		
		#ifdef EMCLIENT
		
		if(cgame_tick < rocket->rocket_data.start_tick + ROCKET_THRUST_TIME)
			tick_rocket(rocket, xdis, ydis);
		
		#endif
		
		rocket->xdis = xdis;
		rocket->ydis = ydis;
		
		break;
	}
}


void s_tick_mine(struct entity_t *mine)
{
	if(mine->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - mine->teleporting_tick) / 200.0;
		if(time > TELEPORT_FADE_TIME)
		{
			switch(mine->teleporting)
			{
			case TELEPORTING_DISAPPEARING:
				mine->teleporting = TELEPORTING_APPEARING;
				mine->teleporting_tick = cgame_tick;
			
				get_spawn_point_coords(mine->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				mine->xdis = teleporter_x - mine->xvel * TELEPORT_FADE_TIME * 200.0;
				mine->ydis = teleporter_y - mine->yvel * TELEPORT_FADE_TIME * 200.0;
				break;
			
			case TELEPORTING_APPEARING:
				mine->teleporting = TELEPORTING_FINISHED;
				break;
			}
		}
		
		if(mine->teleporting)
		{
			mine->xdis += mine->xvel;
			mine->ydis += mine->yvel;
			return;
		}
	}

	
	// see if we are no longer under the craft
	
	if(mine->mine_data.under_craft)
	{
		struct entity_t *craft = get_entity(*sentity0, mine->mine_data.craft_id);
			
		if(!craft)
			mine->mine_data.under_craft = 0;
		else
			if(!circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS * 2, 
				craft->xdis, craft->ydis, CRAFT_RADIUS))
			{
				mine->mine_data.under_craft = 0;
			}
	}

	
	
	apply_gravity_acceleration(mine);
	
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);
	slow_entity(mine);

	
	while(1)
	{
		double xdis = mine->xdis + mine->xvel;
		double ydis = mine->ydis + mine->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, MINE_RADIUS);
			
		if(!node)
			node = line_walk_bsp_tree(mine->xdis, mine->ydis, xdis, ydis);
		
		if(node)
		{
			destroy_mine(mine);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = *sentity0;
		while(entity)
		{
			if(entity == mine || entity->teleporting)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					if(!mine->mine_data.under_craft || 
						mine->mine_data.craft_id != entity->index)
					{
						destroy_mine(mine);
					}
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					destroy_mine(mine);
				}
				break;
				
			case ENT_PLASMA:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, PLASMA_RADIUS))
				{
					plasma_mine_collision(entity, mine);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					rocket_mine_collision(entity, mine);
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, MINE_RADIUS))
				{
					mine_mine_collision(mine, entity);
				}
				break;
				
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					mine_rails_collision(mine, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, 
					entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					destroy_shield(entity);
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(sentity0, entity);
			else
				entity->in_tick = 0;

			if(mine->kill_me)
				return;
			
			entity = next;
		}
		
		
		// check for collision with speedup ramp
		
		int s = 0;
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			if(cgame_tick - speedup_ramp->last_boost_tick < 200)
			{
				LL_NEXT(speedup_ramp);
				continue;
			}
		
		
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, CRAFT_RADIUS))
			{
				s = 1;
				
				if(!mine->speeding_up)
				{
					mine->xvel -= speedup_ramp->boost * sin_theta;
					mine->yvel += speedup_ramp->boost * cos_theta;
					
					speedup_ramp->last_boost_tick = cgame_tick;
					restart = 1;
					mine->speeding_up = 1;
					#ifdef EMSERVER
					emit_speedup_to_all_players(speedup_ramp);
					#endif
					break;
				}
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		mine->speeding_up = s;
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, MINE_RADIUS, 
				teleporter->x, teleporter->y, teleporter->radius))
			{
				mine->teleport_spawn_index = teleporter->spawn_index;
				
				mine->teleporting = TELEPORTING_DISAPPEARING;
				mine->teleporting_tick = cgame_tick;
				
				#ifdef EMSERVER
				emit_teleport_to_all_players(mine);
				#endif
				
			//	restart = 1;
				break;
			}
			
			teleporter = teleporter->next;
		}

		if(restart)
			continue;
		
		
		mine->xdis = xdis;
		mine->ydis = ydis;
		
		break;
	}
}


void s_tick_rails(struct entity_t *rails)
{
	if(rails->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - rails->teleporting_tick) / 200.0;
		if(time > TELEPORT_FADE_TIME)
		{
			switch(rails->teleporting)
			{
			case TELEPORTING_DISAPPEARING:
				rails->teleporting = TELEPORTING_APPEARING;
				rails->teleporting_tick = cgame_tick;
			
				get_spawn_point_coords(rails->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				rails->xdis = teleporter_x - rails->xvel * TELEPORT_FADE_TIME * 200.0;
				rails->ydis = teleporter_y - rails->yvel * TELEPORT_FADE_TIME * 200.0;
				break;
			
			case TELEPORTING_APPEARING:
				rails->teleporting = TELEPORTING_FINISHED;
				break;
			}
		}
		
		if(rails->teleporting)
		{
			rails->xdis += rails->xvel;
			rails->ydis += rails->yvel;
			return;
		}
	}

	
	apply_gravity_acceleration(rails);
	
	slow_entity(rails);

	double xdis, ydis;
	int restart = 0;
	
	while(1)
	{
		// see if our iterative collison technique has locked
		
		if(restart && rails->xdis == xdis && rails->ydis == ydis)
		{
			#ifdef EMSERVER
			explode_rails(rails, NULL);
		//	char *msg = "infinite iteration rails collison broken\n";
		//	console_print(msg);
		//	print_on_all_players(msg);
			#endif
			
			#ifdef EMCLIENT
			// try to prevent this happening again until the server 
			// explodes the rails or history gets rewritten
			rails->xvel = rails->yvel = 0.0;
			#endif

			return;
		}
		
		xdis = rails->xdis + rails->xvel;
		ydis = rails->ydis + rails->yvel;
		
		restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, RAILS_RADIUS);
			
		if(!node)
			node = line_walk_bsp_tree(rails->xdis, rails->ydis, xdis, ydis);
		
		if(node)
		{
			#ifdef EMSERVER
			rails_force(rails, hypot(rails->xvel, rails->yvel) * VELOCITY_FORCE_MULTIPLIER, NULL);
			
			if(rails->kill_me)
				return;
			#endif
			
			entity_wall_bounce(rails, node);
			
			restart = 1;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = *sentity0;
		while(entity)
		{
			if(entity == rails || entity->teleporting)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, 
					entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_rails_collision(entity, rails);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, 
					entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					weapon_rails_collision(entity, rails);
					restart = 1;
				}
				break;
				
			case ENT_PLASMA:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, 
					entity->xdis, entity->ydis, PLASMA_RADIUS))
				{
					plasma_rails_collision(entity, rails);
				}
				break;
			
			case ENT_BULLET:
				if(point_in_circle(entity->xdis, entity->ydis, 
					rails->xdis, rails->ydis, RAILS_RADIUS))
				{
					bullet_rails_collision(entity, rails);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, 
					entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					rocket_rails_collision(entity, rails);
					restart = 1;
				}
				break;
			
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, 
					entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					rails_rails_collision(rails, entity);
					restart = 1;
				}
				break;
			
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, 
					entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					destroy_shield(entity);
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(sentity0, entity);
			else
				entity->in_tick = 0;

			if(rails->kill_me)
				return;
			
			entity = next;
		}
		
		
		// check for collision with speedup ramp
		
		int s = 0;
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, CRAFT_RADIUS))
			{
				s = 1;
				
				if(!rails->speeding_up)
				{
					rails->xvel -= speedup_ramp->boost * sin_theta;
					rails->yvel += speedup_ramp->boost * cos_theta;
					
					speedup_ramp->last_boost_tick = cgame_tick;
					restart = 1;
					rails->speeding_up = 1;
					#ifdef EMSERVER
					emit_speedup_to_all_players(speedup_ramp);
					#endif
					break;
				}
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		rails->speeding_up = s;
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, 
				RAILS_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				rails->teleport_spawn_index = teleporter->spawn_index;
				
				rails->teleporting = TELEPORTING_DISAPPEARING;
				rails->teleporting_tick = cgame_tick;
				
				#ifdef EMSERVER
				emit_teleport_to_all_players(rails);
				#endif
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		if(restart)
			continue;
		
		
		rails->xdis = xdis;
		rails->ydis = ydis;
		
		break;
	}
}


void s_tick_shield(struct entity_t *shield)
{
	if(shield->teleporting)
	{
		float teleporter_x, teleporter_y;
		double time = (double)(cgame_tick - shield->teleporting_tick) / 200.0;
		if(time > TELEPORT_FADE_TIME)
		{
			switch(shield->teleporting)
			{
			case TELEPORTING_DISAPPEARING:
				shield->teleporting = TELEPORTING_APPEARING;
				shield->teleporting_tick = cgame_tick;
			
				get_spawn_point_coords(shield->teleport_spawn_index, &teleporter_x, &teleporter_y);
			
				// this should really use descrete ticks
				shield->xdis = teleporter_x - shield->xvel * TELEPORT_FADE_TIME * 200.0;
				shield->ydis = teleporter_y - shield->yvel * TELEPORT_FADE_TIME * 200.0;
				break;
			
			case TELEPORTING_APPEARING:
				shield->teleporting = TELEPORTING_FINISHED;
				break;
			}
		}
		
		if(shield->teleporting)
		{
			shield->xdis += shield->xvel;
			shield->ydis += shield->yvel;
			return;
		}
	}

	
	apply_gravity_acceleration(shield);
	
	slow_entity(shield);

	
	while(1)
	{
		double xdis = shield->xdis + shield->xvel;
		double ydis = shield->ydis + shield->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, SHIELD_RADIUS);
			
		if(!node)
			node = line_walk_bsp_tree(shield->xdis, shield->ydis, xdis, ydis);
		
		if(node)
		{
			destroy_shield(shield);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = *sentity0;
		while(entity)
		{
			if(entity == shield || entity->teleporting)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, 
					entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_shield_collision(entity, shield);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, 
					entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					weapon_shield_collision(entity, shield);
				}
				break;
				
			case ENT_PLASMA:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, 
					entity->xdis, entity->ydis, PLASMA_RADIUS))
				{
					plasma_shield_collision(entity, shield);
				}
				break;
			
			case ENT_BULLET:
				if(point_in_circle(entity->xdis, entity->ydis, 
					xdis, ydis, SHIELD_RADIUS))
				{
					destroy_shield(entity);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, 
					entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					destroy_shield(shield);
				}
				break;
			
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, 
					entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					destroy_shield(shield);
				}
				break;
			
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, 
					entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					shield_shield_collision(shield, shield);
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(sentity0, entity);
			else
				entity->in_tick = 0;

			if(shield->kill_me)
				return;
			
			entity = next;
		}
		
		
		// check for collision with speedup ramp
		
		int s = 0;
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			if(cgame_tick - speedup_ramp->last_boost_tick < 200)
			{
				LL_NEXT(speedup_ramp);
				continue;
			}
		
		
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, CRAFT_RADIUS))
			{
				s = 1;
				
				if(!shield->speeding_up)
				{
					shield->xvel -= speedup_ramp->boost * sin_theta;
					shield->yvel += speedup_ramp->boost * cos_theta;
					
					speedup_ramp->last_boost_tick = cgame_tick;
					restart = 1;
					shield->speeding_up = 1;
					#ifdef EMSERVER
					emit_speedup_to_all_players(speedup_ramp);
					#endif
					break;
				}
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		shield->speeding_up = s;
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, 
				SHIELD_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				shield->teleport_spawn_index = teleporter->spawn_index;
				
				shield->teleporting = TELEPORTING_DISAPPEARING;
				shield->teleporting_tick = cgame_tick;
				
				#ifdef EMSERVER
				emit_teleport_to_all_players(shield);
				#endif
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		if(restart)
			continue;
		
		
		shield->xdis = xdis;
		shield->ydis = ydis;
		
		break;
	}
}


void s_tick_entities(struct entity_t **entity0)
{
	sentity0 = entity0;
	
	struct entity_t *centity = *sentity0;
	
	while(centity)
	{
		centity->in_tick = 1;
		
		switch(centity->type)
		{
		case ENT_CRAFT:
			s_tick_craft(centity);
			break;
		}
		
		if(centity->kill_me)
		{
			struct entity_t *next = centity->next;
			remove_entity(sentity0, centity);
			centity = next;
		}
		else
		{
			centity->in_tick = 0;
			centity = centity->next;
		}
	}

	
	centity = *sentity0;
	
	while(centity)
	{
		centity->in_tick = 1;
		
		switch(centity->type)
		{
		case ENT_WEAPON:
			s_tick_weapon(centity);
			break;
		}
		
		if(centity->kill_me)
		{
			struct entity_t *next = centity->next;
			remove_entity(sentity0, centity);
			centity = next;
		}
		else
		{
			centity->in_tick = 0;
			centity = centity->next;
		}
	}
	
	
	centity = *sentity0;
	
	while(centity)
	{
		centity->in_tick = 1;
		
		switch(centity->type)
		{
		case ENT_PLASMA:
			s_tick_plasma(centity);
			break;
			
		case ENT_ROCKET:
			s_tick_rocket(centity);
			break;
			
		case ENT_BULLET:
			s_tick_bullet(centity);
			break;
			
		case ENT_MINE:
			s_tick_mine(centity);
			break;
		
		case ENT_RAILS:
			s_tick_rails(centity);
			break;
		
		case ENT_SHIELD:
			s_tick_shield(centity);
			break;
		}
		
		if(centity->kill_me)
		{
			struct entity_t *next = centity->next;
			remove_entity(sentity0, centity);
			centity = next;
		}
		else
		{
			centity->in_tick = 0;
			centity = centity->next;
		}
	}
}
