#include "E00_introduction.h"
#include <SDL3/SDL.h>


int main(void) {
  SDL_Log("hello sdl");

  float window_w = 800;
  float window_h = 600;
  int target_framerate_ms = 1000 / 60;       // 16 milliseconds
  int target_framerate_ns = 1000000000 / 60; // 16666666 nanoseconds

  SDL_Window *window =
      SDL_CreateWindow("E00 - introduction", window_w, window_h, 0);
  SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

  // increase the zoom to make debug text more legible
  // (ie, on the class projector, we will usually use 2)
  {
    float zoom = 2;
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

  float player_size = 40;
  float player_speed = 200.0f; // logical pixels per second
  SDL_FRect player_rect;
  player_rect.w = player_size;
  player_rect.h = player_size;
  player_rect.x = window_w / 4 - player_size / 2;
  player_rect.y = window_h / 2 - player_size / 2;

  SDL_FRect player_rect_2;
  player_rect_2.w = player_size;
  player_rect_2.h = player_size;
  player_rect_2.x = (3 * (window_w / 4)) - player_size / 2;
  player_rect_2.y = window_h / 2 - player_size / 2;

  bool btn_pressed_up_p1 = false;
  bool btn_pressed_down_p1 = false;
  bool btn_pressed_left_p1 = false;
  bool btn_pressed_right_p1 = false;

  bool btn_pressed_up_p2 = false;
  bool btn_pressed_down_p2 = false;
  bool btn_pressed_left_p2 = false;
  bool btn_pressed_right_p2 = false;

  // initialize to target frame time to avoid garbage dt on first frame
  time_elapsed_frame = target_framerate_ns;

  SDL_GetCurrentTime(&walltime_frame_beg);
  while (!quit) {
    // input
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
        quit = true;
        break;
      case SDL_EVENT_KEY_DOWN: {
        int keycode = event.key.key;          // SDL_Keycode
        SDL_Scancode sc = event.key.scancode; // SDL_Scancode
        if (keycode >= SDLK_0 && keycode < SDLK_5) {
          delay_type = keycode - SDLK_0;
          break; // exit switch(event.type)
        }

        switch (sc) {
        case SDL_SCANCODE_W:
          btn_pressed_up_p1 = true;
          break;
        case SDL_SCANCODE_S:
          btn_pressed_down_p1 = true;
          break;
        case SDL_SCANCODE_A:
          btn_pressed_left_p1 = true;
          break;
        case SDL_SCANCODE_D:
          btn_pressed_right_p1 = true;
          break;
		case SDL_SCANCODE_UP:
		  btn_pressed_up_p2 = true;
		  break;
		case SDL_SCANCODE_DOWN:
		  btn_pressed_down_p2 = true;
		  break;
		case SDL_SCANCODE_LEFT:
		  btn_pressed_left_p2 = true;
		  break;
		case SDL_SCANCODE_RIGHT:
		  btn_pressed_right_p2 = true;
		  break;
        }
        break;
      }
      case SDL_EVENT_KEY_UP: {
        SDL_Scancode sc = event.key.scancode;
        switch (sc) {
        case SDL_SCANCODE_W:
          btn_pressed_up_p1 = false;
          break;
        case SDL_SCANCODE_S:
          btn_pressed_down_p1 = false;
          break;
        case SDL_SCANCODE_A:
          btn_pressed_left_p1 = false;
          break;
        case SDL_SCANCODE_D:
          btn_pressed_right_p1 = false;
          break;
		case SDL_SCANCODE_UP:
		  btn_pressed_up_p2 = false;
		  break;
		case SDL_SCANCODE_DOWN:
		  btn_pressed_down_p2 = false;
		  break;
		case SDL_SCANCODE_LEFT:
		  btn_pressed_left_p2 = false;
		  break;
		case SDL_SCANCODE_RIGHT:
		  btn_pressed_right_p2 = false;
		  break;
        }
        break;
      }
      }
    }

    // update (smooth, frame rate independent movement)
    {
      // move player 1
      movePlayer(time_elapsed_frame, btn_pressed_left_p1, btn_pressed_right_p1,
                 btn_pressed_up_p1, btn_pressed_down_p1, player_rect, player_speed,
                 window_w, window_h);
      // move player 2
      movePlayer(time_elapsed_frame, btn_pressed_left_p2, btn_pressed_right_p2,
                 btn_pressed_up_p2, btn_pressed_down_p2, player_rect_2, player_speed,
                 window_w, window_h);
    }

    // clear screen
    // NOTE: `0x` prefix means we are expressing the number in hexadecimal (base
    // 16)
    //       `0b` is another useful prefix, expresses the number in binary
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 0x3C, 0x63, 0xFF, 0XFF);
    SDL_RenderFillRect(renderer, &player_rect);

    SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
    SDL_RenderFillRect(renderer, &player_rect_2);

    SDL_GetCurrentTime(&walltime_work_end);
    time_elapsed_work = walltime_work_end - walltime_frame_beg;

    if (target_framerate_ns > time_elapsed_work) {
      switch (delay_type) {
      case 0: {
        // busy wait - very precise, but costly
        SDL_Time walltime_busywait = walltime_work_end;
        while (walltime_busywait - walltime_frame_beg < target_framerate_ns)
          SDL_GetCurrentTime(&walltime_busywait);
        break;
      }
      case 1: {
        // simple delay - too imprecise
        // NOTE: `SDL_Delay` gets milliseconds, but our timer gives us
        // nanoseconds! We need to covert it manually
        SDL_Delay((target_framerate_ns - time_elapsed_work) / 1000000);
        break;
      }
      case 2: {
        // delay ns - also too imprecise
        SDL_DelayNS(target_framerate_ns - time_elapsed_work);
        break;
      }
      case 3: {
        // delay precise
        SDL_DelayPrecise(target_framerate_ns - time_elapsed_work);
        break;
      }
      case 4: {
        // custom delay - we use the sleeping delay with an arbitrary margin,
        // then we busywait what's left
        SDL_DelayNS(target_framerate_ns - time_elapsed_work - 1000000);
        SDL_Time walltime_busywait = walltime_work_end;

        while (walltime_busywait - walltime_frame_beg < target_framerate_ns)
          SDL_GetCurrentTime(&walltime_busywait);
        break;
      }
      }
    }

    SDL_GetCurrentTime(&walltime_frame_end);
    time_elapsed_frame = walltime_frame_end - walltime_frame_beg;

    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderDebugTextFormat(renderer, 10.0f, 10.0f,
                              "elapsed (frame): %9.6f ms",
                              (float)time_elapsed_frame / (float)1000000);
    SDL_RenderDebugTextFormat(renderer, 10.0f, 20.0f,
                              "elapsed(work   : %9.6f ms",
                              (float)time_elapsed_work / (float)1000000);
    SDL_RenderDebugTextFormat(renderer, 10.0f, 30.0f,
                              "delay type: %d (change with 0-4)", delay_type);

    // render
    SDL_RenderPresent(renderer);

    walltime_frame_beg = walltime_frame_end;
  }

  return 0;
};

