#define STB_IMAGE_IMPLEMENTATION

#define ITU_UNITY_BUILD


#include <SDL3/SDL.h>
#include <stb_image.h>

#include <itu_common.hpp>
#include <itu_lib_render.hpp>
#include <itu_lib_overlaps.hpp>

#define ENABLE_DIAGNOSTICS

#define TARGET_FRAMERATE SECONDS(1) / 60
#define WINDOW_W 1000
#define WINDOW_H 900

#define ENTITY_COUNT 1000
#define MAX_COLLISIONS 2024   // num max collisions per frame
#define COLLISION_RADIUS 12

static const int CollisionZoneSize = 2 * COLLISION_RADIUS; // cell size heuristic: ~ 2x radius
// Max entities per cell (tune). Previous heuristic could produce very large numbers; keep modest to catch clustering.
static const int MaxEntitiesPerCell = 32; // adjust as needed; assertions will warn if insufficient
static const int NumCellX = (WINDOW_W + CollisionZoneSize - 1) / CollisionZoneSize;
static const int NumCellY = (WINDOW_H + CollisionZoneSize - 1) / CollisionZoneSize;
static const int NumCells = NumCellX * NumCellY;

bool DEBUG_separate_collisions   = true;
bool DEBUG_render_colliders      = true;
bool DEBUG_render_texture_border = false;


struct Entity;
struct EntityCollisionInfo;

struct SDLContext
{
	SDL_Renderer* renderer;
	float zoom;     // render zoom
	float window_w;	// current window width after render zoom has been applied
	float window_h;	// current window width after render zoom has been applied

	float delta;    // in seconds
	float uptime;   // in seconds

	bool btn_isdown_up;
	bool btn_isdown_down;
	bool btn_isdown_left;
	bool btn_isdown_right;
	bool btn_isdown_space;
};

struct GameState
{
	Entity* player;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;

	EntityCollisionInfo* frame_collisions;
	int frame_collisions_count;

	// SDL-allocated structures
	SDL_Texture* atlas;

	// Cells store entity indices to remain valid across swap-delete
	int cells[NumCells][MaxEntitiesPerCell];
	int cell_counts[NumCells];

	// Diagnostics
	int dbg_overflow_count;
	int dbg_max_cell_count;
	float dbg_avg_cell_count;

	// RNG state (simple LCG for deterministic spawning)
	uint32_t rng_state;

	// Collision diagnostics
	bool collisions_truncated;
	int  collisions_capacity; // equals MAX_COLLISIONS for diagnostics
};

static inline uint32_t rng_next(GameState* s)
{
    // LCG constants from Numerical Recipes
    s->rng_state = s->rng_state * 1664525u + 1013904223u;
    return s->rng_state;
}

static inline float rng_next01(GameState* s)
{
    return (rng_next(s) >> 8) * (1.0f / 16777216.0f); // 24-bit mantissa fraction
}

static SDL_Texture* texture_create(SDLContext* context, const char* path)
{
	int w=0, h=0, n=0;
	unsigned char* pixels = stbi_load(path, &w, &h, &n, 0);
	SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);

	SDL_Texture* ret = SDL_CreateTextureFromSurface(context->renderer, surface);

	SDL_DestroySurface(surface);
	stbi_image_free(pixels);

	return ret;
}

// ********************************************************************************************************************
// sprite
// ********************************************************************************************************************

struct Sprite
{
	SDL_Texture* texture;
	SDL_FRect    rect;
	color        tint;
	vec2f        pivot;
};

