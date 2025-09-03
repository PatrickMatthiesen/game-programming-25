#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define ENABLE_DIAGNOSTICS
#define NUM_ASTEROIDS 10
#define NUM_BULLETS 20

#define VALIDATE(expression)             \
	if (!(expression))                   \
	{                                    \
		SDL_Log("%s\n", SDL_GetError()); \
	}

#define NANOS(x) (x)				  // converts nanoseconds into nanoseconds
#define MICROS(x) (NANOS(x) * 1000)	  // converts microseconds into nanoseconds
#define MILLIS(x) (MICROS(x) * 1000)  // converts milliseconds into nanoseconds
#define SECONDS(x) (MILLIS(x) * 1000) // converts seconds into nanoseconds

#define NS_TO_MILLIS(x) ((float)(x) / (float)1000000)	  // converts nanoseconds to milliseconds (in floating point precision)
#define NS_TO_SECONDS(x) ((float)(x) / (float)1000000000) // converts nanoseconds to seconds (in floating point precision)

struct SDLContext
{
	SDL_Renderer *renderer;
	float window_w; // current window width after render zoom has been applied
	float window_h; // current window height after render zoom has been applied

	float delta; // in seconds

	bool btn_pressed_up = false;
	bool btn_pressed_down = false;
	bool btn_pressed_left = false;
	bool btn_pressed_right = false;

	bool btn_pressed_fire = false;
};

struct Entity
{
	SDL_FPoint position;
	float size;
	float velocity;

	SDL_FRect rect;
	SDL_Texture *texture_atlas;
	SDL_FRect texture_rect;

	bool active;
};

struct GameState
{
	Entity player;
	Entity asteroids[NUM_ASTEROIDS];
	Entity bullets[NUM_BULLETS];

	SDL_Texture *texture_atlas;

	int next_bullet_index;
	float fire_cooldown;
};

static float distance_between(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return SDL_sqrtf(dx * dx + dy * dy);
}

static float distance_between_sq(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx * dx + dy * dy;
}

static void init_asteroid_top_random(Entity *asteroid, float window_w, float entity_size_world, float asteroid_speed_min, float asteroid_speed_range)
{
	asteroid->position.x = entity_size_world + SDL_randf() * (window_w - entity_size_world * 2);
	asteroid->position.y = -entity_size_world; // spawn asteroids off screen (almost)
	asteroid->velocity = asteroid_speed_min + SDL_randf() * asteroid_speed_range;
}

static void load_textures(GameState *game_state, SDLContext *context);

static void init_character(GameState *game_state, SDLContext *context, const float entity_size_world, const float entity_size_texture, const float player_speed, const int player_sprite_coords_x, const int player_sprite_coords_y);

static void init_asteroids(GameState *game_state, SDLContext *context, const float entity_size_world, const float entity_size_texture, const float asteroid_speed_min, const float asteroid_speed_range, const int asteroid_sprite_coords_x, const int asteroid_sprite_coords_y);

static void init_bullets(GameState *game_state, SDLContext *context, const float entity_size_world, const float entity_size_texture, const int bullet_sprite_coords_x, const int bullet_sprite_coords_y);

static void init(SDLContext *context, GameState *game_state)
{
	// NOTE: these are a "design" parameter
	//       it is worth specifying a proper
	const float entity_size_world = 64;
	const float entity_size_texture = 128;
	const float player_speed = entity_size_world * 5;
	const int player_sprite_coords_x = 6;
	const int player_sprite_coords_y = 0;
	const float asteroid_speed_min = entity_size_world * 2;
	const float asteroid_speed_range = entity_size_world * 4;
	const int asteroid_sprite_coords_x = 0;
	const int asteroid_sprite_coords_y = 4;
	const int bullet_sprite_coords_x = 6;
	const int bullet_sprite_coords_y = 5;

	// load textures
	load_textures(game_state, context);

	// character
	init_character(game_state, context, entity_size_world, entity_size_texture, player_speed, player_sprite_coords_x, player_sprite_coords_y);

	// asteroids
	init_asteroids(game_state, context, entity_size_world, entity_size_texture, asteroid_speed_min, asteroid_speed_range, asteroid_sprite_coords_x, asteroid_sprite_coords_y);

	// bullets
	init_bullets(game_state, context, entity_size_world, entity_size_texture, bullet_sprite_coords_x, bullet_sprite_coords_y);
}

