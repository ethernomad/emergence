/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_SGAME
#define _INC_SGAME


// server -> client

#define EMMSGCLASS_MASK		(0x3 << 6)

#define EMMSGCLASS_STND			(0x0 << 6)
#define EMMSGCLASS_NETONLY		(0x1 << 6)		// not written to demo files
#define EMMSGCLASS_EVENT		(0x2 << 6)

#define EMMSG_PROTO_VER			(EMMSGCLASS_STND | 0x00)


/*

changes above this line and in the network code must not break backward
compatibility of the protocol version detection mechanism

----------------------------------------------------------------------------------------------------

if changes below this line or in the game code (either server or client side)
break backward compatibility, EM_PROTO_VER must be incremented accordingly

*/


#define EM_PROTO_VER									0x05



// server -> client

#define EMMSG_LOADMAP			(EMMSGCLASS_STND | 0x01)
#define EMMSG_LOADSKIN			(EMMSGCLASS_STND | 0x02)
#define EMMSG_PLAYING			(EMMSGCLASS_STND | 0x03)
#define EMMSG_SPECTATING		(EMMSGCLASS_STND | 0x04)

#define EMNETMSG_PRINT				(EMMSGCLASS_NETONLY | 0x00)
#define EMNETMSG_AUTHENTICATE		(EMMSGCLASS_NETONLY | 0x01)
#define EMNETMSG_JOINED				(EMMSGCLASS_NETONLY | 0x02)
#define EMNETMSG_FAILED				(EMMSGCLASS_NETONLY | 0x03)
#define EMNETMSG_INRCON				(EMMSGCLASS_NETONLY | 0x05)
#define EMNETMSG_NOTINRCON			(EMMSGCLASS_NETONLY | 0x06)
#define EMNETMSG_MATCH_BEGUN		(EMMSGCLASS_NETONLY | 0x07)
#define EMNETMSG_PLAYER_INFO		(EMMSGCLASS_NETONLY | 0x08)
#define EMNETMSG_REMOVE_PLAYER_INFO	(EMMSGCLASS_NETONLY | 0x09)
#define EMNETMSG_MATCH_OVER			(EMMSGCLASS_NETONLY | 0x0a)
#define EMNETMSG_LOBBY				(EMMSGCLASS_NETONLY | 0x0b)

#define EMEVENT_PULSE				(EMMSGCLASS_EVENT | 0x00)
#define EMEVENT_PRINT				(EMMSGCLASS_EVENT | 0x01)
#define EMEVENT_SPAWN_ENT			(EMMSGCLASS_EVENT | 0x02)
#define EMEVENT_UPDATE_ENT			(EMMSGCLASS_EVENT | 0x03)
#define EMEVENT_KILL_ENT			(EMMSGCLASS_EVENT | 0x04)
#define EMEVENT_FOLLOW_ME			(EMMSGCLASS_EVENT | 0x05)
#define EMEVENT_CARCASS				(EMMSGCLASS_EVENT | 0x06)
#define EMEVENT_RAILTRAIL			(EMMSGCLASS_EVENT | 0x07)
#define EMEVENT_DETACH				(EMMSGCLASS_EVENT | 0x08)
#define EMEVENT_TELEPORT			(EMMSGCLASS_EVENT | 0x09)
#define EMEVENT_SPEEDUP				(EMMSGCLASS_EVENT | 0x0a)
#define EMEVENT_EXPLOSION			(EMMSGCLASS_EVENT | 0x0c)
#define EMEVENT_COLOURS				(EMMSGCLASS_EVENT | 0x0d)
#define EMEVENT_WEAPON_START_FIRING	(EMMSGCLASS_EVENT | 0x0e)
#define EMEVENT_WEAPON_STOP_FIRING	(EMMSGCLASS_EVENT | 0x0f)
#define EMEVENT_SHIELD_STRENGTHS	(EMMSGCLASS_EVENT | 0x10)
#define EMEVENT_AMMO_LEVELS			(EMMSGCLASS_EVENT | 0x11)



// client -> server

#define EMMSG_JOIN				0x00
#define EMMSG_SESSION_KEY		0x01
#define EMMSG_PLAY				0x03
#define EMMSG_SPECTATE			0x04
#define EMMSG_START				0x05
#define EMMSG_SAY				0x19
#define EMMSG_READY				0x1a
#define EMMSG_SERVERS			0x1b