// quick sprite rendering function that takes care of most of the functionalities
// NOTE: this function is still temporary since ATM we can't really deal with game worlds bigger than the rendering window
//       we will address it in lecture 03, and then we will just create a final sprite system and be done with it
static void sprite_render(SDLContext* context, vec2f position, vec2f size, Sprite* sprite)
{
	SDL_FRect dst_rect;
	dst_rect.w = size.x;
	dst_rect.h = size.y;
	dst_rect.x = position.x - dst_rect.w * sprite->pivot.x;
	dst_rect.y = position.y - dst_rect.h * sprite->pivot.y;
		
	SDL_SetTextureColorModFloat(sprite->texture, sprite->tint.r, sprite->tint.g, sprite->tint.b);
	SDL_SetTextureAlphaModFloat(sprite->texture, sprite->tint.a);
	SDL_RenderTexture(context->renderer, sprite->texture, &sprite->rect, &dst_rect);

	if(DEBUG_render_texture_border)
	{
		SDL_SetRenderDrawColorFloat(context->renderer, 1, 1, 1, 1);
		SDL_RenderRect(context->renderer, &dst_rect);
	}
}

// ********************************************************************************************************************
// entity
// ********************************************************************************************************************

struct Entity
{
	vec2f position;
	vec2f size;

	Sprite sprite;

	// collider info
	float collider_radius;
	vec2f collider_offset;
};

static Entity* entity_create(GameState* state)
{
	if(!(state->entities_alive_count < ENTITY_COUNT))
		return NULL;

	// // concise version
	//return &state->entities[state->entities_alive_count++];

	Entity* ret = &state->entities[state->entities_alive_count];
	++state->entities_alive_count;

	// initialize defaults that might not be overwritten later
	ret->collider_offset = vec2f{0,0};

	return ret;
}

// NOTE: this only works if nobody holds references to other entities!
static void entity_destroy(GameState* state, Entity* entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity >= state->entities && entity < state->entities + ENTITY_COUNT);

	--state->entities_alive_count;
	*entity = state->entities[state->entities_alive_count];
}
// ********************************************************************************************************************
// collisions
// ********************************************************************************************************************

static int get_cell(float x, float y)
// Gets the cell index for the given position
{
	return SDL_clamp((int)(x / CollisionZoneSize), 0, NumCellX - 1)      // x
		+ SDL_clamp((int)(y / CollisionZoneSize), 0, NumCellY - 1) * NumCellX; // y)
}

static void partition_entities(GameState* state, SDLContext* context)
{
	// Clear cell counts
	for(int i=0; i < NumCells; ++i) state->cell_counts[i] = 0;
	state->dbg_overflow_count = 0;
	state->collisions_truncated = false; // reset each frame

	// Insert entities (single cell based on center)
	for(int i=0; i < state->entities_alive_count; ++i)
	{
		Entity* e = &state->entities[i];
		int cell = get_cell(e->position.x, e->position.y);
		if(cell < 0 || cell >= NumCells) continue; // should not happen with clamping
		int count = state->cell_counts[cell];
		if(count < MaxEntitiesPerCell)
		{
			state->cells[cell][count] = i; // store index
			state->cell_counts[cell] = count + 1;
		}
		else
		{
			++state->dbg_overflow_count;
			// Keep last valid indexes; skip additional ones
			SDL_Log("[WARN] cell %d overflow (cap=%d, attempted count=%d)", cell, MaxEntitiesPerCell, count+1);
		}
	}

	// Diagnostics aggregation
	int total = 0; int nonEmpty=0; int maxc=0;
	for(int c=0; c < NumCells; ++c){
		int cc = state->cell_counts[c];
		if(cc){ total += cc; ++nonEmpty; if(cc>maxc) maxc=cc; }
	}
	state->dbg_max_cell_count = maxc;
	state->dbg_avg_cell_count = nonEmpty? (float)total / (float)nonEmpty : 0.0f;
}


struct EntityCollisionInfo
{
	Entity* e1;
	Entity* e2;

	vec2f normal;
	float separation;
};