void init_character(GameState *game_state, SDLContext *context, const float entity_size_world, const float entity_size_texture, const float player_speed, const int player_sprite_coords_x, const int player_sprite_coords_y)
{
	{
		game_state->player.position.x = context->window_w / 2 - entity_size_world / 2;
		game_state->player.position.y = context->window_h - entity_size_world * 2;
		game_state->player.size = entity_size_world;
		game_state->player.velocity = player_speed;
		game_state->player.texture_atlas = game_state->texture_atlas;

		// player size in the game world
		game_state->player.rect.w = game_state->player.size;
		game_state->player.rect.h = game_state->player.size;

		// sprite size (in the tilemap)
		game_state->player.texture_rect.w = entity_size_texture;
		game_state->player.texture_rect.h = entity_size_texture;
		// sprite position (in the tilemap)
		game_state->player.texture_rect.x = entity_size_texture * player_sprite_coords_x;
		game_state->player.texture_rect.y = entity_size_texture * player_sprite_coords_y;
	}
}

void init_asteroids(GameState *game_state, SDLContext *context, const float entity_size_world, const float entity_size_texture, const float asteroid_speed_min, const float asteroid_speed_range, const int asteroid_sprite_coords_x, const int asteroid_sprite_coords_y)
{
	{
		for (int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity *asteroid_curr = &game_state->asteroids[i];

			asteroid_curr->position.x = entity_size_world + SDL_randf() * (context->window_w - entity_size_world * 2);
			asteroid_curr->position.y = -entity_size_world + SDL_randf() * (context->window_h + entity_size_world); // spawn asteroids off screen
			asteroid_curr->size = entity_size_world;
			asteroid_curr->velocity = asteroid_speed_min + SDL_randf() * asteroid_speed_range;
			asteroid_curr->texture_atlas = game_state->texture_atlas;

			asteroid_curr->rect.w = asteroid_curr->size;
			asteroid_curr->rect.h = asteroid_curr->size;

			asteroid_curr->texture_rect.w = entity_size_texture;
			asteroid_curr->texture_rect.h = entity_size_texture;

			asteroid_curr->texture_rect.x = entity_size_texture * asteroid_sprite_coords_x;
			asteroid_curr->texture_rect.y = entity_size_texture * asteroid_sprite_coords_y;
		}
	}
}

void init_bullets(GameState *game_state, SDLContext *context, const float entity_size_world, const float entity_size_texture, const int bullet_sprite_coords_x, const int bullet_sprite_coords_y)
{
	{
		for (int i = 0; i < NUM_BULLETS; ++i)
		{
			Entity *bullet_curr = &game_state->bullets[i];

			bullet_curr->position.x = -1000.0f; // off-screen
			bullet_curr->position.y = -1000.0f; // off-screen
			bullet_curr->rect.x = bullet_curr->position.x;
			bullet_curr->rect.y = bullet_curr->position.y;

			bullet_curr->size = entity_size_world;
			bullet_curr->velocity = 0.0f;
			bullet_curr->active = false;
			bullet_curr->texture_atlas = game_state->texture_atlas;

			bullet_curr->rect.w = bullet_curr->size;
			bullet_curr->rect.h = bullet_curr->size;

			bullet_curr->texture_rect.w = entity_size_texture;
			bullet_curr->texture_rect.h = entity_size_texture;
			bullet_curr->texture_rect.x = entity_size_texture * bullet_sprite_coords_x;
			bullet_curr->texture_rect.y = entity_size_texture * bullet_sprite_coords_y;
		}

		game_state->next_bullet_index = 0;
		game_state->fire_cooldown = 0.0f;
	}
}

