#ifndef ITU_RESOURCE_STORAGE_HPP
#define ITU_RESOURCE_STORAGE_HPP

#ifndef ITU_UNITY_BUILD
#include <itu_engine.hpp>
#endif

typedef Uint32 ITU_IdTexture;
typedef Uint32 ITU_IdAudio;
typedef Uint32 ITU_IdFont;

ITU_IdTexture itu_sys_rstorage_texture_load(SDLContext* context, const char* path, SDL_ScaleMode mode);
ITU_IdTexture itu_sys_rstorage_texture_add(SDL_Texture* texture);
ITU_IdTexture itu_sys_rstorage_texture_from_ptr(SDL_Texture* texture);
SDL_Texture*  itu_sys_rstorage_texture_get_ptr(ITU_IdTexture id);
void          itu_sys_rstorage_texture_set_debug_name(ITU_IdTexture id, const char* debug_name);
const char*   itu_sys_rstorage_texture_get_debug_name(ITU_IdTexture id);


ITU_IdFont  itu_sys_rstorage_font_load(SDLContext* context, const char* path, float size);
ITU_IdFont  itu_sys_rstorage_font_add(TTF_Font* font);
ITU_IdFont  itu_sys_rstorage_font_from_ptr(TTF_Font* font);
TTF_Font*   itu_sys_rstorage_font_get_ptr(ITU_IdFont id);
void        itu_sys_rstorage_font_set_debug_name(ITU_IdFont id, const char* debug_name);
const char* itu_sys_rstorage_font_get_debug_name(ITU_IdFont id);


void itu_sys_rstorage_debug_render(SDLContext* context);
bool itu_sys_rstorage_debug_render_font(TTF_Font* font, TTF_Font** new_font);
bool itu_sys_rstorage_debug_render_texture(SDL_Texture* texture, SDL_Texture** new_texture, SDL_FRect* rect);

#endif // ITU_RESOURCE_STORAGE_HPP