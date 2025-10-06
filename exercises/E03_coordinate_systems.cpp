#define TEXTURE_PIXELS_PER_UNIT 128
#define CAMERA_PIXELS_PER_UNIT  128

#include <itu_unity_include.hpp>

#define ENABLE_DIAGNOSTICS

#define TARGET_FRAMERATE SECONDS(1) / 60
#define WINDOW_W         800
#define WINDOW_H         600


#define ENTITY_COUNT 4096

#define TILEMAP_W 12
#define TILEMAP_H 11
#define PIXELS_PER_UNIT 16


bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = true;

struct Entity
{
	Sprite sprite;
	Transform transform;
};

struct Tilemap
{
	int w, h;
	uint16_t* tiles; // tile indices
};

struct GameState
{
	// shortcut references
	Entity* player;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;
	Tilemap tilemap;
	vec2f mouse_world;
	int mouse_tile_x;
	int mouse_tile_y;
	int player_tile_x;
	int player_tile_y;

	// SDL-allocated structures
	SDL_Texture* atlas;
	SDL_Texture* bg;
};

struct Int16Vec2 {
    int x;  // These occupy separate
    int y;  // memory locations
};

static uint16_t tilemap_tiles_for_debug[256] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
	17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
	33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
	49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
	65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
	81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
	97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
	113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
	129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,
	145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,
	161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,
	177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,
	193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,
	209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,
	225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,
	241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256
};

static Entity* entity_create(GameState* state)
{
	if(!(state->entities_alive_count < ENTITY_COUNT))
		// NOTE: this might as well be an assert, if we don't have a way to recover/handle it
		return NULL;

	// // concise version
	//return &state->entities[state->entities_alive_count++];

	Entity* ret = &state->entities[state->entities_alive_count];
	++state->entities_alive_count;
	return ret;
}

// NOTE: this only works if nobody holds references to other entities!
//       if that were the case, we couldn't swap them around.
//       We will see in later lectures how to handle this kind of problems
static void entity_destroy(GameState* state, Entity* entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity < state->entities ||entity > state->entities + ENTITY_COUNT);

	--state->entities_alive_count;
	*entity = state->entities[state->entities_alive_count];
}

static void tilemap_alloc(GameState* state, int w, int h)
{
    state->tilemap.w = w;
    state->tilemap.h = h;
    state->tilemap.tiles = (uint16_t *)SDL_calloc(w * h, sizeof(uint16_t ));
    SDL_assert(state->tilemap.tiles);
}

static void tilemap_build_pattern(GameState* state)
{
	// simple pattern for testing
	// state->tilemap.tiles = tilemap_tiles_for_debug;
	// return;

    for(int y=0; y < state->tilemap.h; ++y)
        for(int x=0; x < state->tilemap.w; ++x) {
            int idx = y * state->tilemap.w + x;
            state->tilemap.tiles[idx] = (x + y);
		}
}

static int16_t coords_to_tilemap_index(GameState* state, int x, int y)
{
	return y * state->tilemap.w + x;
}

static Int16Vec2 tilemap_index_to_coords(int width, int idx)
{
	Int16Vec2 ret;
	ret.x = idx % width;
	ret.y = idx / width;
	return ret;
}

static void tilemap_render(SDLContext* context, GameState* state)
{
    for(int y = 0; y < state->tilemap.h; ++y)
    {
        for(int x = 0; x < state->tilemap.w; ++x)
        {
            int idx = y * state->tilemap.w + x;
            int tile_index = state->tilemap.tiles[idx];

            Sprite s;
			Int16Vec2 coords = tilemap_index_to_coords(state->tilemap.w, tile_index);
			SDL_FRect tile_atlas_rect = itu_lib_sprite_get_rect(coords.x, coords.y, PIXELS_PER_UNIT, PIXELS_PER_UNIT);
            itu_lib_sprite_init(&s, state->atlas, tile_atlas_rect);
            s.pivot = VEC2F_ZERO;

            Transform t{};
            t.position.x = (float)x;
            t.position.y = (float)y;
            t.scale = VEC2F_ONE;
            t.rotation = 0;

            bool is_mouse  = (x == state->mouse_tile_x  && y == state->mouse_tile_y);
            bool is_player = (x == state->player_tile_x && y == state->player_tile_y);

#ifdef SPRITE_HAS_COLOR   // (Only if your Sprite defines a 'color' member)
            if(is_mouse){
                s.color.r = 255; s.color.g = 180; s.color.b = 180;
            }else if(is_player){
                s.color.r = 180; s.color.g = 255; s.color.b = 180;
            }
            itu_lib_sprite_render(context, &s, &t);
#else
            itu_lib_sprite_render(context, &s, &t);
            if(is_mouse || is_player){
                SDL_FRect r = rect_global_to_screen(&context->camera,
                    SDL_FRect{ t.position.x, t.position.y, 1.0f, 1.0f });
                if(is_mouse) SDL_SetRenderDrawColor(context->renderer, 255, 120, 120, 90);
                else         SDL_SetRenderDrawColor(context->renderer, 120, 255, 120, 90);
                SDL_RenderFillRect(context->renderer, &r);
            }
#endif
            if(DEBUG_render_outlines)
                itu_lib_sprite_render_debug(context, &s, &t);
        }
    }
}

