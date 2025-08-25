#pragma once
#include <SDL3/SDL.h>

void movePlayer(SDL_Time time_elapsed_frame, bool btn_pressed_left,
                bool btn_pressed_right, bool btn_pressed_up,
                bool btn_pressed_down, SDL_FRect &player_rect,
                float player_speed, float window_w, float window_h);