#define EMMSG_THRUST			0x06
#define EMMSG_BRAKE				0x07
#define EMMSG_NOBRAKE			0x08
#define EMMSG_ROLL				0x09
#define EMMSG_ROLL_LEFT			0x0a
#define EMMSG_NOROLL_LEFT		0x0b
#define EMMSG_ROLL_RIGHT		0x0c
#define EMMSG_NOROLL_RIGHT		0x0d
#define EMMSG_FIRERAIL			0x0e
#define EMMSG_FIRELEFT			0x0f
#define EMMSG_FIRERIGHT			0x10
#define EMMSG_DROPMINE			0x11
#define EMMSG_ENTERRCON			0x12
#define EMMSG_LEAVERCON			0x13
#define EMMSG_RCONMSG			0x14
#define EMMSG_STATUS			0x15
#define EMMSG_NAMECNGE			0x16
#define EMMSG_SUICIDE			0x17
#define EMMSG_COLOURS			0x18

#define WINNER_YOU		0x00
#define WINNER_INDEX	0x01



struct craft_data_t
{
	float acc;
	float theta;
	int braking;
	int rolling_left;
	int rolling_right;

	struct entity_t *left_weapon, *right_weapon;
	float shield_flare;
	int carcass;
	
	uint8_t magic_smoke;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
	uint8_t shield_red, shield_green, shield_blue;
	uint8_t plasma_red, plasma_green, plasma_blue;
	
	#ifdef EMSERVER
	float shield_strength;
	struct player_t *owner;
	#endif
	
	#ifdef EMCLIENT
	float old_theta;
	int skin;
	struct surface_t *surface;
	float particle;
	uint32_t last_tick;
	#endif
};


struct weapon_data_t
{
	int type;
	float theta;
	
	struct entity_t *craft;
	float shield_flare;
	uint8_t shield_red, shield_green, shield_blue;
	int detached;
	
	uint8_t firing;
	uint32_t firing_start;
	uint32_t fired;
	
	int original_ownership_defined;
	uint8_t magic_smoke;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
	uint8_t plasma_red, plasma_green, plasma_blue;

	#ifdef EMSERVER
	int ammo;
	float shield_strength;
	struct pickup_spawn_point_t *spawn_point;
	int respawned;
	#endif
	
	#ifdef EMCLIENT
	int skin;
	struct surface_t *surface;
	#endif
};


struct plasma_data_t
{
	int in_weapon;
	uint32_t weapon_id;
	uint8_t red, green, blue;

	#ifdef EMSERVER
	struct player_t *owner;
	#endif
	
	#ifdef EMCLIENT
	int skin;
	struct surface_t *surface;
	#endif
};


struct rocket_data_t
{
	uint32_t start_tick;
	float theta;
	int in_weapon;
	uint32_t weapon_id;

	uint8_t magic_smoke;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;

	#ifdef EMSERVER
	struct player_t *owner;
	#endif
	
	#ifdef EMCLIENT
	int skin;
	struct surface_t *surface;
	float particle;
	uint32_t last_tick;
	uint32_t sample;
	#endif
};


struct bullet_data_t
{
	int in_weapon;
	uint32_t weapon_id;

	#ifdef EMSERVER
	struct player_t *owner;
	#endif
	
	#ifdef EMCLIENT
	float old_xdis, old_ydis;
	#endif
};


struct mine_data_t
{
	int under_craft;
	uint32_t craft_id;

	#ifdef EMSERVER
	struct player_t *owner;
	uint8_t magic_smoke;
	uint8_t smoke_start_red, smoke_start_green, smoke_start_blue;
	uint8_t smoke_end_red, smoke_end_green, smoke_end_blue;
	#endif
	
	#ifdef EMCLIENT
	int skin;
	struct surface_t *surface;
	#endif
};


struct rails_data_t
{
	float theta;

	#ifdef EMSERVER
	int quantity;
	struct pickup_spawn_point_t *spawn_point;
	#endif
};


struct shield_data_t
{
	float theta;

	#ifdef EMSERVER
	float strength;
	struct pickup_spawn_point_t *spawn_point;
	#endif
	
	#ifdef EMCLIENT
	int skin;
	struct surface_t *surface;
	#endif
};


struct entity_t
{
	uint32_t index;
	uint8_t type;
	uint8_t in_tick;
	uint8_t kill_me;
	
	float xdis, ydis;
	float xvel, yvel;
	
	int speeding_up;
	int teleporting;
	uint32_t teleporting_tick;
	uint32_t teleport_spawn_index;
	
	union
	{
		struct craft_data_t craft_data;
		struct weapon_data_t weapon_data;
		struct plasma_data_t plasma_data;
		struct bullet_data_t bullet_data;
		struct rocket_data_t rocket_data;
		struct mine_data_t mine_data;
		struct rails_data_t rails_data;
		struct shield_data_t shield_data;
	};
	
