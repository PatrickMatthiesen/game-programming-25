#define PIXELS_PER_UNIT 16		   // The size of one tile in the texture atlas (in pixels)
#define TEXTURE_PIXELS_PER_UNIT 16 // How many pixels in the texture correspond to one world unit
#define CAMERA_ZOOM_FACTOR 4	   // How many times to magnify the base resolution. 1 = no zoom.
#define ZOOM_SPEED 1.0f			   // How fast to zoom in and out

#include <itu_unity_include.hpp>

#define ENABLE_DIAGNOSTICS

#define PHYSICS_TIMESTEP_SECS (1.0f / 60.0f)
#define PHYSICS_TIMESTEP_NSECS (SECONDS(1) / 60)
#define PHYSICS_MAX_TIMESTEPS_PER_FRAME 4
#define TARGET_FRAMERATE SECONDS(1) / 60
#define WINDOW_W 800
#define WINDOW_H 600

#define ENTITY_COUNT 4096

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = true;
bool DEBUG_render_mouse_info = true;

// atlas (tileset) layout in the texture image (columns x rows)
#define ATLAS_COLS 12
#define ATLAS_ROWS 11

// tilemap (world) dimensions
#define TILEMAP_W 20
#define TILEMAP_H 20

// Map built with the provided tile indices (examples used: 3=cobblestone_floor, 6=magic_pool_green, 35=platform_circular,
// 96=pillar_top, 100=carpet_small, 98=torch_lit, 13=wood_plank_border, 19=wooden_crate_top, 101=table_top, 102=chair, 31=chest_closed)
#include <stdint.h>
#include <stdlib.h>

// 20x20 outdoor ruins/courtyard style map
static uint16_t tilemap_template[20 * 20] = {
	// Row 0
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	// Row 1
	14, 48, 48, 48, 48, 14, 48, 48, 48, 48, 48, 48, 48, 48, 14, 48, 48, 48, 48, 14,
	// Row 2
	14, 19, 26, 48, 48, 14, 19, 34, 48, 48, 48, 48, 75, 19, 14, 19, 27, 19, 28, 14,
	// Row 3
	14, 48, 48, 48, 48, 22, 48, 48, 19, 14, 14, 14, 14, 14, 14, 48, 48, 48, 48, 14,
	// Row 4
	14, 19, 39, 36, 37, 23, 38, 40, 19, 14, 80, 80, 82, 80, 14, 19, 41, 19, 42, 14,
	// Row 5
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 80, 84, 82, 85, 14, 14, 14, 22, 14, 14,
	// Row 6
	14, 48, 48, 48, 48, 48, 48, 48, 48, 14, 80, 80, 88, 80, 14, 48, 48, 23, 19, 14,
	// Row 7
	14, 19, 63, 19, 64, 19, 65, 48, 48, 14, 14, 14, 14, 14, 14, 19, 29, 19, 29, 14,
	// Row 8
	14, 48, 48, 48, 48, 48, 48, 48, 48, 22, 48, 48, 48, 48, 23, 48, 48, 48, 48, 14,
	// Row 9
	14, 14, 14, 14, 48, 48, 19, 14, 14, 23, 19, 98, 48, 48, 22, 14, 14, 14, 14, 14,
	// Row 10
	14, 80, 80, 14, 19, 66, 19, 14, 80, 80, 48, 48, 48, 48, 80, 80, 14, 82, 84, 14,
	// Row 11
	14, 80, 82, 14, 19, 67, 19, 14, 80, 82, 48, 48, 48, 48, 80, 82, 14, 80, 80, 14,
	// Row 12
	14, 14, 14, 14, 48, 48, 19, 14, 14, 14, 14, 22, 23, 14, 14, 14, 14, 14, 14, 14,
	// Row 13
	14, 48, 48, 48, 48, 35, 48, 48, 48, 48, 19, 23, 22, 48, 48, 48, 48, 48, 48, 14,
	// Row 14
	14, 19, 56, 19, 61, 19, 58, 48, 48, 48, 48, 48, 48, 48, 48, 19, 57, 19, 62, 14,
	// Row 15
	14, 14, 14, 14, 14, 14, 14, 14, 48, 48, 48, 48, 48, 48, 14, 14, 14, 14, 14, 14,
	// Row 16
	14, 80, 80, 80, 80, 80, 80, 14, 19, 43, 19, 44, 48, 48, 14, 80, 80, 80, 80, 14,
	// Row 17
	14, 80, 88, 80, 89, 80, 82, 14, 48, 48, 48, 48, 48, 48, 14, 80, 88, 82, 80, 14,
	// Row 18
	14, 80, 80, 80, 80, 80, 80, 14, 19, 71, 19, 73, 19, 74, 14, 80, 80, 80, 80, 14,
	// Row 19
	14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};