static void collision_check(GameState* state)
{
	state->frame_collisions_count = 0;

	// Forward neighbor offsets (covers all unique pairs once)
	const int OFF[][2] = {
		{0,0}, {1,0}, {0,1}, {1,1}, {-1,1}
	};
	const int OFF_COUNT = (int)(sizeof(OFF)/sizeof(OFF[0]));

	for(int cy = 0; cy < NumCellY; ++cy)
	{
		for(int cx = 0; cx < NumCellX; ++cx)
		{
			int baseCell = cy * NumCellX + cx;
			int countA = state->cell_counts[baseCell];
			int* listA = state->cells[baseCell];

			for(int n = 0; n < OFF_COUNT; ++n)
			{
				int nx = cx + OFF[n][0];
				int ny = cy + OFF[n][1];
				if(nx < 0 || ny < 0 || nx >= NumCellX || ny >= NumCellY) continue;
				int cellB = ny * NumCellX + nx;

				if(cellB == baseCell)
				{
					// internal unordered pairs
					if(countA < 2) continue;
					for(int i = 0; i < countA - 1; ++i)
					{
						Entity* e1 = &state->entities[listA[i]];
						for(int j = i + 1; j < countA; ++j)
						{
							Entity* e2 = &state->entities[listA[j]];

							if(!itu_lib_overlaps_circle_circle(
								e1->position + e1->collider_offset, e1->collider_radius,
								e2->position + e2->collider_offset, e2->collider_radius))
								continue;

							e1->sprite.tint = COLOR_RED;
							e2->sprite.tint = COLOR_RED;

							if(state->frame_collisions_count >= MAX_COLLISIONS)
							{
								state->collisions_truncated = true;
								continue; // keep scanning other pairs for diagnostics consistency
							}

							vec2f p1 = e1->position + e1->collider_offset;
							vec2f p2 = e2->position + e2->collider_offset;
							vec2f v = p2 - p1;
							float l = length(v);
							if(l <= 0.00001f) { v = vec2f{1,0}; l = 1.0f; }
							float separation_vector = e1->collider_radius + e2->collider_radius - l;

							int new_collision_idx = state->frame_collisions_count++;
							state->frame_collisions[new_collision_idx].e1 = e1;
							state->frame_collisions[new_collision_idx].e2 = e2;
							state->frame_collisions[new_collision_idx].normal = v / l;
							state->frame_collisions[new_collision_idx].separation = separation_vector;
						}
					}
				}
				else
				{
					int countB = state->cell_counts[cellB];
					if(countA == 0 || countB == 0) continue;
					int* listB = state->cells[cellB];

					for(int ia = 0; ia < countA; ++ia)
					{
						Entity* e1 = &state->entities[listA[ia]];
						for(int ib = 0; ib < countB; ++ib)
						{
							Entity* e2 = &state->entities[listB[ib]];
							if(!itu_lib_overlaps_circle_circle(
								e1->position + e1->collider_offset, e1->collider_radius,
								e2->position + e2->collider_offset, e2->collider_radius))
								continue;

							e1->sprite.tint = COLOR_RED;
							e2->sprite.tint = COLOR_RED;

							if(state->frame_collisions_count >= MAX_COLLISIONS)
							{
								state->collisions_truncated = true;
								continue; // do not early out entire pass
							}
							vec2f p1 = e1->position + e1->collider_offset;
							vec2f p2 = e2->position + e2->collider_offset;
							vec2f v = p2 - p1;
							float l = length(v);
							if(l <= 0.00001f) { v = vec2f{1,0}; l = 1.0f; }
							float separation_vector = e1->collider_radius + e2->collider_radius - l;

							int new_collision_idx = state->frame_collisions_count++;
							state->frame_collisions[new_collision_idx].e1 = e1;
							state->frame_collisions[new_collision_idx].e2 = e2;
							state->frame_collisions[new_collision_idx].normal = v / l;
							state->frame_collisions[new_collision_idx].separation = separation_vector;
						}
					}
				}
			}
		}
	}
}
static void collision_separate(GameState* state)
{
	for(int i = 0; i < state->frame_collisions_count; ++i)
	{
		EntityCollisionInfo entity_collision_info = state->frame_collisions[i];

		vec2f sep = entity_collision_info.normal * entity_collision_info.separation / 2;
		entity_collision_info.e1->position -= sep;
		entity_collision_info.e2->position += sep;
	}

	// Clamp to world after resolution (simple world bounds 0..WINDOW_W/H)
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* e = &state->entities[i];
		float r = e->collider_radius;
		if(e->position.x < r) e->position.x = r;
		if(e->position.x > WINDOW_W - r) e->position.x = WINDOW_W - r;
		if(e->position.y < r) e->position.y = r;
		if(e->position.y > WINDOW_H - r) e->position.y = WINDOW_H - r;
	}
}

