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

#include "../../common/types.h"
#include "../../common/minmax.h"
#include "../../common/llist.h"
#include "../../common/stringbuf.h"
#include "../../common/buffer.h"
#include "sgame.h"
#include "bsp.h"
#include "objects.h"

#ifdef EMSERVER
#include "../game.h"
#include "../console.h"
#endif

#ifdef EMCLIENT
#include "../game.h"
#include "../console.h"
#endif

struct spawn_point_t *spawn_point0 = NULL;
struct teleporter_t *teleporter0 = NULL;
struct speedup_ramp_t *speedup_ramp0 = NULL;
struct gravity_well_t *gravity_well0 = NULL;

struct entity_t *sentity0 = NULL;
	
int nextentity = 0;

#define SPACE_FRICTION		0.99935
#define COLLISION_FRICTION	0.75


#define BOGIE_DAMAGE	10.0
#define BULLET_DAMAGE	5.0

#define ROCKET_FORCE_THRESHOLD	50.0
#define MINE_FORCE_THRESHOLD	60.0
#define RAILS_FORCE_THRESHOLD	20.0
#define SHIELD_FORCE_THRESHOLD	20.0

#define CRAFT_SPLASH_FORCE	200.0
#define WEAPON_SPLASH_FORCE	200.0
#define ROCKET_SPLASH_FORCE	4000.0
#define MINE_SPLASH_FORCE	150.0
#define RAILS_SPLASH_FORCE	5.0

#define VELOCITY_FORCE_MULTIPLIER	0.05
#define FORCE_VELOCITY_MULTIPLIER	(1.0 / VELOCITY_FORCE_MULTIPLIER)

#define IDEAL_CRAFT_WEAPON_DIST			100.0
#define MAX_CRAFT_WEAPON_DIST			150.0
#define MAX_WEAPON_ROTATE_SPEED			0.075
#define MAX_WEAPON_MOVE_IN_OUT_SPEED	0.4

#define ROCKET_THRUST_TIME 400


int read_spawn_point(gzFile file)
{
	struct spawn_point_t spawn_point;
	
	if(gzread(file, &spawn_point.x, 8) != 8)
		goto error;
	
	if(gzread(file, &spawn_point.y, 8) != 8)
		goto error;
	
	if(gzread(file, &spawn_point.angle, 8) != 8)
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
		
	if(gzread(file, &speedup_ramp.x, 8) != 8)
		goto error;
	
	if(gzread(file, &speedup_ramp.y, 8) != 8)
		goto error;
	
	if(gzread(file, &speedup_ramp.theta, 8) != 8)
		goto error;
	
	if(gzread(file, &speedup_ramp.width, 8) != 8)
		goto error;
	
	if(gzread(file, &speedup_ramp.boost, 8) != 8)
		goto error;
	
	LL_ADD(struct speedup_ramp_t, &speedup_ramp0, &speedup_ramp);
	
	return 1;
	
error:
	
	return 0;
}


int read_teleporter(gzFile file)
{
	struct teleporter_t teleporter;
		
	if(gzread(file, &teleporter.x, 8) != 8)
		goto error;
	
	if(gzread(file, &teleporter.y, 8) != 8)
		goto error;
	
	if(gzread(file, &teleporter.radius, 8) != 8)
		goto error;
	
	if(gzread(file, &teleporter.spawn_index, 4) != 4)
		goto error;
	
	LL_ADD(struct teleporter_t, &teleporter0, &teleporter);
	
	return 1;
	
error:
	
	return 0;
}


int read_gravity_well(gzFile file)
{
	struct gravity_well_t gravity_well;
		
	if(gzread(file, &gravity_well.x, 8) != 8)
		goto error;
	
	if(gzread(file, &gravity_well.y, 8) != 8)
		goto error;
	
	if(gzread(file, &gravity_well.strength, 8) != 8)
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
	
	return entity;
}


void remove_entity(struct entity_t **entity0, struct entity_t *entity)
{
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
	sentity0 = *entity0;
}


struct entity_t *get_entity(struct entity_t *entity0, uint32_t index)
{
	struct entity_t *entity = entity0;
	
	while(entity)
	{
		if(entity->index == index)
			return entity;

		entity = entity->next;
	}

	return NULL;
}


int point_in_circle(double px, double py, double cx, double cy, double cr)
{
	double xdist = px - cx;
	double ydist = py - cy;
	
	return xdist * xdist + ydist * ydist < cr * cr;
}


int line_in_circle(double lx1, double ly1, double lx2, double ly2, double cx, double cy, double cr)
{
	double x1 = lx1 - cx;
	double y1 = ly1 - cy;
	
	if(x1 * x1 + y1 * y1 < cr * cr)
		return 1;
	
	double x2 = lx2 - cx;
	double y2 = ly2 - cy;
	
	if(x2 * x2 + y2 * y2 < cr * cr)
		return 1;

	// TODO : use closest distance?
	
	double dx = x2 - x1;
	double dy = y2 - y1;
	double dr2 = dx * dx + dy * dy;
	double D = x1 * y2 - x2 * y1;
	double discr = cr * cr * dr2 - D * D;

	if(discr >= 0.0)
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
	if(weapon->weapon_data.craft)
	{
		if(weapon->weapon_data.craft->craft_data.left_weapon == weapon)
			weapon->weapon_data.craft->craft_data.left_weapon = NULL;
		
		if(weapon->weapon_data.craft->craft_data.right_weapon == weapon)
			weapon->weapon_data.craft->craft_data.right_weapon = NULL;
		
		weapon->weapon_data.craft = NULL;
	}
}

	
void explode_craft(struct entity_t *craft);
void craft_force(struct entity_t *craft, double force)
{
	#ifdef EMSERVER
	craft->craft_data.shield_strength -= force;
	if(craft->craft_data.shield_strength < 0.0)
	{
		if(!craft->craft_data.carcass)
		{
			respawn_craft(craft);
			
			if(craft->craft_data.shield_strength > -0.25)
			{
				craft->craft_data.carcass = 1;
				strip_weapons_from_craft(craft);
				make_carcass_on_all_players(craft);
			}
			else
			{
				explode_craft(craft);
			}
		}
		else
		{
			if(craft->craft_data.shield_strength < -0.25)
				explode_craft(craft);
		}
	}
	else
	{
		craft->craft_data.shield_flare += force * 8.0;
		craft->craft_data.shield_flare = min(craft->craft_data.shield_flare, 1.0);
		craft->propagate_me = 1;
	}
	#endif
}