void movePlayer(SDL_Time time_elapsed_frame, bool btn_pressed_left,
                bool btn_pressed_right, bool btn_pressed_up,
                bool btn_pressed_down, SDL_FRect &player_rect,
                float player_speed, float window_w, float window_h) {
  float dt = (float)time_elapsed_frame / 1000000000.0f; // seconds
  float dx = 0.0f, dy = 0.0f;
  if (btn_pressed_left)
    dx -= 1.0f;
  if (btn_pressed_right)
    dx += 1.0f;
  if (btn_pressed_up)
    dy -= 1.0f;
  if (btn_pressed_down)
    dy += 1.0f;
  // normalize diagonal to keep speed consistent
  if (dx != 0.0f && dy != 0.0f) {
    const float invsqrt2 = 0.70710678f;
    dx *= invsqrt2;
    dy *= invsqrt2;
  }

  player_rect.x += dx * player_speed * dt;
  player_rect.y += dy * player_speed * dt;

  // optional: keep player on-screen in logical coordinates
  if (player_rect.x < 0)
    player_rect.x = 0;
  if (player_rect.y < 0)
    player_rect.y = 0;
  if (player_rect.x + player_rect.w > window_w)
    player_rect.x = window_w - player_rect.w;
  if (player_rect.y + player_rect.h > window_h)
    player_rect.y = window_h - player_rect.h;
}