void load_textures(GameState *game_state, SDLContext *context)
{
	{
		int w = 0;
		int h = 0;
		int n = 0;
		unsigned char *pixels = stbi_load("../data/kenney/simpleSpace_tilesheet_2.png", &w, &h, &n, 0);

		SDL_assert(pixels);

		// we don't really need this SDL_Surface, but it's the most conveninet way to create out texture
		// NOTE: out image has the color channels in RGBA order, but SDL_PIXELFORMAT
		//       behaves the opposite on little endina architectures (ie, most of them)
		//       we won't worry too much about that, just remember this if your textures looks wrong
		//       - check that the the color channels are actually what you expect (how many? how big? which order)
		//       - if everythig looks right, you might just need to flip the channel order, because of SDL
		SDL_Surface *surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);
		game_state->texture_atlas = SDL_CreateTextureFromSurface(context->renderer, surface);

		// NOTE: the texture will make a copy of the pixel data, so after creation we can release both surface and pixel data
		SDL_DestroySurface(surface);
		stbi_image_free(pixels);
	}
}

static void spawn_bullet(GameState* game_state)
{
    // pick a slot (try NUM_BULLETS times, then overwrite the cursor)
    int idx = game_state->next_bullet_index;
    for (int tries = 0; tries < NUM_BULLETS; ++tries)
    {
        if (!game_state->bullets[idx].active) break;
        idx = (idx + 1) % NUM_BULLETS;
    }

    Entity* b = &game_state->bullets[idx];

    const float bullet_speed = game_state->player.size * 10.0f; // fast and simple
    // b->size = b->size; // already set in init
    b->velocity = bullet_speed;
    b->active = true;

    // spawn from top-center of player
    b->position.x = game_state->player.position.x + game_state->player.size * 0.5f - b->size * 0.5f;
    b->position.y = game_state->player.position.y - b->size * 0.25f;

    b->rect.x = b->position.x;
    b->rect.y = b->position.y;

    game_state->next_bullet_index = (idx + 1) % NUM_BULLETS;
}

static void update(SDLContext *context, GameState *game_state)
{
	// player
	{
		Entity *entity_player = &game_state->player;
		if (context->btn_pressed_up)
			entity_player->position.y -= context->delta * entity_player->velocity;
		if (context->btn_pressed_down)
			entity_player->position.y += context->delta * entity_player->velocity;
		if (context->btn_pressed_left)
			entity_player->position.x -= context->delta * entity_player->velocity;
		if (context->btn_pressed_right)
			entity_player->position.x += context->delta * entity_player->velocity;

		if (entity_player->position.x < 0)
			entity_player->position.x = 0;
		if (entity_player->position.x + entity_player->size > context->window_w)
			entity_player->position.x = context->window_w - entity_player->size;

		entity_player->rect.x = entity_player->position.x;
		entity_player->rect.y = entity_player->position.y;
		SDL_SetTextureColorMod(entity_player->texture_atlas, 0xFF, 0xFF, 0xFF);
		SDL_RenderTexture(
			context->renderer,
			entity_player->texture_atlas,
			&entity_player->texture_rect,
			&entity_player->rect);
	}

	// asteroids
	{
		// how close an asteroid must be before categorizing it as "too close" (100 pixels. We square it because we can avoid doing the square root later)
		const float warning_distance_sq = 100 * 100;

		// how close an asteroid must be before triggering a collision (64 pixels. We square it because we can avoid doing the square root later)
		// the number 64 is obtained by summing togheter the "radii" of the sprites
		const float collision_distance_sq = 64 * 64;

		for (int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity *asteroid_curr = &game_state->asteroids[i];

			// if the asteroid is off the bottom of the screen, respawn it at the top
			if (asteroid_curr->position.y > context->window_h)
			{
				asteroid_curr->position.y = -64;
				asteroid_curr->position.x = 64 + SDL_randf() * (context->window_w - 64 * 2);
			}

			asteroid_curr->position.y += context->delta * asteroid_curr->velocity;

			asteroid_curr->rect.x = asteroid_curr->position.x;
			asteroid_curr->rect.y = asteroid_curr->position.y;

			float distance_sq = distance_between_sq(asteroid_curr->position, game_state->player.position);
			if (distance_sq < collision_distance_sq)
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0x00, 0x00);
			else if (distance_sq < warning_distance_sq)
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xCC, 0xCC, 0x00);
			else
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0xFF, 0xFF);

			SDL_RenderTexture(
				context->renderer,
				asteroid_curr->texture_atlas,
				&asteroid_curr->texture_rect,
				&asteroid_curr->rect);
		}
	}

	// bullets
    {
        // fire control (hold to auto-fire)
        const float fire_interval = 0.15f; // seconds between shots
        if (game_state->fire_cooldown > 0.0f)
            game_state->fire_cooldown -= context->delta;

        if (context->btn_pressed_fire && game_state->fire_cooldown <= 0.0f)
        {
            spawn_bullet(game_state);
            game_state->fire_cooldown = fire_interval;
        }

        for (int i = 0; i < NUM_BULLETS; ++i)
        {
            Entity *bullet_curr = &game_state->bullets[i];
            if (!bullet_curr->active) continue;

            bullet_curr->position.y -= context->delta * bullet_curr->velocity;

            // deactivate when off-screen
            if (bullet_curr->position.y + bullet_curr->size < 0.0f)
            {
                bullet_curr->active = false;
                continue;
            }

            bullet_curr->rect.x = bullet_curr->position.x;
            bullet_curr->rect.y = bullet_curr->position.y;

            SDL_SetTextureColorMod(bullet_curr->texture_atlas, 0xFF, 0xFF, 0xFF);
            SDL_RenderTexture(
                context->renderer,
                bullet_curr->texture_atlas,
                &bullet_curr->texture_rect,
                &bullet_curr->rect
            );
        }
    }
}