static void game_init(SDLContext* context, GameState* state)
{
	// allocate memory
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	// TODO allocate space for tile info (when we'll load those from file)
	// added
	tilemap_alloc(state, TILEMAP_W, TILEMAP_H);

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/tiny_dungeon_packed.png", SDL_SCALEMODE_NEAREST);
	state->bg    = texture_create(context, "data/kenney/prototype_texture_dark/texture_13.png", SDL_SCALEMODE_LINEAR);
}

static void game_reset(SDLContext* context, GameState* state)
{
	state->entities_alive_count = 0;
	
	// entities
	{
		Entity* bg = entity_create(state);
		SDL_FRect sprite_rect = SDL_FRect{ 0, 0, 1024, 1024};
		itu_lib_sprite_init(
			&bg->sprite,
			state->bg,
			itu_lib_sprite_get_rect(0, 0, 1024, 1024)
		);
		bg->transform.scale = VEC2F_ONE;
	}
	
	// reset tilemap
	tilemap_build_pattern(state);

	{
		state->player = entity_create(state);
		state->player->transform.position = VEC2F_ZERO;
		state->player->transform.scale = VEC2F_ONE;
		itu_lib_sprite_init(
			&state->player->sprite,
			state->atlas,
			itu_lib_sprite_get_rect(1, 8, PIXELS_PER_UNIT, PIXELS_PER_UNIT)
		);

		// raise sprite a bit, so that the position concides with the center of the image
		state->player->sprite.pivot.y = 0.3f;
	}
}

static void game_update(SDLContext* context, GameState* state)
{
	{
		const float player_speed = PIXELS_PER_UNIT / 2;

		Entity* entity = state->player;
		vec2f mov = { 0 };
		if(context->btn_isdown_up)
			mov.y += player_speed;
		if(context->btn_isdown_down)
			mov.y -= player_speed;
		if(context->btn_isdown_left)
			mov.x -= player_speed;
		if(context->btn_isdown_right)
			mov.x += player_speed;
	
		entity->transform.position = entity->transform.position + mov * (context->delta);

		// camera follows player
		context->camera_active->world_position = entity->transform.position;
	}
	// mouse position
	{
        vec2f mouse_screen = context->mouse_pos;
        vec2f mouse_world  = point_screen_to_global(&context->camera, mouse_screen);
        state->mouse_world = mouse_world;

        state->mouse_tile_x = (int)SDL_floorf(mouse_world.x);
        state->mouse_tile_y = (int)SDL_floorf(mouse_world.y);

        state->player_tile_x = (int)SDL_floorf(state->player->transform.position.x);
        state->player_tile_y = (int)SDL_floorf(state->player->transform.position.y);

        // clamp (avoid going outside tilemap)
        if(state->mouse_tile_x < 0 || state->mouse_tile_x >= state->tilemap.w ||
           state->mouse_tile_y < 0 || state->mouse_tile_y >= state->tilemap.h)
        {
            // mark as invalid
            state->mouse_tile_x = state->mouse_tile_y = -1;
        }

        SDL_RenderDebugTextFormat(context->renderer, 10, 100, "mouse screen: %7.2f %7.2f", mouse_screen.x, mouse_screen.y);
        SDL_RenderDebugTextFormat(context->renderer, 10, 110, "mouse world : %7.2f %7.2f", mouse_world.x, mouse_world.y);
        SDL_RenderDebugTextFormat(context->renderer, 10, 120, "mouse tile  : %3d %3d", state->mouse_tile_x, state->mouse_tile_y);
        SDL_RenderDebugTextFormat(context->renderer, 10, 130, "player tile : %3d %3d", state->player_tile_x, state->player_tile_y);
	}
}

