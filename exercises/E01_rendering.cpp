#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define ENABLE_DIAGNOSTICS
#define NUM_ASTEROIDS 10


#define VALIDATE(expression) if(!(expression)) { SDL_Log("%s\n", SDL_GetError()); }

#define NANOS(x)   (x)                // converts nanoseconds into nanoseconds
#define MICROS(x)  (NANOS(x) * 1000)  // converts microseconds into nanoseconds
#define MILLIS(x)  (MICROS(x) * 1000) // converts milliseconds into nanoseconds
#define SECONDS(x) (MILLIS(x) * 1000) // converts seconds into nanoseconds

#define NS_TO_MILLIS(x)  ((float)(x)/(float)1000000)    // converts nanoseconds to milliseconds (in floating point precision)
#define NS_TO_SECONDS(x) ((float)(x)/(float)1000000000) // converts nanoseconds to seconds (in floating point precision)

struct SDLContext
{
	SDL_Renderer* renderer;
	float window_w; // current window width after render zoom has been applied
	float window_h; // current window height after render zoom has been applied

	float delta;    // in seconds

	bool btn_pressed_up    = false;
	bool btn_pressed_down  = false;
	bool btn_pressed_left  = false;
	bool btn_pressed_right = false;
};

struct Entity
{
	SDL_FPoint   position;
	float        size;
	float        velocity;

	SDL_FRect    rect;
	SDL_Texture* texture_atlas;
	SDL_FRect    texture_rect;
};

struct GameState
{
	Entity player;
	Entity asteroids[NUM_ASTEROIDS];

	SDL_Texture* texture_atlas;
};

static float distance_between(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return SDL_sqrtf(dx*dx + dy*dy);
}

static float distance_between_sq(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx*dx + dy*dy;
}

static void init(SDLContext* context, GameState* game_state)
{
	// NOTE: these are a "design" parameter
	//       it is worth specifying a proper 
	const float entity_size_world = 64;
	const float entity_size_texture = 128;
	const float player_speed = entity_size_world * 5;
	const int   player_sprite_coords_x = 4;
	const int   player_sprite_coords_y = 0;
	const float asteroid_speed_min = entity_size_world * 2;
	const float asteroid_speed_range = entity_size_world * 4;
	const int   asteroid_sprite_coords_x = 0;
	const int   asteroid_sprite_coords_y = 4;
	// load textures
	{
		int w = 0;
		int h = 0;
		int n = 0;
		unsigned char* pixels = stbi_load("data/kenney/simpleSpace_tilesheet_2.png", &w, &h, &n, 0);

		SDL_assert(pixels);

		// we don't really need this SDL_Surface, but it's the most conveninet way to create out texture
		// NOTE: out image has the color channels in RGBA order, but SDL_PIXELFORMAT
		//       behaves the opposite on little endina architectures (ie, most of them)
		//       we won't worry too much about that, just remember this if your textures looks wrong
		//       - check that the the color channels are actually what you expect (how many? how big? which order)
		//       - if everythig looks right, you might just need to flip the channel order, because of SDL
		SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);
		game_state->texture_atlas = SDL_CreateTextureFromSurface(context->renderer, surface);
		
		// NOTE: the texture will make a copy of the pixel data, so after creatio we can release both surface and pixel data
		SDL_DestroySurface(surface);
		stbi_image_free(pixels);
	}

	// character
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

	// asteroids
	{
		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];

			asteroid_curr->position.x = entity_size_world + SDL_randf() * (context->window_w - entity_size_world * 2);
			asteroid_curr->position.y = -entity_size_world; // spawn asteroids off screen (almost)
			asteroid_curr->size       = entity_size_world;
			asteroid_curr->velocity   = asteroid_speed_min + SDL_randf() * asteroid_speed_range;
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

static void update(SDLContext* context, GameState* game_state)
{
	// player
	{
		Entity* entity_player = &game_state->player; 
		if(context->btn_pressed_up)
			entity_player->position.y -= context->delta * entity_player->velocity;
		if(context->btn_pressed_down)
			entity_player->position.y += context->delta * entity_player->velocity;
		if(context->btn_pressed_left)
			entity_player->position.x -= context->delta * entity_player->velocity;
		if(context->btn_pressed_right)
			entity_player->position.x += context->delta * entity_player->velocity;

		entity_player->rect.x = entity_player->position.x;
		entity_player->rect.y = entity_player->position.y;
		SDL_SetTextureColorMod(entity_player->texture_atlas, 0xFF, 0xFF, 0xFF);
		SDL_RenderTexture(
			context->renderer,
			entity_player->texture_atlas,
			&entity_player->texture_rect,
			&entity_player->rect
		);
	}

	// asteroids
	{
		// how close an asteroid must be before categorizing it as "too close" (100 pixels. We square it because we can avoid doing the square root later)
		const float warning_distance_sq = 100*100;

		// how close an asteroid must be before triggering a collision (64 pixels. We square it because we can avoid doing the square root later)
		// the number 64 is obtained by summing togheter the "radii" of the sprites
		const float collision_distance_sq = 64*64;

		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];
			asteroid_curr->position.y += context->delta * asteroid_curr->velocity;

			asteroid_curr->rect.x = asteroid_curr->position.x;
			asteroid_curr->rect.y = asteroid_curr->position.y;

			float distance_sq = distance_between_sq(asteroid_curr->position, game_state->player.position);
			if(distance_sq < collision_distance_sq)
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0x00, 0x00);
			else if(distance_sq < warning_distance_sq)
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xCC, 0xCC, 0x00);
			else
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0xFF, 0xFF);

			SDL_RenderTexture(
				context->renderer,
				asteroid_curr->texture_atlas,
				&asteroid_curr->texture_rect,
				&asteroid_curr->rect
			);
		}
	}
}

int main(void)
{
	SDLContext context = { 0 };
	GameState game_state = { 0 };

	float window_w = 600;
	float window_h = 800;
	int target_framerate = SECONDS(1) / 60;

	SDL_Window* window = SDL_CreateWindow("E01 - Rendering", window_w, window_h, 0);
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

				case SDL_EVENT_KEY_UP:
				case SDL_EVENT_KEY_DOWN:
					if(event.key.key == SDLK_W)
						context.btn_pressed_up = event.key.down;
					if(event.key.key == SDLK_A)
						context.btn_pressed_left = event.key.down;
					if(event.key.key == SDLK_S)
						context.btn_pressed_down = event.key.down;
					if(event.key.key == SDLK_D)
						context.btn_pressed_right = event.key.down;
			}
		}

		// clear screen
		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		update(&context, &game_state);

		SDL_GetCurrentTime(&walltime_work_end);
		time_elapsed_work = walltime_work_end - walltime_frame_beg;

		if(target_framerate > time_elapsed_work)
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
#endif

		// render
		SDL_RenderPresent(context.renderer);

		walltime_frame_beg = walltime_frame_end;
	}

	return 0;
};