void explode_weapon(struct entity_t *weapon);
void weapon_force(struct entity_t *weapon, double force)
{
	#ifdef EMSERVER
	weapon->weapon_data.shield_strength -= force;
	if(weapon->weapon_data.shield_strength < 0.0)
		explode_weapon(weapon);
	else
	{
		weapon->weapon_data.shield_flare += force;
		weapon->weapon_data.shield_flare = min(weapon->weapon_data.shield_flare, 1.0);
		weapon->propagate_me = 1;
	}
	#endif
}


void explode_rocket(struct entity_t *rocket);
void rocket_force(struct entity_t *rocket, double force)
{
	if(force > ROCKET_FORCE_THRESHOLD)
		explode_rocket(rocket);
}


void explode_mine(struct entity_t *mine);
void mine_force(struct entity_t *mine, double force)
{
	if(force > MINE_FORCE_THRESHOLD)
		explode_mine(mine);
}


void explode_rails(struct entity_t *rails);
void rails_force(struct entity_t *rails, double force)
{
	if(force > RAILS_FORCE_THRESHOLD)
		explode_rails(rails);
}


void shield_force(struct entity_t *shield, double force)
{
	if(force > SHIELD_FORCE_THRESHOLD)
		shield->kill_me = 1;
}


#ifdef EMSERVER

void splash_force(double x, double y, double force)
{
	struct entity_t *entity = sentity0;
	
	while(entity)
	{
		if(line_walk_bsp_tree(x, y, entity->xdis, entity->ydis))
		{
			entity = entity->next;
			continue;
		}
		
		struct entity_t *next;
		
		if(!entity->kill_me)
		{
			double xdist = entity->xdis - x;
			double ydist = entity->ydis - y;
			double dist_squared = xdist * xdist + ydist * ydist;
			double dist = sqrt(dist_squared);
			double att_force = force / dist_squared;
			
			entity->xvel += (xdist / dist) * att_force * FORCE_VELOCITY_MULTIPLIER;
			entity->yvel += (xdist / dist) * att_force * FORCE_VELOCITY_MULTIPLIER;

			int in_tick = entity->in_tick;
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				craft_force(entity, att_force);
				break;
			
			case ENT_WEAPON:
				weapon_force(entity, att_force);
				break;
			
			case ENT_ROCKET:
				rocket_force(entity, att_force);
				break;
			
			case ENT_MINE:
				mine_force(entity, att_force);
				break;
			
			case ENT_RAILS:
				rails_force(entity, att_force);
				break;
			
			case ENT_SHIELD:
				shield_force(entity, att_force);
				break;
			}
			
			next = entity->next;
			
			if(!in_tick && entity->kill_me)
				remove_entity(&sentity0, entity);
			else
			{
				entity->propagate_me = 1;
				entity->in_tick = in_tick;
			}
		}
		else
		{
			next = entity->next;
		}
		
		entity = next;
	}
}


void explode_craft(struct entity_t *craft)
{
	craft->kill_me = 1;
	strip_weapons_from_craft(craft);
	remove_entity_from_all_players(craft);
	splash_force(craft->xdis, craft->ydis, CRAFT_SPLASH_FORCE);
}


void explode_weapon(struct entity_t *weapon)
{
	weapon->kill_me = 1;
	strip_craft_from_weapon(weapon);
	remove_entity_from_all_players(weapon);
	splash_force(weapon->xdis, weapon->ydis, WEAPON_SPLASH_FORCE);
}


#endif



void explode_rocket(struct entity_t *rocket)
{
	#ifdef EMCLIENT
	explosion(rocket);
	#endif
	
	rocket->kill_me = 1;
	
	#ifdef EMSERVER
	splash_force(rocket->xdis, rocket->ydis, ROCKET_SPLASH_FORCE);
	#endif
}


void explode_mine(struct entity_t *mine)
{
	#ifdef EMCLIENT
	explosion(mine);
	#endif
	
	mine->kill_me = 1;

	#ifdef EMSERVER
	splash_force(mine->xdis, mine->ydis, MINE_SPLASH_FORCE);
	#endif
}


void explode_rails(struct entity_t *rails)
{
	#ifdef EMCLIENT
	explosion(rails);
	#endif
	
	rails->kill_me = 1;

	#ifdef EMSERVER
	splash_force(rails->xdis, rails->ydis, RAILS_SPLASH_FORCE);
	#endif
}


void craft_craft_collision(struct entity_t *craft1, struct entity_t *craft2)
{
	craft_force(craft1, hypot(craft2->xvel, craft2->yvel) * VELOCITY_FORCE_MULTIPLIER);
	
	if(!craft2->kill_me)
		craft_force(craft2, hypot(craft1->xvel, craft1->yvel) * VELOCITY_FORCE_MULTIPLIER);
	
	if(!craft1->kill_me && !craft2->kill_me)
		equal_bounce(craft1, craft2);
}


void craft_weapon_collision(struct entity_t *craft, struct entity_t *weapon)
{
	craft_force(craft, hypot(weapon->xvel, weapon->yvel) * (WEAPON_MASS / CRAFT_MASS) * 
		VELOCITY_FORCE_MULTIPLIER);
	
	if(!weapon->kill_me)
		weapon_force(weapon, hypot(craft->xvel, craft->yvel) * (CRAFT_MASS / WEAPON_MASS) * 
			VELOCITY_FORCE_MULTIPLIER);
	
	if(!craft->kill_me && !weapon->kill_me)
		craft_weapon_bounce(craft, weapon);
}