	#ifdef EMSERVER
	uint8_t propagate_me;
	#endif
	
	struct entity_t *next;
};

#define NO_ENT_INDEX	-1

#define ENT_CRAFT		0
#define ENT_WEAPON		1
#define ENT_PLASMA		2
#define ENT_BULLET		3
#define ENT_ROCKET		4
#define ENT_MINE		5
#define	ENT_RAILS		6
#define ENT_SHIELD		7

#define WEAPON_PLASMA_CANNON	0
#define WEAPON_MINIGUN			1
#define WEAPON_ROCKET_LAUNCHER	2

#define TELEPORT_FADE_TIME		0.25
#define TELEPORT_TRAVEL_TIME	1.0

#define TELEPORTING_FINISHED		0
#define TELEPORTING_DISAPPEARING	1
#define TELEPORTING_TRAVELLING		2
#define TELEPORTING_APPEARING		3

#define PLASMA_DAMAGE	0.1
#define BULLET_DAMAGE	0.05
#define RAIL_DAMAGE		0.5

struct spawn_point_t
{
	float x, y;
	float angle;
	int teleport_only;
	int index;
	int respawn_index;
	
	struct spawn_point_t *next;
};

#if defined (EMSERVER) || defined (_INC_PARTICLES)

struct teleporter_t
{
	float x, y;
	float radius;
	uint16_t colour;
	int sparkles;
	int spawn_index;
	
	int rotation_type;
	float rotation_angle;
	int divider;
	float divider_angle;
	
	#ifdef EMCLIENT
	struct particle_t particles[1000];
	float particle_power;
	int next_particle;
	#endif
	
	struct teleporter_t *next;
};

#endif	// _INC_PARTICLES


struct speedup_ramp_t
{
	float x, y;
	float theta;
	float width;
	float boost;
	uint32_t last_boost_tick;
	
	struct speedup_ramp_t *next;
};


struct gravity_well_t
{
	float x, y;
	float strength;
	int confined;
	
	struct gravity_well_t *next;
};


extern struct spawn_point_t *spawn_point0;
extern struct teleporter_t *teleporter0;
extern struct speedup_ramp_t *speedup_ramp0;
extern struct gravity_well_t *gravity_well0;

void clear_sgame();

struct entity_t *new_entity(struct entity_t **entity0);
struct entity_t *get_entity(struct entity_t *entity0, uint32_t index);
void remove_entity(struct entity_t **entity0, struct entity_t *entity);

#ifdef ZLIB_H_
int read_spawn_point(gzFile file);
int read_gravity_well(gzFile file);
int read_teleporter(gzFile file);
int read_speedup_ramp(gzFile file);
int read_shield_energy(gzFile file);
#endif

#ifdef EMSERVER
void splash_force(float x, float y, float force, struct player_t *responsibility);
void explode_craft(struct entity_t *craft, struct player_t *responsibility);
int craft_force(struct entity_t *craft, double force, struct player_t *responsibility);
int weapon_force(struct entity_t *weapon, double force, struct player_t *responsibility);
int plasma_rail_hit(struct entity_t *plasma);
int rocket_force(struct entity_t *rocket, double force);
int mine_force(struct entity_t *mine, double force);
int rails_force(struct entity_t *rails, double force, struct player_t *responsibility);
int shield_force(struct entity_t *shield, double force);
#endif

void s_tick_entities(struct entity_t **entity0);
void strip_weapons_from_craft(struct entity_t *craft);
void strip_craft_from_weapon(struct entity_t *weapon);
void get_spawn_point_coords(uint32_t index, float *x, float *y);
int line_in_circle(double lx1, double ly1, double lx2, double ly2, 
	double cx, double cy, double cr);
int line_in_circle_with_coords(double lx1, double ly1, double lx2, double ly2, 
	double cx, double cy, double cr, float *out_x, float *out_y);
int circles_intersect(double x1, double y1, double r1, double x2, double y2, double r2);
int point_in_circle(double px, double py, double cx, double cy, double cr);




#define CRAFT_RADIUS	56.569
#define WEAPON_RADIUS	35.355
#define PLASMA_RADIUS	5.0
#define ROCKET_RADIUS	25.0
#define MINE_RADIUS		60.0
#define RAILS_RADIUS	20.0
#define SHIELD_RADIUS	20.0

#define CRAFT_MASS	100.0
#define WEAPON_MASS	75.0
#define RAILS_MASS	15.0

#define EMERGENCE_COMPILEDFORMATID	0


#endif // _INC_SGAME