// ********************************************************************************************************************
// game
// ********************************************************************************************************************

static void game_init(SDLContext* context, GameState* state)
{
	// contiguous memory
	{
		state->entities = (Entity*)SDL_malloc(ENTITY_COUNT * sizeof(Entity));
		SDL_assert(state->entities);

		state->frame_collisions = (EntityCollisionInfo*)SDL_malloc(MAX_COLLISIONS * sizeof(EntityCollisionInfo));
		SDL_assert(state->frame_collisions);
	}

	// texture atlasesw
	state->atlas = texture_create(context, "data/kenney/simpleSpace_tilesheet_2.png");

}

static void game_reset(SDLContext* context, GameState* state)
{
	SDL_memset(state->entities, 0, ENTITY_COUNT * sizeof(Entity));
	state->entities_alive_count = 0;
	state->rng_state = 0xC001C0DEu; // deterministic seed

	// entities
	Entity* player = entity_create(state);
	// we always have a player. This should also always be the first entity created, so it should never fail
	SDL_assert(player);
	player->position.x = (float)context->window_w / 2;
	player->position.y = (float)context->window_h / 2;
	player->size = vec2f{ 64, 64 };
	player->sprite = {
		.texture = state->atlas,
		.rect = SDL_FRect{ 0, 0, 128, 128 },
		.tint = COLOR_WHITE,
		.pivot = vec2f{ 0.5f, 0.5f }
	};
	player->collider_radius = COLLISION_RADIUS;
	state->player = player;

	// Pseudo-random uniform spawn (deterministic). Spread all entities, avoid wrapping artifact.
	int toSpawn = ENTITY_COUNT - 1; // excluding player
	for(int i = 0; i < toSpawn; ++i)
	{
		Entity* entity = entity_create(state);
		if(!entity) break;
		float rx = rng_next01(state); // [0,1)
		float ry = rng_next01(state);
		// Keep entities away from very edge by one radius to reduce immediate clamping
		float x = COLLISION_RADIUS + rx * (context->window_w - 2 * COLLISION_RADIUS);
		float y = COLLISION_RADIUS + ry * (context->window_h - 2 * COLLISION_RADIUS);
		// Small jitter around position (already random, but can help future deterministic patterns)
		entity->size = vec2f{ 32, 32 };
		entity->position = vec2f{ x, y };
		entity->sprite = {
			.texture = state->atlas,
			.rect = SDL_FRect{ 0, 4*128, 128, 128 },
			.tint = COLOR_WHITE,
			.pivot = vec2f{ 0.5f, 0.5f }
		};
		entity->collider_offset = vec2f{0,0};
		entity->collider_radius = COLLISION_RADIUS;
	}

	// reset collisions
	// state->frame_collisions_count = 0;
	// partition_entities(state, context);
}