void craft_bogie_collision(struct entity_t *craft, struct entity_t *bogie)
{
	craft_force(craft, BOGIE_DAMAGE);
	bogie->kill_me = 1;
}


void craft_bullet_collision(struct entity_t *craft, struct entity_t *bullet)
{
	craft_force(craft, BULLET_DAMAGE);
	bullet->kill_me = 1;
}


void craft_rails_collision(struct entity_t *craft, struct entity_t *rails)
{
	#ifdef EMSERVER
	#endif
	
	rails->kill_me = 1;
}


void craft_shield_collision(struct entity_t *craft, struct entity_t *shield)
{
	#ifdef EMSERVER
	#endif
	
	shield->kill_me = 1;
}


void weapon_weapon_collision(struct entity_t *weapon1, struct entity_t *weapon2)
{
	weapon_force(weapon1, hypot(weapon2->xvel, weapon2->yvel) * VELOCITY_FORCE_MULTIPLIER);
	if(!weapon2->kill_me)
		weapon_force(weapon2, hypot(weapon1->xvel, weapon1->yvel) * VELOCITY_FORCE_MULTIPLIER);
	
	if(!weapon1->kill_me && !weapon2->kill_me)
		equal_bounce(weapon1, weapon2);
}


void weapon_bogie_collision(struct entity_t *weapon, struct entity_t *bogie)
{
	weapon_force(weapon, BOGIE_DAMAGE);
	bogie->kill_me = 1;
}


void weapon_bullet_collision(struct entity_t *weapon, struct entity_t *bullet)
{
	weapon_force(weapon, BULLET_DAMAGE);
	bullet->kill_me = 1;
}


void weapon_shield_collision(struct entity_t *weapon, struct entity_t *shield)
{
	#ifdef EMSERVER
	#endif
	
	shield->kill_me = 1;
}


void bogie_rocket_collision(struct entity_t *bogie, struct entity_t *rocket)
{
	bogie->kill_me = 1;
	explode_rocket(rocket);
}


void bogie_mine_collision(struct entity_t *bogie, struct entity_t *mine)
{
	bogie->kill_me = 1;
	explode_mine(mine);
}


void bogie_rails_collision(struct entity_t *bogie, struct entity_t *rails)
{
	bogie->kill_me = 1;
	explode_rails(rails);
}


void bogie_shield_collision(struct entity_t *bogie, struct entity_t *shield)
{
	bogie->kill_me = 1;
	shield->kill_me = 1;
}


void bullet_rocket_collision(struct entity_t *bullet, struct entity_t *rocket)
{
	bullet->kill_me = 1;
	explode_rocket(rocket);
}


void bullet_mine_collision(struct entity_t *bullet, struct entity_t *mine)
{
	bullet->kill_me = 1;
	explode_mine(mine);
}


void bullet_rails_collision(struct entity_t *bullet, struct entity_t *rails)
{
	bullet->kill_me = 1;
	explode_rails(rails);
}


void rocket_rocket_collision(struct entity_t *rocket1, struct entity_t *rocket2)
{
	explode_rocket(rocket1);
	if(!rocket2->kill_me)
		explode_rocket(rocket2);
}


void rocket_mine_collision(struct entity_t *rocket, struct entity_t *mine)
{
	explode_rocket(rocket);
	if(!mine->kill_me)
		explode_mine(mine);
}


void rocket_rails_collision(struct entity_t *rocket, struct entity_t *rails)
{
	explode_rocket(rocket);
}


void mine_mine_collision(struct entity_t *mine1, struct entity_t *mine2)
{
	explode_mine(mine1);
	if(!mine2->kill_me)
		explode_mine(mine2);
}


void mine_rails_collision(struct entity_t *mine, struct entity_t *rails)
{
	explode_mine(mine);
}


void rails_rails_collision(struct entity_t *rails1, struct entity_t *rails2)
{
	explode_rails(rails1);
	if(!rails2->kill_me)
		explode_rails(rails2);
}


void shield_shield_collision(struct entity_t *shield1, struct entity_t *shield2)
{
	shield1->kill_me = 1;
	shield2->kill_me = 1;
}


void telefrag_craft(struct entity_t *craft)
{
	#ifdef EMSERVER
//	explode_craft(craft);
//	craft_telefragged(craft);
	#endif
	
	#ifdef EMCLIENT
//		explode_craft(craft);
	#endif
}