static void game_render(SDLContext* context, GameState* state)
{
	tilemap_render(context, state);

	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		// render texture
		SDL_FRect rect_src = entity->sprite.rect;
		SDL_FRect rect_dst;

		if(DEBUG_render_textures)
			itu_lib_sprite_render(context, &entity->sprite, &entity->transform);

		if(DEBUG_render_outlines)
			itu_lib_sprite_render_debug(context, &entity->sprite, &entity->transform);
	}

	// debug window
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
	SDL_RenderRect(context->renderer, NULL);
}

int main(void)
{
	bool quit = false;
	SDL_Window* window;
	SDLContext context = { 0 };
	GameState  state   = { 0 };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	SDL_CreateWindowAndRenderer("E03 - Coordinate Systems", WINDOW_W, WINDOW_H, 0, &window, &context.renderer);
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);

	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		context.zoom = 1;
		context.window_w /= context.zoom;
		context.window_h /= context.zoom;
		SDL_SetRenderScale(context.renderer, context.zoom, context.zoom);
	}

	context.camera_default.normalized_screen_size.x = 1.0f;
	context.camera_default.normalized_screen_size.y = 1.0f;
	context.camera_default.normalized_screen_offset.x = 0.0f;
	context.camera_default.normalized_screen_offset.y = 0.0f;
	context.camera_default.zoom = 1;
	context.camera_default.pixels_per_unit = CAMERA_PIXELS_PER_UNIT;

	camera_set_active(&context, &context.camera_default);

	game_init(&context, &state);
	game_reset(&context, &state);

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_frame_end;
	SDL_Time walltime_work_end;
	SDL_Time elapsed_work;
	SDL_Time elapsed_frame;

	SDL_GetCurrentTime(&walltime_frame_beg);
	walltime_frame_end = walltime_frame_beg;

	while(!quit)
	{
		// input
		SDL_Event event;
		sdl_input_clear(&context);
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_EVENT_QUIT:
					quit = true;
					break;

				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_KEY_UP:
					switch(event.key.key)
					{
						case SDLK_W: sdl_input_key_process(&context, BTN_TYPE_UP, &event);        break;
						case SDLK_A: sdl_input_key_process(&context, BTN_TYPE_LEFT, &event);      break;
						case SDLK_S: sdl_input_key_process(&context, BTN_TYPE_DOWN, &event);      break;
						case SDLK_D: sdl_input_key_process(&context, BTN_TYPE_RIGHT, &event);     break;
						case SDLK_Q: sdl_input_key_process(&context, BTN_TYPE_ACTION_0, &event);  break;
						case SDLK_E: sdl_input_key_process(&context, BTN_TYPE_ACTION_1, &event);  break;
						case SDLK_SPACE: sdl_input_key_process(&context, BTN_TYPE_SPACE, &event); break;
					}

					// debug keys
					if(event.key.down && !event.key.repeat)
					{
						switch(event.key.key)
						{
							case SDLK_TAB: game_reset(&context, &state); break;
							case SDLK_F1: DEBUG_render_textures = !DEBUG_render_textures; break;
							case SDLK_F2: DEBUG_render_outlines = !DEBUG_render_outlines; break;
						}
					}
					break;
			}
		}

		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		// update
		game_update(&context, &state);
		game_render(&context, &state);

		SDL_GetCurrentTime(&walltime_work_end);
		elapsed_work = walltime_work_end - walltime_frame_beg;

		if(elapsed_work < TARGET_FRAMERATE)
			SDL_DelayNS(TARGET_FRAMERATE - elapsed_work);
		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;
		
#ifdef ENABLE_DIAGNOSTICS
		{
			SDL_SetRenderDrawColor(context.renderer, 0x0, 0x00, 0x00, 0xCC);
			SDL_FRect rect = SDL_FRect{ 5, 5, 225, 55 };
			SDL_RenderFillRect(context.renderer, &rect);

			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10, 10, "work: %9.6f ms/f", (float)elapsed_work  / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 20, "tot : %9.6f ms/f", (float)elapsed_frame / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 30, "[TAB] reset ");
			SDL_RenderDebugTextFormat(context.renderer, 10, 40, "[F1]  render textures   %s", DEBUG_render_textures   ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 50, "[F2]  render outlines   %s", DEBUG_render_outlines   ? " ON" : "OFF");
		}
#endif
		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		walltime_frame_beg = walltime_frame_end;
	}
}
