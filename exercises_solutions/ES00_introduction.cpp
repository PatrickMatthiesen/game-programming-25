#include <SDL3/SDL.h>

int main(void)
{
	SDL_Log("hello sdl");
	
	// toggle to swith between the insulated player update (aka, the way you want to do it)
	// and the one performed immediate after polling the event queue
	bool use_insulated_player_update = true;

	float window_w = 800;
	float window_h = 600;
	int target_framerate_ms = 1000 / 60;       // 16 milliseconds
	int target_framerate_ns = 1000000000 / 60; // 16666666 nanoseconds

	SDL_Window* window = SDL_CreateWindow("ES00 - introduction (solved)", window_w, window_h, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		float zoom = 1;
		window_w /= zoom;
		window_h /= zoom;
		SDL_SetRenderScale(renderer, zoom, zoom);
	}

	bool quit = false;

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_work_end;
	SDL_Time walltime_frame_end = 0;
	SDL_Time time_elapsed_frame;
	SDL_Time time_elapsed_work;

	int delay_type = 0;

	float player_speed = 2;
	float player_size = 40;
	SDL_FRect player_rect;
	player_rect.w = player_size;
	player_rect.h = player_size;
	player_rect.x = window_w / 2 - player_size / 2;
	player_rect.y = window_h / 2 - player_size / 2;

	// NOTE: list of stuff with the same prefix? Looks like it's a good candidate for consolidation
	bool btn_pressed_up    = false;
	bool btn_pressed_down  = false;
	bool btn_pressed_left  = false;
	bool btn_pressed_right = false;

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

				// NOTE: when there is no break, both switch cases will execute the same code block below.
				//       These kind of "clever" solutions can become messy very fast.
				//       We will soon move it to a more appropriate function (with a more solid event parsing).
				case SDL_EVENT_KEY_UP:
				case SDL_EVENT_KEY_DOWN:
				{
					// player inputs
					// NOTE: OS and hardware will notify events at their own pace, re-triggering events and other shenanigans.
					//       We want to insulate our game code from this, so here we will just keep track of events happening and
					//       do ACTUAL updates later in the loop
					if(use_insulated_player_update)
					{
						if(event.key.key == SDLK_W) btn_pressed_up    = event.key.down;
						if(event.key.key == SDLK_S) btn_pressed_down  = event.key.down;
						if(event.key.key == SDLK_A) btn_pressed_left  = event.key.down;
						if(event.key.key == SDLK_D) btn_pressed_right = event.key.down;

						
					}
					else
					{
						if(event.key.key == SDLK_W) player_rect.y -= player_speed;
						if(event.key.key == SDLK_S) player_rect.y += player_speed;
						if(event.key.key == SDLK_A) player_rect.x -= player_speed;
						if(event.key.key == SDLK_D) player_rect.x += player_speed;
					}

					// debug utilities
					if (event.key.down)
					{
						if(event.key.key >= SDLK_0 && event.key.key < SDLK_5)
							delay_type = event.key.key - SDLK_0;
						if(event.key.key == SDLK_F1)
							use_insulated_player_update = !use_insulated_player_update;
					}
					break;
				}
			}
		}

		// clear screen
		// NOTE: `0x` prefix means we are expressing the number in hexadecimal (base 16)
		//       `0b` is another useful prefix, expresses the number in binary
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(renderer);

		if(use_insulated_player_update)
		{
			// update player position
			if(btn_pressed_up)    player_rect.y -= player_speed;
			if(btn_pressed_down)  player_rect.y += player_speed;
			if(btn_pressed_left)  player_rect.x -= player_speed;
			if(btn_pressed_right) player_rect.x += player_speed;
		}

		SDL_SetRenderDrawColor(renderer, 0x3C, 0x63, 0xFF, 0XFF);
		SDL_RenderFillRect(renderer, &player_rect);

		SDL_GetCurrentTime(&walltime_work_end);
		time_elapsed_work = walltime_work_end - walltime_frame_beg;

		if(target_framerate_ns > time_elapsed_work)
		{
			switch(delay_type)
			{
				case 0:
				{
					// busy wait - very precise, but costly
					SDL_Time walltime_busywait = walltime_work_end;
					while(walltime_busywait - walltime_frame_beg < target_framerate_ns)
						SDL_GetCurrentTime(&walltime_busywait);
					break;
				}
				case 1:
				{
					// simple delay - too imprecise
					// NOTE: `SDL_Delay` gets milliseconds, but our timer gives us nanoseconds! We need to covert it manually
					SDL_Delay((target_framerate_ns - time_elapsed_work) / 1000000);
					break;
				}
				case 2:
				{
					// delay ns - also too imprecise
					SDL_DelayNS(target_framerate_ns - time_elapsed_work);
					break;
				}
				case 3:
				{
					// delay precise
					SDL_DelayPrecise(target_framerate_ns - time_elapsed_work);
					break;
				}
				case 4:
				{
					// custom delay - we use the sleeping delay with an arbitrary safety margin, then we busywait what's left
					SDL_DelayNS(target_framerate_ns - time_elapsed_work - 1000000);
					SDL_Time walltime_busywait = walltime_work_end;

					while(walltime_busywait - walltime_frame_beg < target_framerate_ns)
						SDL_GetCurrentTime(&walltime_busywait);
					break;
				}
			}
		}
		SDL_GetCurrentTime(&walltime_frame_end);
		time_elapsed_frame = walltime_frame_end - walltime_frame_beg;

		SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		SDL_RenderDebugTextFormat(renderer, 10.0f, 10.0f, "elapsed (frame): %9.6f ms", (float)time_elapsed_frame/(float)1000000);
		SDL_RenderDebugTextFormat(renderer, 10.0f, 20.0f, "elapsed (work) : %9.6f ms", (float)time_elapsed_work/(float)1000000);
		SDL_RenderDebugTextFormat(renderer, 10.0f, 40.0f, "delay  type (change with 0-4): %d", delay_type);
		SDL_RenderDebugTextFormat(renderer, 10.0f, 50.0f, "update type (toggle with F1) : %9s", use_insulated_player_update ? "INSULATED" : "IMMEDIATE");

		// render
		SDL_RenderPresent(renderer);
		
		walltime_frame_beg = walltime_frame_end;
	}

	return 0;
};