void telefrag_weapon(struct entity_t *weapon)
{
	#ifdef EMSERVER
//	explode_weapon(weapon);
//	weapon_telefragged(weapon);
	#endif
	
	#ifdef EMCLIENT
//		explode_weapon(weapon);
	#endif
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


void check_craft_teleportation(struct entity_t *craft)
{
	// check for overlap with other entities
	
	struct entity_t *entity = sentity0;
	while(entity)
	{
		if(entity == craft)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				telefrag_craft(entity);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				telefrag_weapon(entity);
			}
			break;
			
		case ENT_BOGIE:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
			{
				craft_bogie_collision(craft, entity);
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, craft->xdis, craft->ydis, CRAFT_RADIUS))
			{
				craft_bullet_collision(craft, entity);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				explode_rocket(entity);
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
			{
				explode_mine(entity);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				explode_rails(entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(craft->xdis, craft->ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				craft_shield_collision(craft, entity);
			}
			break;
		}
		
		struct entity_t *next = entity->next;
		
		if(entity->kill_me)
;//			remove_entity(entity);
		else
			entity->in_tick = 0;
		
		if(craft->kill_me)
			return;
		
		entity = next;
	}
}


void check_weapon_teleportation(struct entity_t *weapon)
{
	// check for overlap with other entities
	
	struct entity_t *entity = sentity0;
	while(entity)
	{
		if(entity == weapon)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				telefrag_craft(entity);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				telefrag_weapon(entity);
			}
			break;
			
		case ENT_BOGIE:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
			{
				weapon_bogie_collision(weapon, entity);
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, weapon->xdis, weapon->ydis, WEAPON_RADIUS))
			{
				weapon_bullet_collision(weapon, entity);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				explode_rocket(entity);
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
			{
				explode_mine(entity);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				explode_rails(entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(weapon->xdis, weapon->ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				weapon_shield_collision(weapon, entity);
			}
		}
		
		struct entity_t *next = entity->next;
		
		if(entity->kill_me)
;//			remove_entity(entity);
		else
			entity->in_tick = 0;
		
		if(weapon->kill_me)
			return;
		
		entity = next;
	}
}


void check_rocket_teleportation(struct entity_t *rocket)
{
	// check for overlap with other entities
	
	struct entity_t *entity = sentity0;
	while(entity)
	{
		if(entity == rocket)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				explode_rocket(rocket);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				explode_rocket(rocket);
			}
			break;
			
		case ENT_BOGIE:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
			{
				bogie_rocket_collision(entity, rocket);
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, rocket->xdis, rocket->ydis, ROCKET_RADIUS))
			{
				bullet_rocket_collision(entity, rocket);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				rocket_rocket_collision(rocket, entity);
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
			{
				rocket_mine_collision(rocket, entity);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				rocket_rails_collision(rocket, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				entity->kill_me = 1;		
			}
		}
		
		struct entity_t *next = entity->next;
		
		if(entity->kill_me)
;//			remove_entity(entity);
		else
			entity->in_tick = 0;
		
		if(rocket->kill_me)
			return;
		
		entity = next;
	}
}


void check_mine_teleportation(struct entity_t *mine)
{
	// check for overlap with other entities
	
	struct entity_t *entity = sentity0;
	while(entity)
	{
		if(entity == mine)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				explode_mine(mine);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				explode_mine(mine);
			}
			break;
			
		case ENT_BOGIE:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
			{
				bogie_mine_collision(entity, mine);
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, mine->xdis, mine->ydis, MINE_RADIUS))
			{
				bullet_mine_collision(entity, mine);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				rocket_mine_collision(entity, mine);
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
			{
				mine_mine_collision(mine, entity);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				mine_rails_collision(mine, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(mine->xdis, mine->ydis, MINE_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				entity->kill_me = 1;
			}
		}
		
		struct entity_t *next = entity->next;
		
		if(entity->kill_me)
;//			remove_entity(entity);
		else
			entity->in_tick = 0;
		
		if(mine->kill_me)
			return;
		
		entity = next;
	}
}