struct Entity
{
	Sprite sprite;
	Transform transform;
};

struct Tilemap
{
	int w, h;
	uint16_t *tiles; // tile indices
};

struct GameState
{
	// shortcut references
	Entity *player;

	// game-allocated memory
	Entity *entities;
	int entities_alive_count;
	Tilemap tilemap;
	vec2f mouse_world;
	int mouse_tile_x;
	int mouse_tile_y;
	int player_tile_x;
	int player_tile_y;

	// SDL-allocated structures
	SDL_Texture *atlas;
	SDL_Texture *bg;

	// Tile editing UI (activated with middle-click)
	bool tile_edit_active;
	int edit_tile_x;
	int edit_tile_y;
	char edit_buffer[16];
	int edit_buf_len;
	int edit_current_idx;
	int edit_previous_idx;
};

struct Int16Vec2
{
	int x; // These occupy separate
	int y; // memory locations
};

static Entity *entity_create(GameState *state)
{
	if (!(state->entities_alive_count < ENTITY_COUNT))
		// NOTE: this might as well be an assert, if we don't have a way to recover/handle it
		return NULL;

	// // concise version
	// return &state->entities[state->entities_alive_count++];

	Entity *ret = &state->entities[state->entities_alive_count];
	++state->entities_alive_count;
	return ret;
}

// NOTE: this only works if nobody holds references to other entities!
//       if that were the case, we couldn't swap them around.
//       We will see in later lectures how to handle this kind of problems
static void entity_destroy(GameState *state, Entity *entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity < state->entities || entity > state->entities + ENTITY_COUNT);

	--state->entities_alive_count;
	*entity = state->entities[state->entities_alive_count];
}

static int16_t coords_to_tilemap_index(GameState *state, int x, int y)
{
	return y * state->tilemap.w + x;
}

static void tilemap_build_pattern(GameState *state)
{
	// simple pattern for testing
	// copy the static template into the allocated tile buffer
	int totalTiles = TILEMAP_W * TILEMAP_H;
	SDL_assert(state->tilemap.tiles); // ensure allocation succeeded
	SDL_memcpy(state->tilemap.tiles, tilemap_template, totalTiles * sizeof(uint16_t));
	return;

	for (int y = 0; y < state->tilemap.h; ++y)
	{
		for (int x = 0; x < state->tilemap.w; ++x)
		{
			int idx = coords_to_tilemap_index(state, x, y);
			state->tilemap.tiles[idx] = idx;
		}
	}
}

static Int16Vec2 tilemap_index_to_coords(int width, int idx)
{
	Int16Vec2 ret;
	ret.x = idx % width;
	ret.y = idx / width;
	return ret;
}

vec2f world_to_tile(const Tilemap map, vec2f world)
{
	float tx = SDL_floorf(world.x);
	float ty = SDL_floorf(world.y);
	tx = SDL_clamp(tx, 0, map.w - 1);
	ty = SDL_clamp(ty, 0, map.h - 1);
	return vec2f{tx, ty};
}

