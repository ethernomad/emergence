#ifdef _INC_NFBUFFER
void game_process_stream_timed(uint32_t conn, uint32_t index, 
	uint64_t *stamp, struct buffer_t *stream);
void game_process_stream_untimed(uint32_t conn, uint32_t index, struct buffer_t *stream);
void game_process_stream_timed_ooo(uint32_t conn, uint32_t index, 
	uint64_t *stamp, struct buffer_t *stream);
void game_process_stream_untimed_ooo(uint32_t conn, uint32_t index, struct buffer_t *stream);
#endif

void game_resolution_change();
void create_colour_cvars();


void game_process_cookie_echoed();
void game_process_connecting();
void game_process_conn_lost(uint32_t conn);
void game_process_connection(uint32_t conn);
void game_process_connection_failed();
void game_process_disconnection(uint32_t conn);
void render_game();
void init_game();
void kill_game();


int message_reader_more();
uint8_t message_reader_read_uint8();
uint32_t message_reader_read_uint32();
int message_reader_read_int();
float message_reader_read_float();
struct string_t *message_reader_read_string();



void start_moving_view(float x1, float y1, float x2, float y2);



void world_to_screen(double worldx, double worldy, int *screenx, int *screeny);
void screen_to_world(int screenx, int screeny, double *worldx, double *worldy);

void rev_thrust(int state);
void fire_rail();
void fire_left();
void fire_right();
void drop_mine();

extern uint32_t cgame_tick;
extern float cgame_time;

extern int recording;
extern struct string_t *recording_filename;
#ifdef ZLIB_H
extern gzFile gzrecording;
#endif


#ifdef _INC_SGAME
void tick_craft(struct entity_t *craft, float xdis, float ydis);
void tick_rocket(struct entity_t *rocket, float xdis, float ydis);
void explosion(float xdis, float ydis, float xvel, float yvel, 
	float size, uint8_t magic, 
	uint8_t start_red, uint8_t start_green, uint8_t start_blue,
	uint8_t end_red, uint8_t end_green, uint8_t end_blue);
#endif	

void create_teleporter_sparkles();

extern double viewx, viewy;

void update_game();

extern uint32_t game_conn;


#define ROTATIONS 120


#define GAMESTATE_DEAD			0
#define GAMESTATE_DEMO			1
#define GAMESTATE_CONNECTING	2
#define GAMESTATE_SPECTATING	3
#define GAMESTATE_PLAYING		4

extern int game_state;
extern int game_rendering;