void check_rails_teleportation(struct entity_t *rails)
{
	// check for overlap with other entities
	
	struct entity_t *entity = sentity0;
	while(entity)
	{
		if(entity == rails)
		{
			entity = entity->next;
			continue;
		}
		
		entity->in_tick = 1;
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
			{
				explode_rails(rails);
			}
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
			{
				explode_rails(rails);
			}
			break;
			
		case ENT_BOGIE:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
			{
				bogie_rails_collision(entity, rails);
			}
			break;
			
		case ENT_BULLET:
			if(point_in_circle(entity->xdis, entity->ydis, rails->xdis, rails->ydis, RAILS_RADIUS))
			{
				bullet_rails_collision(entity, rails);
			}
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
			{
				rocket_rails_collision(entity, rails);
			}
			break;
			
		case ENT_MINE:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
			{
				mine_rails_collision(entity, rails);
			}
			break;
			
		case ENT_RAILS:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
			{
				rails_rails_collision(rails, entity);
			}
			break;
			
		case ENT_SHIELD:
			if(circles_intersect(rails->xdis, rails->ydis, RAILS_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
			{
				entity->kill_me = 1;
			}
		}
		
		struct entity_t *next = entity->next;
		
		if(entity->kill_me)
;//			remove_entity(entity);
		else
			entity->in_tick = 0;
		
		if(rails->kill_me)
			return;
		
		entity = next;
	}
}


void s_tick_craft(struct entity_t *craft)
{
	craft->craft_data.shield_flare = max(0.0, craft->craft_data.shield_flare - 0.005);
	
	while(craft->craft_data.theta >= M_PI)
		craft->craft_data.theta -= 2 * M_PI;
	
	while(craft->craft_data.theta < -M_PI)
		craft->craft_data.theta += 2 * M_PI;

	if(!craft->craft_data.carcass)
	{
		double sin_theta, cos_theta;
		sincos(craft->craft_data.theta, &sin_theta, &cos_theta);
		
		craft->xvel -= craft->craft_data.acc * sin_theta;
		craft->yvel += craft->craft_data.acc * cos_theta;
	}

//	apply_gravity_acceleration(craft);
	slow_entity(craft);


	int restart = 0;
	double xdis;
	double ydis;
	
	while(1)
	{
		// see if our iterative collison technique has locked
		
		if(restart && craft->xdis == xdis && craft->ydis == ydis)
		{
			#ifdef EMSERVER
			respawn_craft(craft);
			explode_craft(craft);
			char *msg = "infinite iteration craft collison broken\n";
			console_print(msg);
			print_on_all_players(msg);
			#endif
			
			#ifdef EMCLIENT
			// try to prevent this happening again until the server 
			// explodes the craft or history gets rewritten
			craft->xvel = craft->yvel = 0.0;
			craft->craft_data.acc = 0.0;
			#endif

			return;
		}
		
		xdis = craft->xdis + craft->xvel;
		ydis = craft->ydis + craft->yvel;
		
		restart = 0;
		
		
		// check for wall collision
		
		double t;
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, CRAFT_RADIUS, &t);
		if(node)
		{
			craft_force(craft, hypot(craft->xvel, craft->yvel) * VELOCITY_FORCE_MULTIPLIER);
			
			#ifdef EMSERVER
			if(craft->kill_me)
				return;
			#endif
			
			entity_wall_bounce(craft, node);
			
			restart = 1;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == craft)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_craft_collision(craft, entity);
					restart = 1;
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					craft_weapon_collision(craft, entity);
					restart = 1;
				}
				break;
				
			case ENT_BOGIE:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				{
					craft_bogie_collision(craft, entity);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					explode_rocket(entity);
					restart = 1;
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
				{
					explode_mine(entity);
					restart = 1;
				}
				break;
				
		/*	case ENT_RAILS:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					craft_rails_collision(craft, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, CRAFT_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					craft_shield_collision(craft, entity);
				}
				break;
		*/	}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(&sentity0, entity);
			else
				entity->in_tick = 0;

			#ifdef EMSERVER
			if(craft->kill_me)
				return;
			#endif
			
			entity = next;
		}
		
	/*	
		// check for collision with speedup ramp
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta + M_PI / 2.0, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, CRAFT_RADIUS))
			{
				sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
				
				craft->xvel += speedup_ramp->boost * sin_theta;
				craft->yvel += speedup_ramp->boost * cos_theta;
				
				if(craft->craft_data.left_weapon)
				{
					craft->craft_data.left_weapon->xvel += speedup_ramp->boost * sin_theta;
					craft->craft_data.left_weapon->yvel += speedup_ramp->boost * cos_theta;
				}
				
				if(craft->craft_data.right_weapon)
				{
					craft->craft_data.right_weapon->xvel += speedup_ramp->boost * sin_theta;
					craft->craft_data.right_weapon->yvel += speedup_ramp->boost * cos_theta;
				}
					
				restart = 1;
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, CRAFT_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				double teleporter_x, teleporter_y;
				get_teleporter_spawn_point(teleporter, &teleporter_x, &teleporter_y);
				
				if(craft->craft_data.left_weapon)
				{
					craft->craft_data.left_weapon->xdis += teleporter_x - craft->xdis;
					craft->craft_data.left_weapon->ydis += teleporter_y - craft->ydis;
				}
				
				if(craft->craft_data.right_weapon)
				{
					craft->craft_data.right_weapon->xdis += teleporter_x - craft->xdis;
					craft->craft_data.right_weapon->ydis += teleporter_y - craft->ydis;
				}
				
				craft->xdis = teleporter_x;
				craft->ydis = teleporter_y;
				
				check_craft_teleportation(craft);
				
				if(craft->craft_data.left_weapon)
				{
					craft->craft_data.left_weapon->in_tick = 1;
					check_weapon_teleportation(craft->craft_data.left_weapon);
					
					if(craft->craft_data.left_weapon->kill_me)
					{
;//						remove_entity(craft->craft_data.left_weapon);
						craft->craft_data.left_weapon = NULL;
					}
					else
					{
						craft->craft_data.left_weapon->in_tick = 0;
					}
				}
				
				if(craft->craft_data.right_weapon)
				{
					craft->craft_data.right_weapon->in_tick = 1;
					check_weapon_teleportation(craft->craft_data.right_weapon);
					
					if(craft->craft_data.right_weapon->kill_me)
					{
;//						remove_entity(craft->craft_data.right_weapon);
						craft->craft_data.right_weapon = NULL;
					}
					else
					{
						craft->craft_data.right_weapon->in_tick = 0;
					}
					
				}
				
				#ifdef EMSERVER
				if(craft->kill_me)
				{
;//					remove_entity(craft);
					return;
				}
				#endif
				
				restart = 1;
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		*/
		
		if(restart)
			continue;
		
		
		#ifdef EMCLIENT
		
		tick_craft(craft, xdis, ydis);
		
		#endif
		
		
		craft->xdis = xdis;
		craft->ydis = ydis;
		
		break;
	}
}


int check_weapon_placement(double xdis, double ydis, struct entity_t *weapon)
{
	// check for collision with wall
	
	if(circle_walk_bsp_tree(xdis, ydis, WEAPON_RADIUS, NULL))
		return 0;
	
	
	// check for collision with other entities
	
	struct entity_t *entity = sentity0;
	while(entity)
	{
		if(entity == weapon)
		{
			entity = entity->next;
			continue;
		}
		
		switch(entity->type)
		{
		case ENT_CRAFT:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				return 0;
			break;
			
		case ENT_WEAPON:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				return 0;
			break;
			
		case ENT_BOGIE:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				return 0;
			break;
			
		case ENT_BULLET:
			if(point_in_circle(xdis, ydis, weapon->xdis, weapon->ydis, WEAPON_RADIUS))
				return 0;
			break;
			
		case ENT_ROCKET:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				return 0;
			break;
			
		case ENT_MINE:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
				return 0;
			break;
			
		case ENT_RAILS:
			if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				return 0;
			break;
		}
		
		entity = entity->next;
	}
	
	return 1;
}