static void tilemap_render(SDLContext *context, GameState *state)
{
	// const int offset = -0.5f; // center the pattern a bit

	for (int y = 0; y < state->tilemap.h; ++y)
	{
		for (int x = 0; x < state->tilemap.w; ++x)
		{
			int idx = y * state->tilemap.w + x;
			int tile_index = state->tilemap.tiles[idx];

			// atlas uses ATLAS_COLS per row (independent from tilemap width)
			Int16Vec2 coords = tilemap_index_to_coords(ATLAS_COLS, tile_index);
			SDL_FRect tile_atlas_rect = itu_lib_sprite_get_rect(coords.x, coords.y, TEXTURE_PIXELS_PER_UNIT, TEXTURE_PIXELS_PER_UNIT);

			Sprite s;
			itu_lib_sprite_init(&s, state->atlas, tile_atlas_rect);
			s.pivot = VEC2F_ZERO;

			Transform t{};
			t.position.x = (float)x; // offset;
			t.position.y = (float)y; // offset;
			t.scale = VEC2F_ONE;
			t.rotation = 0;

			bool is_mouse = (x == state->mouse_tile_x && y == state->mouse_tile_y);
			bool is_player = (x == state->player_tile_x && y == state->player_tile_y);

#ifdef SPRITE_HAS_COLOR // (Only if your Sprite defines a 'color' member)
			if (is_mouse)
			{
				s.color.r = 255;
				s.color.g = 180;
				s.color.b = 180;
			}
			else if (is_player)
			{
				s.color.r = 180;
				s.color.g = 255;
				s.color.b = 180;
			}
			itu_lib_sprite_render(context, &s, &t);
#else
			itu_lib_sprite_render(context, &s, &t);
			if (is_mouse || is_player)
			{
				SDL_FRect r = rect_global_to_screen(context,
													SDL_FRect{t.position.x, t.position.y, 1.0f, 1.0f});
				if (is_mouse)
					SDL_SetRenderDrawColor(context->renderer, 255, 120, 120, 90);
				else
					SDL_SetRenderDrawColor(context->renderer, 120, 255, 120, 90);
				SDL_RenderFillRect(context->renderer, &r);
			}
#endif
			if (DEBUG_render_outlines)
				itu_lib_sprite_render_debug(context, &s, &t);
		}
	}
}

static void tilemap_alloc(GameState *state, int w, int h)
{
	state->tilemap.w = w;
	state->tilemap.h = h;
	state->tilemap.tiles = (uint16_t *)SDL_calloc(w * h, sizeof(uint16_t));
	SDL_assert(state->tilemap.tiles);
}

static void game_init(SDLContext *context, GameState *state)
{
	// allocate memory
	state->entities = (Entity *)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	// TODO allocate space for tile info (when we'll load those from file)
	// added
	tilemap_alloc(state, TILEMAP_W, TILEMAP_H);

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/tiny_dungeon_packed.png", SDL_SCALEMODE_NEAREST);
	state->bg = texture_create(context, "data/kenney/prototype_texture_dark/texture_13.png", SDL_SCALEMODE_LINEAR);

	// init tile edit UI
	state->tile_edit_active = false;
	state->edit_tile_x = state->edit_tile_y = -1;
	state->edit_buf_len = 0;
	state->edit_buffer[0] = '\0';
	state->edit_current_idx = 0;
	state->edit_previous_idx = 0;
}

static void game_reset(SDLContext *context, GameState *state)
{
	state->entities_alive_count = 0;

	// reset tilemap
	{
		tilemap_build_pattern(state);
	}

	{
		state->player = entity_create(state);
		state->player->transform.position = VEC2F_ZERO;
		state->player->transform.scale = VEC2F_ONE;
		itu_lib_sprite_init(
			&state->player->sprite,
			state->atlas,
			itu_lib_sprite_get_rect(1, 8, TEXTURE_PIXELS_PER_UNIT, TEXTURE_PIXELS_PER_UNIT));

		// raise sprite a bit, so that the position concides with the center of the image
		state->player->sprite.pivot.y = 0.3f;
	}
}