static void game_update(SDLContext* context, GameState* state)
{
	vec2f mov = { 0 };
	if(context->btn_isdown_up)
		mov.y -= 1;
	if(context->btn_isdown_down)
		mov.y += 1;
	if(context->btn_isdown_left)
		mov.x -= 1;
	if(context->btn_isdown_right)
		mov.x += 1;

	vec2f velocity = normalize(mov) * (128 * context->delta);
	state->player->position = state->player->position + velocity;

	// reset tint
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		entity->sprite.tint = COLOR_WHITE;

		// Clamp to window
		float r = entity->collider_radius;
        if(entity->position.x < r) entity->position.x = r;
        if(entity->position.x > context->window_w - r) entity->position.x = context->window_w - r;
        if(entity->position.y < r) entity->position.y = r;
        if(entity->position.y > context->window_h - r) entity->position.y = context->window_h - r;

	}

	partition_entities(state, context);

	// collisions
	collision_check(state);
	if(DEBUG_separate_collisions)
		collision_separate(state);
}

static void game_render(SDLContext* context, GameState* state)
{
	// render
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		sprite_render(context, entity->position, entity->size, &entity->sprite);

		if(DEBUG_render_colliders)
		{
			itu_lib_render_draw_point(context->renderer, entity->position + entity->collider_offset, 5, COLOR_GREEN);
			itu_lib_render_draw_circle(
				context->renderer,
				entity->position + entity->collider_offset,
				entity->collider_radius,
				16, COLOR_GREEN
			);
		}
	}
		
	// debug window
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
	SDL_RenderRect(context->renderer, NULL);
}

int main(void)
{
	int a = sizeof(int*);
	bool quit = false;
	SDL_Window* window;
	SDLContext context = { 0 };
	GameState  state   = { 0 };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	SDL_CreateWindowAndRenderer("E02 - Collisions", context.window_w, context.window_h, 0, &window, &context.renderer);

	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	
	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		context.zoom = 1;
		context.window_w /= context.zoom;
		context.window_h /= context.zoom;
		SDL_SetRenderScale(context.renderer, context.zoom, context.zoom);
	}

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
						case SDLK_W: context.btn_isdown_up    = event.key.down; break;
						case SDLK_A: context.btn_isdown_left  = event.key.down; break;
						case SDLK_S: context.btn_isdown_down  = event.key.down; break;
						case SDLK_D: context.btn_isdown_right = event.key.down; break;
						case SDLK_SPACE: context.btn_isdown_space = event.key.down; break;
					}

					// debug keys
					if(event.key.down && !event.key.repeat)
					{
						switch(event.key.key)
						{
							case SDLK_TAB: game_reset(&context, &state); break;
							case SDLK_F1: DEBUG_separate_collisions   = !DEBUG_separate_collisions;   break;
							case SDLK_F2: DEBUG_render_colliders      = !DEBUG_render_colliders;      break;
							case SDLK_F3: DEBUG_render_texture_border = !DEBUG_render_texture_border; break;
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
			SDL_DelayPrecise(TARGET_FRAMERATE - elapsed_work);
		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;
		

#ifdef ENABLE_DIAGNOSTICS
		{
			SDL_SetRenderDrawColor(context.renderer, 0x0, 0x00, 0x00, 0xCC);
			SDL_FRect rect = SDL_FRect{ 5, 5, 300, 110 };
			SDL_RenderFillRect(context.renderer, &rect);
			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10, 10, "work: %9.6f ms/f", (float)elapsed_work  / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 20, "tot : %9.6f ms/f", (float)elapsed_frame / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 30, "[TAB] reset ");
			SDL_RenderDebugTextFormat(context.renderer, 10, 40, "[F1]  collisions        %s", DEBUG_separate_collisions   ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 50, "[F2]  render colliders  %s", DEBUG_render_colliders      ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 60, "[F3]  render tex border %s", DEBUG_render_texture_border ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 70, "grid avg/max: %.1f / %d", state.dbg_avg_cell_count, state.dbg_max_cell_count);
			SDL_RenderDebugTextFormat(context.renderer, 10, 80, "grid overflow: %d", state.dbg_overflow_count);
			SDL_RenderDebugTextFormat(context.renderer, 10, 90, "collisions: %d%s", state.frame_collisions_count, state.collisions_truncated?" (TRUNC)":"");
		}
#endif

		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		walltime_frame_beg = walltime_frame_end;
	}
}