void s_tick_weapon(struct entity_t *weapon)
{
	weapon->weapon_data.shield_flare = max(0.0, weapon->weapon_data.shield_flare - 0.005);
	
	struct entity_t *craft = weapon->weapon_data.craft;
	
	
	if(craft)
	{
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
		
		double theta_side = 0.0;
		
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
		
		
		weapon->xvel = xdis - weapon->xdis;
		weapon->yvel = ydis - weapon->ydis;
	
	//	apply_gravity_acceleration(weapon);
	//	slow_entity(weapon);
	}
	else
	{
	//	apply_gravity_acceleration(weapon);
		slow_entity(weapon);
	}
	

	int restart = 0;
	double xdis;
	double ydis;
	
	while(1)
	{
		// see if our iterative collison technique has broken
		
		if(restart && weapon->xdis == xdis && weapon->ydis == ydis)
		{
			#ifdef EMSERVER
			explode_weapon(weapon);
			char *msg = "infinite iteration weapon collison broken\n";
			console_print(msg);
			print_on_all_players(msg);
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
		
		double t;
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, WEAPON_RADIUS, &t);
		if(node)
		{
			weapon_force(weapon, hypot(weapon->xvel, weapon->yvel) * VELOCITY_FORCE_MULTIPLIER);
			
			#ifdef EMSERVER
			if(weapon->kill_me)
				return;
			#endif
			
			entity_wall_bounce(weapon, node);
			
			restart = 1;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == weapon)
			{
				entity = entity->next;
				continue;
			}
		
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_weapon_collision(entity, weapon);
					restart = 1;
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					weapon_weapon_collision(weapon, entity);
					restart = 1;
				}
				break;
				
			case ENT_BOGIE:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				{
					if(!entity->plasma_data.in_weapon || entity->plasma_data.weapon_id != weapon->index)
					{
						weapon_bogie_collision(weapon, entity);
						restart = 1;
					}
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					if(!entity->rocket_data.in_weapon || entity->rocket_data.weapon_id != weapon->index)
					{
						explode_rocket(entity);
						restart = 1;
					}
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
				{
					explode_mine(entity);
					restart = 1;
				}
				break;
				
	/*		case ENT_RAILS:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					explode_rails(entity);
					restart = 1;
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, WEAPON_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					weapon_shield_collision(weapon, entity);
				}
				break;
	*/		}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(&sentity0, entity);
			else
				entity->in_tick = 0;
			
			#ifdef EMSERVER
			if(weapon->kill_me)
				return;
			#endif
			
			entity = next;
		}
		
	/*	
		if(!craft)
		{
			// check for collision with speedup ramp
			
			struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
			while(speedup_ramp)
			{
				double sin_theta, cos_theta;
				sincos(speedup_ramp->theta + M_PI / 2.0, &sin_theta, &cos_theta);
				
				if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
					speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
					speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
					speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
					xdis, ydis, WEAPON_RADIUS))
				{
					sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
					
					weapon->xvel += speedup_ramp->boost * sin_theta;
					weapon->yvel += speedup_ramp->boost * cos_theta;
					
					restart = 1;
				}			
				
				speedup_ramp = speedup_ramp->next;
			}
			
			
			// check for collision with teleporter
			
			struct teleporter_t *teleporter = teleporter0;
			while(teleporter)
			{
				if(circle_in_circle(xdis, ydis, WEAPON_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
				{
					get_teleporter_spawn_point(teleporter, &weapon->xdis, &weapon->ydis);
					
					check_weapon_teleportation(weapon);
					
					#ifdef EMSERVER
					if(weapon->kill_me)
					{
;//						remove_entity(weapon);
						return;
					}
					#endif
					
					restart = 1;
					break;
				}
				
				teleporter = teleporter->next;
			}
		}
		*/
		
		if(restart)
			continue;
		
		
		weapon->xdis = xdis;
		weapon->ydis = ydis;
		
		break;
	}
	
	if(!craft)
	{
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity->type == ENT_CRAFT && !entity->craft_data.carcass && 
				entity->craft_data.left_weapon != weapon && entity->craft_data.right_weapon != weapon)
			{
				if(point_in_circle(weapon->xdis, weapon->ydis, entity->xdis, entity->ydis, MAX_CRAFT_WEAPON_DIST))
				{
					double delta = atan2(-(weapon->xdis - entity->xdis), weapon->ydis - entity->ydis) 
						- entity->craft_data.theta;
					
					while(delta >= M_PI)
						delta -= M_PI * 2.0;
					while(delta < -M_PI)
						delta += M_PI * 2.0;
					
					if(delta > 0.0 && !entity->craft_data.left_weapon)
					{
						entity->craft_data.left_weapon = weapon;
						weapon->weapon_data.craft = entity;
						craft = entity;
						break;
					}
					
					if(delta < 0.0 && !entity->craft_data.right_weapon)
					{
						entity->craft_data.right_weapon = weapon;
						weapon->weapon_data.craft = entity;
						craft = entity;
						break;
					}
				}
			}
			
			entity = entity->next;
		}
	}
	else
	{
		if(!point_in_circle(weapon->xdis, weapon->ydis, craft->xdis, craft->ydis, MAX_CRAFT_WEAPON_DIST))
		{
			if(craft->craft_data.left_weapon == weapon)
				craft->craft_data.left_weapon = NULL;
			else
				craft->craft_data.right_weapon = NULL;
			
			weapon->weapon_data.craft = NULL;
			craft = NULL;
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
					craft->craft_data.left_weapon = NULL;
					weapon->weapon_data.craft = NULL;
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
					craft->craft_data.right_weapon = NULL;
					weapon->weapon_data.craft = NULL;
				}
			}
		}
	}
}


void s_tick_bogie(struct entity_t *bogie)
{
	// see if we have left the cannon
	
	if(bogie->plasma_data.in_weapon)
	{
		struct entity_t *weapon = get_entity(sentity0, bogie->plasma_data.weapon_id);
			
		if(!weapon)
			bogie->plasma_data.in_weapon = 0;
		else
			if(!circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, weapon->xdis, weapon->ydis, WEAPON_RADIUS))
			{
				bogie->plasma_data.in_weapon = 0;
			}
	}
			
			
			

	apply_gravity_acceleration(bogie);
	
	
	bogie->xdis += bogie->xvel;
	bogie->ydis += bogie->yvel;
	
	while(1)
	{
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(bogie->xdis, bogie->ydis, BOGIE_RADIUS, NULL);
		if(node)
		{
			bogie->kill_me = 1;
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == bogie)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_bogie_collision(entity, bogie);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					if(!bogie->plasma_data.in_weapon || bogie->plasma_data.weapon_id != entity->index)
						weapon_bogie_collision(entity, bogie);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					bogie_rocket_collision(bogie, entity);
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
				{
					bogie_mine_collision(bogie, entity);
				}
				break;
				
		/*	case ENT_RAILS:
				if(circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					bogie_rails_collision(bogie, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(bogie->xdis, bogie->ydis, BOGIE_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					bogie_shield_collision(bogie, entity);
				}
				break;
		*/	}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(&sentity0, entity);
			else
				entity->in_tick = 0;
			
			if(bogie->kill_me)
				return;
			
			entity = next;
		}
		
	/*	
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(bogie->xdis, bogie->ydis, BOGIE_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				get_teleporter_spawn_point(teleporter, &bogie->xdis, &bogie->ydis);
				restart = 1;
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		*/
		
		if(!restart)
			break;
	}
}