static vec2f vec2f_normalize(vec2f v)
{
	float len = SDL_sqrtf(v.x * v.x + v.y * v.y);
	if (len != 0.0f)
		return v / len;
	return v;
}

static void game_update(SDLContext *context, GameState *state)
{
	{
		const float player_speed = 5.0f; // 5 tiles per second

		Entity *entity = state->player;
		vec2f mov = {0};
		if (context->btn_isdown_up)
			mov.y += 1.0f;
		if (context->btn_isdown_down)
			mov.y -= 1.0f;
		if (context->btn_isdown_left)
			mov.x -= 1.0f;
		if (context->btn_isdown_right)
			mov.x += 1.0f;

		if (mov.x != 0.0f || mov.y != 0.0f)
		{
			mov = vec2f_normalize(mov);
			entity->transform.position = entity->transform.position + mov * player_speed * context->delta;
		}

		// camera follows player
		context->camera_active->world_position = entity->transform.position;
		context->camera_active->zoom += context->mouse_scroll * ZOOM_SPEED * context->delta;
		// avoid zero/negative zoom (which mirrors) and limit extremes
		context->camera_active->zoom = SDL_clamp(context->camera_active->zoom, 0.05f, 16.0f);
	}
	// mouse position
	{
		vec2f mouse_screen = context->mouse_pos;
		vec2f mouse_world = point_screen_to_global(context, mouse_screen);
		state->mouse_world = mouse_world;

		state->mouse_tile_x = (int)SDL_floorf(mouse_world.x);
		state->mouse_tile_y = (int)SDL_floorf(mouse_world.y);

		state->player_tile_x = (int)SDL_floorf(state->player->transform.position.x);
		state->player_tile_y = (int)SDL_floorf(state->player->transform.position.y);

		// clamp (avoid going outside tilemap)
		if (state->mouse_tile_x < 0 || state->mouse_tile_x >= state->tilemap.w ||
			state->mouse_tile_y < 0 || state->mouse_tile_y >= state->tilemap.h)
		{
			// mark as invalid
			state->mouse_tile_x = state->mouse_tile_y = -1;
		}

		// // debug window
		// SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
		// SDL_RenderRect(context->renderer, NULL);
	}
}