int main(void)
{
	SDLContext context = {0};
	GameState game_state = {0};

	float window_w = 600;
	float window_h = 800;
	int target_framerate = SECONDS(1) / 60;

	SDL_Window *window = SDL_CreateWindow("E01 - Rendering", window_w, window_h, 0);
	context.renderer = SDL_CreateRenderer(window, NULL);
	context.window_w = window_w;
	context.window_h = window_h;

	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		float zoom = 1;
		context.window_w /= zoom;
		context.window_h /= zoom;
		SDL_SetRenderScale(context.renderer, zoom, zoom);
	}

	bool quit = false;

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_work_end;
	SDL_Time walltime_frame_end;
	SDL_Time time_elapsed_frame;
	SDL_Time time_elapsed_work;

	init(&context, &game_state);

	SDL_GetCurrentTime(&walltime_frame_beg);
	while (!quit)
	{
		// input
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				quit = true;
				break;

			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_KEY_DOWN:
				const bool is_key_down = (event.type == SDL_EVENT_KEY_DOWN);
				if (event.key.key == SDLK_W)
					context.btn_pressed_up = is_key_down;
				if (event.key.key == SDLK_A)
					context.btn_pressed_left = is_key_down;
				if (event.key.key == SDLK_S)
					context.btn_pressed_down = is_key_down;
				if (event.key.key == SDLK_D)
					context.btn_pressed_right = is_key_down;
				if (event.key.key == SDLK_SPACE)
					context.btn_pressed_fire = is_key_down;
			}
		}

		// clear screen
		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		update(&context, &game_state);

		SDL_GetCurrentTime(&walltime_work_end);
		time_elapsed_work = walltime_work_end - walltime_frame_beg;

		if (target_framerate > time_elapsed_work)
		{
			SDL_DelayPrecise(target_framerate - time_elapsed_work);
		}

		SDL_GetCurrentTime(&walltime_frame_end);
		time_elapsed_frame = walltime_frame_end - walltime_frame_beg;

		context.delta = NS_TO_SECONDS(time_elapsed_frame);

#ifdef ENABLE_DIAGNOSTICS
		SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 10.0f, "elapsed (frame): %9.6f ms", NS_TO_MILLIS(time_elapsed_frame));
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 20.0f, "elapsed(work)  : %9.6f ms", NS_TO_MILLIS(time_elapsed_work));
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 30.0f, "Next active    : %d", game_state.bullets[game_state.next_bullet_index].active);
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 40.0f, "Bullets active : %d / %d", NUM_BULLETS - (game_state.next_bullet_index + 1), NUM_BULLETS);
#endif

		// render
		SDL_RenderPresent(context.renderer);

		walltime_frame_beg = walltime_frame_end;
	}

	return 0;
};