void s_tick_bullet(struct entity_t *bullet)
{
	apply_gravity_acceleration(bullet);
	
	
	while(1)
	{
		double xdis = bullet->xdis + bullet->xvel;
		double ydis = bullet->ydis + bullet->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = line_walk_bsp_tree(bullet->xdis, bullet->ydis, xdis, ydis);
		if(node)
		{
;//			remove_entity(bullet);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == bullet)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_bullet_collision(entity, bullet);
				}
				break;
				
			case ENT_WEAPON:
				if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					weapon_bullet_collision(entity, bullet);
				}
				break;
				
			case ENT_ROCKET:
				if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					bullet_rocket_collision(bullet, entity);
				}
				break;
				
			case ENT_MINE:
				if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, entity->xdis, entity->ydis, MINE_RADIUS))
				{
					bullet_mine_collision(bullet, entity);
				}
				break;
				
	/*		case ENT_RAILS:
				if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					bullet_rails_collision(bullet, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					entity->kill_me = 1;
				}
				break;
	*/		}
			
			struct entity_t *next = entity->next;
				
			if(entity->kill_me)
				remove_entity(&sentity0, entity);
			else
				entity->in_tick = 0;
			
			if(bullet->kill_me)
				return;
			
			entity = next;
		}
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(line_in_circle(bullet->xdis, bullet->ydis, xdis, ydis, teleporter->x, teleporter->y, teleporter->radius))
			{
				get_teleporter_spawn_point(teleporter, &bullet->xdis, &bullet->ydis);
				restart = 1;
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		if(restart)
			continue;
		
		
		bullet->xdis = xdis;
		bullet->ydis = ydis;
		
		break;
	}
}


void s_tick_rocket(struct entity_t *rocket)
{
	// see if we have left the launcher
	
	if(rocket->rocket_data.in_weapon)
	{
		struct entity_t *weapon = get_entity(sentity0, rocket->rocket_data.weapon_id);
			
		if(!weapon)
			rocket->rocket_data.in_weapon = 0;
		else
			if(!circles_intersect(rocket->xdis, rocket->ydis, ROCKET_RADIUS, weapon->xdis, weapon->ydis, WEAPON_RADIUS))
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
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, ROCKET_RADIUS, NULL);
		if(node)
		{
			explode_rocket(rocket);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == rocket)
			{
				entity = entity->next;
				continue;
			}
		
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					explode_rocket(rocket);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					if(!rocket->rocket_data.in_weapon || rocket->rocket_data.weapon_id != entity->index)
						explode_rocket(rocket);
				}
				break;
				
			case ENT_BOGIE:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				{
					bogie_rocket_collision(entity, rocket);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					rocket_rocket_collision(rocket, entity);
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
				{
					rocket_mine_collision(rocket, entity);
				}
				break;
				
	/*		case ENT_RAILS:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					rocket_rails_collision(rocket, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, ROCKET_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					entity->kill_me = 1;
				}
				break;
	*/		}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(&sentity0, entity);
			else
				entity->in_tick = 0;
			
			if(rocket->kill_me)
				return;
			
			entity = next;
		}
		
		
/*		// check for collision with speedup ramp
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta + M_PI / 2.0, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, ROCKET_RADIUS))
			{
				sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
				
				rocket->xvel += speedup_ramp->boost * sin_theta;
				rocket->yvel += speedup_ramp->boost * cos_theta;
				
				restart = 1;
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, ROCKET_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				get_teleporter_spawn_point(teleporter, &rocket->xdis, &rocket->ydis);
				
				check_rocket_teleportation(rocket);
				
				if(rocket->kill_me)
				{
;//					remove_entity(rocket);
					return;
				}
				
				restart = 1;
				break;
			}
			
			teleporter = teleporter->next;
		}
		
		*/
		
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
	apply_gravity_acceleration(mine);
	
	slow_entity(mine);

	
	while(1)
	{
		double xdis = mine->xdis + mine->xvel;
		double ydis = mine->ydis + mine->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, MINE_RADIUS, NULL);
		if(node)
		{
			explode_mine(mine);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == mine)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					explode_mine(mine);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					explode_mine(mine);
				}
				break;
				
			case ENT_BOGIE:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				{
					bogie_mine_collision(entity, mine);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					rocket_mine_collision(entity, mine);
				}
				break;
				
			case ENT_MINE:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, MINE_RADIUS))
				{
					mine_mine_collision(mine, entity);
				}
				break;
				
		/*	case ENT_RAILS:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					mine_rails_collision(mine, entity);
				}
				break;
				
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, MINE_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					entity->kill_me = 1;
				}
				break;
		*/	}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
				remove_entity(&sentity0, entity);
			else
				entity->in_tick = 0;

			if(mine->kill_me)
				return;
			
			entity = next;
		}
		
		
/*		// check for collision with speedup ramp
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta + M_PI / 2.0, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, MINE_RADIUS))
			{
				sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
				
				mine->xvel += speedup_ramp->boost * sin_theta;
				mine->yvel += speedup_ramp->boost * cos_theta;
				
				restart = 1;
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, MINE_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				get_teleporter_spawn_point(teleporter, &mine->xdis, &mine->ydis);
				
				check_mine_teleportation(mine);
				
				if(mine->kill_me)
				{
;//					remove_entity(mine);
					return;
				}
				
				restart = 1;
				break;
			}
			
			teleporter = teleporter->next;
		}
*/		
		if(restart)
			continue;
		
		
		mine->xdis = xdis;
		mine->ydis = ydis;
		
		break;
	}
}