static void game_render(SDLContext *context, GameState *state)
{
	tilemap_render(context, state);

	// tile edit overlay (if active)
	if (state->tile_edit_active)
	{
		vec2f box_pos = vec2f{20.0f, WINDOW_H - 90.0f};
		vec2f box_size = vec2f{300.0f, 68.0f};
		itu_lib_render_draw_rect_fill(context->renderer, box_pos, box_size, color{0.0f, 0.0f, 0.0f, 0.8f});

		SDL_SetRenderDrawColor(context->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		SDL_RenderDebugTextFormat(context->renderer, box_pos.x + 6, box_pos.y + 6, "Editing tile (%d, %d)", state->edit_tile_x, state->edit_tile_y);
		SDL_RenderDebugTextFormat(context->renderer, box_pos.x + 6, box_pos.y + 22, "Current idx: %d", state->edit_current_idx);
		SDL_RenderDebugTextFormat(context->renderer, box_pos.x + 6, box_pos.y + 38, "Type digits, Backspace, Enter to apply, Esc to cancel");
		SDL_RenderDebugTextFormat(context->renderer, box_pos.x + 6, box_pos.y + 52, "SHFT + Mouse wheel, or +/- to change value");
	}

	for (int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity *entity = &state->entities[i];
		// render texture
		SDL_FRect rect_src = entity->sprite.rect;
		SDL_FRect rect_dst;

		if (DEBUG_render_textures)
			itu_lib_sprite_render(context, &entity->sprite, &entity->transform);

		if (DEBUG_render_outlines)
			itu_lib_sprite_render_debug(context, &entity->sprite, &entity->transform);
	}
}

int main(void)
{
	bool quit = false;
	SDL_Window *window;
	SDLContext context = {0};
	GameState state = {0};

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
	context.camera_default.zoom = 1.0f;
	context.camera_default.pixels_per_unit = CAMERA_ZOOM_FACTOR * TEXTURE_PIXELS_PER_UNIT;

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

	while (!quit)
	{
		// input
		SDL_Event event;
		sdl_input_clear(&context);
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				quit = true;
				break;

			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
				// If we are editing a tile, intercept keyboard input for the editor
				if (state.tile_edit_active && event.type == SDL_EVENT_KEY_DOWN)
				{
					SDL_Keycode kc = event.key.key;
					int tx = state.edit_tile_x;
					int ty = state.edit_tile_y;

					// validate tile coords
					if (tx < 0 || tx >= state.tilemap.w || ty < 0 || ty >= state.tilemap.h)
						break;

					// Initialize previous/current only when we first enter edit mode (edit_previous_idx already set on middle-click)
					if (state.edit_buf_len == 0 && state.edit_buffer[0] == '\0')
					{
						int idx = state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)];
						state.edit_current_idx = idx;
						// leave edit_previous_idx as already set when editing began
					}

					// Digit keys (allow accumulation)
					if ((kc >= SDLK_0 && kc <= SDLK_9) || (kc >= SDLK_KP_0 && kc <= SDLK_KP_9))
					{
						int digit = (kc >= SDLK_KP_0 && kc <= SDLK_KP_9) ? (kc - SDLK_KP_0) : (kc - SDLK_0);
						if (state.edit_buf_len < (int)sizeof(state.edit_buffer) - 1)
						{
							state.edit_buffer[state.edit_buf_len++] = '0' + digit;
							state.edit_buffer[state.edit_buf_len] = '\0';
							state.edit_current_idx = atoi(state.edit_buffer);
							int clamped = SDL_clamp(state.edit_current_idx, 0, ATLAS_COLS * ATLAS_ROWS - 1);
							state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)] = (uint16_t)clamped; // live write
						}
						break;
					}

					// Backspace
					if (kc == SDLK_BACKSPACE)
					{
						if (state.edit_buf_len > 0)
						{
							state.edit_buf_len--;
							state.edit_buffer[state.edit_buf_len] = '\0';
							state.edit_current_idx = state.edit_buf_len > 0 ? atoi(state.edit_buffer) : 0;
							int clamped = SDL_clamp(state.edit_current_idx, 0, ATLAS_COLS * ATLAS_ROWS - 1);
							state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)] = (uint16_t)clamped;
						}
						break;
					}

					// Enter: finish editing (value already written)
					if (kc == SDLK_RETURN || kc == SDLK_KP_ENTER)
					{
						state.tile_edit_active = false;
						break;
					}

					// Escape: restore previous and exit
					if (kc == SDLK_ESCAPE)
					{
						state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)] = (uint16_t)state.edit_previous_idx;
						state.tile_edit_active = false;
						break;
					}

					// +/- to increment/decrement (keyboard)
					if (kc == SDLK_PLUS || kc == SDLK_EQUALS || kc == SDLK_KP_PLUS)
					{
						state.edit_current_idx++;
						int max_idx = ATLAS_COLS * ATLAS_ROWS - 1;
						if (state.edit_current_idx > max_idx)
							state.edit_current_idx = max_idx;
						state.edit_buf_len = 0;
						state.edit_buffer[0] = '\0';
						state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)] = (uint16_t)state.edit_current_idx;
						break;
					}
					if (kc == SDLK_MINUS || kc == SDLK_KP_MINUS)
					{
						state.edit_current_idx--;
						if (state.edit_current_idx < 0)
							state.edit_current_idx = 0;
						state.edit_buf_len = 0;
						state.edit_buffer[0] = '\0';
						state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)] = (uint16_t)state.edit_current_idx;
						break;
					}

					break;
				}

				// If not editing, route keys to normal input system
				switch (event.key.key)
				{
				case SDLK_W:
					sdl_input_key_process(&context, BTN_TYPE_UP, &event);
					break;
				case SDLK_A:
					sdl_input_key_process(&context, BTN_TYPE_LEFT, &event);
					break;
				case SDLK_S:
					sdl_input_key_process(&context, BTN_TYPE_DOWN, &event);
					break;
				case SDLK_D:
					sdl_input_key_process(&context, BTN_TYPE_RIGHT, &event);
					break;
				case SDLK_Q:
					sdl_input_key_process(&context, BTN_TYPE_ACTION_0, &event);
					break;
				case SDLK_E:
					sdl_input_key_process(&context, BTN_TYPE_ACTION_1, &event);
					break;
				case SDLK_SPACE:
					sdl_input_key_process(&context, BTN_TYPE_SPACE, &event);
					break;
				}

				// debug keys
				if (event.key.down && !event.key.repeat)
				{
					switch (event.key.key)
					{
					case SDLK_TAB:
						game_reset(&context, &state);
						break;
					case SDLK_F1:
						DEBUG_render_textures = !DEBUG_render_textures;
						break;
					case SDLK_F2:
						DEBUG_render_outlines = !DEBUG_render_outlines;
						break;
					case SDLK_F3:
						DEBUG_render_mouse_info = !DEBUG_render_mouse_info;
						break;
					}
				}
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				// map left/right buttons to action buttons for input system
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					sdl_input_mouse_button_process(&context, BTN_TYPE_ACTION_0, &event);
				}
				else if (event.button.button == SDL_BUTTON_RIGHT)
				{
					sdl_input_mouse_button_process(&context, BTN_TYPE_ACTION_1, &event);
				}
				// middle-click starts tile edit on down
				if (event.button.button == SDL_BUTTON_MIDDLE && event.button.down)
				{
					// compute mouse world and tile
					vec2f mouse_screen = vec2f{(float)event.button.x / (float)context.zoom, (float)event.button.y / (float)context.zoom};
					vec2f mouse_world = point_screen_to_global(&context, mouse_screen);
					int tx = (int)SDL_floorf(mouse_world.x);
					int ty = (int)SDL_floorf(mouse_world.y);
					if (tx >= 0 && tx < state.tilemap.w && ty >= 0 && ty < state.tilemap.h)
					{
						state.tile_edit_active = true;
						state.edit_tile_x = tx;
						state.edit_tile_y = ty;
						int idx = state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)];
						// save previous so Escape can restore
						state.edit_previous_idx = idx;
						state.edit_current_idx = idx;
						state.edit_buf_len = 0;
						state.edit_buffer[0] = '\0';
					}
				}
				break;
			}
			case SDL_EVENT_MOUSE_MOTION:
				context.mouse_pos.x = (float)event.motion.x / (float)context.zoom;
				context.mouse_pos.y = (float)event.motion.y / (float)context.zoom;
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				// detect modifier keys (Shift needed for changing tile with wheel)
				{
					SDL_Keymod mods = SDL_GetModState();
					bool shift_down = (mods & SDL_KMOD_SHIFT) != 0;

					// store scroll in context (used for zoom) only when not using Shift-edit
					if (!(state.tile_edit_active && shift_down))
						context.mouse_scroll = event.wheel.y;
					else
						context.mouse_scroll = 0; // prevent zoom while using wheel for editing

					// require Shift to change tile index with wheel (avoids interfering with zoom)
					if (state.tile_edit_active && event.wheel.y != 0 && shift_down)
					{
						int tx = state.edit_tile_x;
						int ty = state.edit_tile_y;
						if (tx >= 0 && tx < state.tilemap.w && ty >= 0 && ty < state.tilemap.h)
						{
							state.edit_current_idx += event.wheel.y;
							if (state.edit_current_idx < 0)
								state.edit_current_idx = 0;
							int max_idx = ATLAS_COLS * ATLAS_ROWS - 1;
							if (state.edit_current_idx > max_idx)
								state.edit_current_idx = max_idx;
							state.edit_buf_len = 0;
							state.edit_buffer[0] = '\0';
							state.tilemap.tiles[coords_to_tilemap_index(&state, tx, ty)] = (uint16_t)state.edit_current_idx;
						}
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

		if (elapsed_work < TARGET_FRAMERATE)
			SDL_DelayNS(TARGET_FRAMERATE - elapsed_work);
		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;

#ifdef ENABLE_DIAGNOSTICS
		{
			SDL_SetRenderDrawColor(context.renderer, 0x0, 0x00, 0x00, 0xCC);
			SDL_FRect rect = SDL_FRect{5, 5, 225, 75};
			SDL_RenderFillRect(context.renderer, &rect);

			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10, 10, "work: %9.6f ms/f", (float)elapsed_work / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 20, "tot : %9.6f ms/f", (float)elapsed_frame / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 30, "camera zoom = %f", context.camera_active->zoom);
			SDL_RenderDebugTextFormat(context.renderer, 10, 40, "[TAB] reset ");
			SDL_RenderDebugTextFormat(context.renderer, 10, 50, "[F1]  render textures   %s", DEBUG_render_textures ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 60, "[F2]  render outlines   %s", DEBUG_render_outlines ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 70, "[F3]  render mouse info %s", DEBUG_render_mouse_info ? " ON" : "OFF");
		}

		if (DEBUG_render_mouse_info)
		{
			// // TODO move info to mouse cursor coordinates
			const vec2f debug_rect_size = vec2f{200, 74};
			vec2f mouse_pos_screen = context.mouse_pos;
			vec2f mouse_pos_world = point_screen_to_global(&context, mouse_pos_screen);
			vec2f mouse_pos_camera = mouse_pos_world - context.camera_active->world_position;
			vec2f mouse_pos_tilemap = world_to_tile(state.tilemap, mouse_pos_world);
			vec2f debug_text_pos = mouse_pos_screen;
			debug_text_pos.x -= debug_rect_size.x;

			const int tilex = state.mouse_tile_x;
			const int tiley = state.mouse_tile_y;
			const int tile_idx = (tilex >= 0 && tiley >= 0) ? state.tilemap.tiles[coords_to_tilemap_index(&state, tilex, tiley)] : -1;

			itu_lib_render_draw_rect_fill(context.renderer, debug_text_pos, debug_rect_size, color{0.0f, 0.0f, 0.0f, 0.8f});

			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugText(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 02, "mouse pos");
			SDL_RenderDebugTextFormat(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 12, "screen  : %6.2f, %6.2f", mouse_pos_screen.x, mouse_pos_screen.y);
			SDL_RenderDebugTextFormat(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 22, "world   : %6.2f, %6.2f", mouse_pos_world.x, mouse_pos_world.y);
			SDL_RenderDebugTextFormat(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 32, "camera  : %6.2f, %6.2f", mouse_pos_camera.x, mouse_pos_camera.y);
			SDL_RenderDebugTextFormat(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 42, "tilemap : %6.0f, %6.0f", mouse_pos_tilemap.x, mouse_pos_tilemap.y);
			SDL_RenderDebugTextFormat(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 52, "player t:	%6d,	%6d", state.player_tile_x, state.player_tile_y);
			SDL_RenderDebugTextFormat(context.renderer, debug_text_pos.x + 2, debug_text_pos.y + 62, "tile idx:	%6d", tile_idx);
		}

		// debug window
		SDL_SetRenderDrawColor(context.renderer, 0xFF, 0x00, 0xFF, 0xff);
		SDL_RenderRect(context.renderer, NULL);
#endif

		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		walltime_frame_beg = walltime_frame_end;
	}
}