void s_tick_rails(struct entity_t *rails)
{
	apply_gravity_acceleration(rails);
	
	slow_entity(rails);

	
	while(1)
	{
		double xdis = rails->xdis + rails->xvel;
		double ydis = rails->ydis + rails->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, RAILS_RADIUS, NULL);
		if(node)
		{
			explode_rails(rails);
;//			remove_entity(rails);
			return;
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == rails)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					explode_rails(rails);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					explode_rails(rails);
				}
				break;
				
			case ENT_BOGIE:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				{
					bogie_rails_collision(entity, rails);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					rocket_rails_collision(entity, rails);
				}
				break;
			
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					rails_rails_collision(rails, entity);
				}
				break;
			
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, RAILS_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					entity->kill_me = 1;
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
;//				remove_entity(entity);
			else
				entity->in_tick = 0;
			
			if(rails->kill_me)
			{
;//				remove_entity(rails);
				return;
			}
			
			entity = next;
		}
		
		
		// check for collision with speedup ramp
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta + M_PI / 2.0, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, RAILS_RADIUS))
			{
				sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
				
				rails->xvel += speedup_ramp->boost * sin_theta;
				rails->yvel += speedup_ramp->boost * cos_theta;
				
				restart = 1;
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, RAILS_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
				get_teleporter_spawn_point(teleporter, &rails->xdis, &rails->ydis);
				
				check_rails_teleportation(rails);
				
				if(rails->kill_me)
				{
;//					remove_entity(rails);
					return;
				}
				
				restart = 1;
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
	apply_gravity_acceleration(shield);
	
	slow_entity(shield);

	
	while(1)
	{
		double xdis = shield->xdis + shield->xvel;
		double ydis = shield->ydis + shield->yvel;
		
		int restart = 0;
		
		
		// check for collision against wall
		
		struct bspnode_t *node = circle_walk_bsp_tree(xdis, ydis, SHIELD_RADIUS, NULL);
		if(node)
		{
;//			remove_entity(shield);
		}
		
		
		// check for collision with other entities
		
		struct entity_t *entity = sentity0;
		while(entity)
		{
			if(entity == shield)
			{
				entity = entity->next;
				continue;
			}
			
			entity->in_tick = 1;
			
			switch(entity->type)
			{
			case ENT_CRAFT:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, entity->xdis, entity->ydis, CRAFT_RADIUS))
				{
					craft_shield_collision(entity, shield);
				}
				break;
				
			case ENT_WEAPON:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, entity->xdis, entity->ydis, WEAPON_RADIUS))
				{
					weapon_shield_collision(entity, shield);
				}
				break;
				
			case ENT_BOGIE:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, entity->xdis, entity->ydis, BOGIE_RADIUS))
				{
					bogie_shield_collision(entity, shield);
				}
				break;
				
			case ENT_ROCKET:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, entity->xdis, entity->ydis, ROCKET_RADIUS))
				{
					shield->kill_me = 1;
				}
				break;
			
			case ENT_RAILS:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, entity->xdis, entity->ydis, RAILS_RADIUS))
				{
					shield->kill_me = 1;
				}
				break;
			
			case ENT_SHIELD:
				if(circles_intersect(xdis, ydis, SHIELD_RADIUS, entity->xdis, entity->ydis, SHIELD_RADIUS))
				{
					shield_shield_collision(shield, shield);
				}
				break;
			}
			
			struct entity_t *next = entity->next;
			
			if(entity->kill_me)
;//				remove_entity(entity);
			else
				entity->in_tick = 0;
			
			if(shield->kill_me)
			{
;//				remove_entity(shield);
				return;
			}
			
			entity = next;
		}
		
		
		// check for collision with speedup ramp
		
		struct speedup_ramp_t *speedup_ramp = speedup_ramp0;
		while(speedup_ramp)
		{
			double sin_theta, cos_theta;
			sincos(speedup_ramp->theta + M_PI / 2.0, &sin_theta, &cos_theta);
			
			if(line_in_circle(speedup_ramp->x + cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y + sin_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->x - cos_theta * speedup_ramp->width / 2.0, 
				speedup_ramp->y - sin_theta * speedup_ramp->width / 2.0,
				xdis, ydis, SHIELD_RADIUS))
			{
				sincos(speedup_ramp->theta, &sin_theta, &cos_theta);
				
				shield->xvel += speedup_ramp->boost * sin_theta;
				shield->yvel += speedup_ramp->boost * cos_theta;
				
				restart = 1;
			}			
			
			speedup_ramp = speedup_ramp->next;
		}
		
		
		// check for collision with teleporter
		
		struct teleporter_t *teleporter = teleporter0;
		while(teleporter)
		{
			if(circle_in_circle(xdis, ydis, SHIELD_RADIUS, teleporter->x, teleporter->y, teleporter->radius))
			{
;//				remove_entity(shield);
				return;
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
	sentity0 = *entity0;
	
	struct entity_t *centity = *entity0;
	
	while(centity)
	{
		centity->in_tick = 1;
		
		switch(centity->type)
		{
		case ENT_CRAFT:
			s_tick_craft(centity);
			break;
			
		case ENT_BOGIE:
			s_tick_bogie(centity);
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
		
/*		case ENT_RAILS:
			s_tick_rails(centity);
			break;
		
		case ENT_SHIELD:
			s_tick_shield(centity);
			break;
			*/
		}

		
		if(centity->kill_me)
		{
			struct entity_t *next = centity->next;
			LL_REMOVE(struct entity_t, entity0, centity);
			sentity0 = *entity0;
			centity = next;
		}
		else
		{
			centity->in_tick = 0;
			centity = centity->next;
		}
	}
	
	centity = *entity0;
	
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
			LL_REMOVE(struct entity_t, entity0, centity);
			sentity0 = *entity0;
			centity = next;
		}
		else
		{
			centity->in_tick = 0;
			centity = centity->next;
		}
	}
}
