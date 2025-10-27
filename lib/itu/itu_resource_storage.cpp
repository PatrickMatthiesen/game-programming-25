#ifndef ITU_UNITY_BUILD
#include <itu_resource_storage.hpp>

#include <SDL3/SDL.h>
#include <itu_common.hpp>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stb_ds.h>
#include <imgui/imgui.h>
#endif


struct TextureData
{
	SDL_Texture* texture;
};
static ITU_IdTexture id_tex_next;

struct AudioData
{
	MIX_Audio* audio;
};

struct FontData
{
	TTF_Font* font;
};
static ITU_IdTexture id_font_next;

struct ITU_ResourceStorageContext
{
	stbds_hm(ITU_IdTexture, TextureData) storage_texture;
	stbds_hm(ITU_IdAudio  , AudioData)   storage_audio;
	stbds_hm(ITU_IdFont   , FontData)    storage_font;

	stbds_hm(ITU_IdTexture, const char*) debug_names_texture;
	stbds_hm(ITU_IdAudio  , const char*) debug_names_audio;
	stbds_hm(ITU_IdFont   , const char*) debug_names_font;
};
ITU_ResourceStorageContext ctx_rstorage;

ITU_IdTexture itu_sys_rstorage_texture_load(SDLContext* context, const char* path, SDL_ScaleMode mode)
{
	SDL_Texture*  new_tex = texture_create(context, path, mode);

	if(!new_tex)
	{
		SDL_Log("Invalid or not supported texture file '%s'", path);
		return -1;
	}

	ITU_IdTexture new_tex_idx = itu_sys_rstorage_texture_add(new_tex);


#ifdef ENABLE_DIAGNOSTICS
	itu_sys_rstorage_texture_set_debug_name(new_tex_idx, path);
#endif

	return new_tex_idx;
}

ITU_IdTexture itu_sys_rstorage_texture_from_ptr(SDL_Texture* texture)
{
	// this is slow, but for the amount of textures we will have at the moment it's more than enough
	int num_textures = stbds_hmlen(ctx_rstorage.storage_texture);
	for(int i = 0; i < num_textures; ++i)
		if(ctx_rstorage.storage_texture[i].value.texture == texture)
			return ctx_rstorage.storage_texture[i].key;

	return -1;
}

SDL_Texture* itu_sys_rstorage_texture_get_ptr(ITU_IdTexture id)
{
	int tex_loc = stbds_hmgeti(ctx_rstorage.storage_texture, id);
	if(tex_loc == -1)
		return NULL;

	return ctx_rstorage.storage_texture[tex_loc].value.texture;
}

ITU_IdTexture itu_sys_rstorage_texture_add(SDL_Texture* texture)
{
	ITU_IdTexture new_tex_idx = id_tex_next++;
	TextureData   new_tex_data = { 0 };
	new_tex_data.texture = texture;

	stbds_hmput(ctx_rstorage.storage_texture, new_tex_idx, new_tex_data);

	return new_tex_idx;
}

void itu_sys_rstorage_texture_set_debug_name(ITU_IdTexture id, const char* debug_name)
{
	// NOTE: allocating every single name is BAD, but we haven't looked in allocaiton startegies and memory arenas yet
	int len = SDL_strlen(debug_name);
	char* name_storage = (char*)SDL_malloc(len + 1);
	SDL_memcpy(name_storage, debug_name, len);
	name_storage[len] = 0;

	stbds_hmput(ctx_rstorage.debug_names_texture, id, name_storage);
}

const char* itu_sys_rstorage_texture_get_debug_name(ITU_IdTexture id)
{
	int name_loc = stbds_hmgeti(ctx_rstorage.debug_names_texture, id);
	if(name_loc == -1)
		return NULL;

	return ctx_rstorage.debug_names_texture[name_loc].value;
}

// =====================================================================================
// fonts
// =====================================================================================
ITU_IdFont itu_sys_rstorage_font_load(SDLContext* context, const char* path, float size)
{
	TTF_Font*  new_font = TTF_OpenFont(path, size);

	if(!new_font)
	{
		SDL_Log("Invalid or not supported font file '%s'", path);
		return -1;
	}

	ITU_IdFont new_font_idx = itu_sys_rstorage_font_add(new_font);

#ifdef ENABLE_DIAGNOSTICS
	itu_sys_rstorage_font_set_debug_name(new_font_idx, path);
#endif

	return new_font_idx;
}

ITU_IdFont itu_sys_rstorage_font_add(TTF_Font* font)
{
	ITU_IdFont new_font_idx = id_font_next++;
	FontData new_font_data = { 0 };
	new_font_data.font= font;

	stbds_hmput(ctx_rstorage.storage_font, new_font_idx, new_font_data);

	return new_font_idx;
}

ITU_IdFont itu_sys_rstorage_font_from_ptr(TTF_Font* font)
{
	// this is slow, but for the amount of fonts we will have at the moment it's more than enough
	int num_fonts = stbds_hmlen(ctx_rstorage.storage_font);
	for(int i = 0; i < num_fonts; ++i)
		if(ctx_rstorage.storage_font[i].value.font == font)
			return ctx_rstorage.storage_font[i].key;

	return -1;
}

TTF_Font* itu_sys_rstorage_font_get_ptr(ITU_IdFont id)
{
	int font_loc = stbds_hmgeti(ctx_rstorage.storage_font, id);
	if(font_loc == -1)
		return NULL;

	return ctx_rstorage.storage_font[font_loc].value.font;
}

void itu_sys_rstorage_font_set_debug_name(ITU_IdFont id, const char* debug_name)
{
	// NOTE: allocating every single name is BAD, but we haven't looked in allocaiton startegies and memory arenas yet
	int len = SDL_strlen(debug_name);
	char* name_storage = (char*)SDL_malloc(len + 1);
	SDL_memcpy(name_storage, debug_name, len);
	name_storage[len] = 0;

	stbds_hmput(ctx_rstorage.debug_names_font, id, name_storage);
}

const char* itu_sys_rstorage_font_get_debug_name(ITU_IdFont id)
{
	int name_loc = stbds_hmgeti(ctx_rstorage.debug_names_font, id);
	if(name_loc == -1)
		return NULL;

	return ctx_rstorage.debug_names_font[name_loc].value;
}
// =====================================================================================
// Debug rendering
// =====================================================================================


SDL_DialogFileFilter font_filter[] = { { "TrueFont", "ttf;TTF" } };

void open_font_file_callback(void *userdata, const char * const *filelist, int filter)
{
	SDLContext* context = (SDLContext*)userdata;
	int len_base_path = SDL_strlen(context->working_dir);

	int i = 0;
	while(filelist[i])
	{
		const char* name = itu_lib_fileutils_get_file_name(filelist[i]);
		ITU_IdFont id = itu_sys_rstorage_font_load(context, filelist[i], 12);
		itu_sys_rstorage_font_set_debug_name(id, name);
		++i;
	}
}

enum ITU_SysRstorageDebugDetailCategory
{
	ITU_SYS_RSTORAGE_DETAIL_CATEGORY_TEXTURE,
	ITU_SYS_RSTORAGE_DETAIL_CATEGORY_AUDIO,
	ITU_SYS_RSTORAGE_DETAIL_CATEGORY_FONT,

	ITU_SYS_RSTORAGE_DETAIL_CATEGORY_MAX
};

const char* const sdl_enum_names_blendmode[] = 
{
	"SDL_BLENDMODE_NONE",
	"SDL_BLENDMODE_BLEND",
	"SDL_BLENDMODE_BLEND_PREMULTIPLIED",
	"SDL_BLENDMODE_ADD",
	"SDL_BLENDMODE_ADD_PREMULTIPLIED",
	"SDL_BLENDMODE_MOD",
	"SDL_BLENDMODE_MUL",
	"SDL_BLENDMODE_INVALID",
};

const char* const sdl_enum_names_scalemode[] = 
{
	"SDL_SCALEMODE_INVALID",
	"SDL_SCALEMODE_NEAREST",
	"SDL_SCALEMODE_LINEAR",
	"SDL_SCALEMODE_PIXELART",
};

inline int sdl_blendmode_to_debug_name_loc(SDL_BlendMode mode)
{
	switch(mode) {
		case SDL_BLENDMODE_NONE:                return 0;
		case SDL_BLENDMODE_BLEND:               return 1;
		case SDL_BLENDMODE_BLEND_PREMULTIPLIED: return 2;
		case SDL_BLENDMODE_ADD:                 return 3;
		case SDL_BLENDMODE_ADD_PREMULTIPLIED:   return 4;
		case SDL_BLENDMODE_MOD:                 return 5;
		case SDL_BLENDMODE_MUL:                 return 6;
		case SDL_BLENDMODE_INVALID:             return 7;
	}
}

inline SDL_BlendMode sdl_blendmode_from_debug_name_loc(int loc)
{
	switch(loc) {
		case 0: return SDL_BLENDMODE_NONE;
		case 1: return SDL_BLENDMODE_BLEND;
		case 2: return SDL_BLENDMODE_BLEND_PREMULTIPLIED;
		case 3: return SDL_BLENDMODE_ADD;
		case 4: return SDL_BLENDMODE_ADD_PREMULTIPLIED;
		case 5: return SDL_BLENDMODE_MOD;
		case 6: return SDL_BLENDMODE_MUL;
		case 7: return SDL_BLENDMODE_INVALID;
	}
}

inline int sdl_scalemode_to_debug_name_loc(SDL_ScaleMode mode)
{
	switch(mode) {
		case SDL_SCALEMODE_INVALID:  return 0;
		case SDL_SCALEMODE_NEAREST:  return 1;
		case SDL_SCALEMODE_LINEAR:   return 2;
		case SDL_SCALEMODE_PIXELART: return 3;
	}
}

inline SDL_ScaleMode sdl_scalemode_from_debug_name_loc(int loc)
{
	switch(loc) {
		case 0: return SDL_SCALEMODE_INVALID;
		case 1: return SDL_SCALEMODE_NEAREST;
		case 2: return SDL_SCALEMODE_LINEAR;
		case 3: return SDL_SCALEMODE_PIXELART;
	}
}

void itu_sys_rstorage_debug_render_detail_texture(SDLContext* context, int loc)
{
	SDL_Texture* texture = ctx_rstorage.storage_texture[loc].value.texture;

	if(!texture)
	{
		ImGui::Text("Invalid texture");
		return;
	}

	vec2f size;
	color c;
	SDL_BlendMode blend_mode;
	SDL_ScaleMode scale_mode;
	SDL_GetTextureSize(texture, &size.x, &size.y);
	SDL_GetTextureBlendMode(texture, &blend_mode);
	SDL_GetTextureScaleMode(texture, &scale_mode);

	ImGui::InputFloat2("size (readonly)", &size.x, "%.0f", ImGuiInputTextFlags_ReadOnly);

	int blendmode_loc = sdl_blendmode_to_debug_name_loc(blend_mode);
	if(ImGui::Combo("blend mode", &blendmode_loc, sdl_enum_names_blendmode, (int)array_size(sdl_enum_names_blendmode), -1))
		SDL_SetTextureBlendMode(texture, sdl_blendmode_from_debug_name_loc(blendmode_loc));

	int scalemode_loc = sdl_scalemode_to_debug_name_loc(scale_mode);
	if(ImGui::Combo("scale mode", &scalemode_loc, sdl_enum_names_scalemode, (int)array_size(sdl_enum_names_scalemode), -1))
		SDL_SetTextureScaleMode(texture, sdl_scalemode_from_debug_name_loc(scalemode_loc));

	ImVec2 cursor_pos = ImGui::GetCursorPos();
	ImVec2 window_space_left = ImGui::GetContentRegionAvail();
	float tex_aspect_ratio = size.y / size.x;
	ImVec2 prev_size;
	prev_size.x = SDL_min(texture->w, window_space_left.x);
	prev_size.y = prev_size.x * tex_aspect_ratio;
	
	ImGui::SetCursorPosY(SDL_max(cursor_pos.y, cursor_pos.y + window_space_left.y - prev_size.y - ImGui::GetTextLineHeightWithSpacing() - 15));
	ImGui::SeparatorText("Preview");
	ImGui::Image(texture, prev_size);
}

void itu_sys_rstorage_debug_render_detail_audio(SDLContext* context, int loc)
{
	// TODO
	ImGui::Text("TODO NotYetImplemented");

}
void itu_sys_rstorage_debug_render_detail_font(SDLContext* context, int loc)
{
	TTF_Font* font = ctx_rstorage.storage_font[loc].value.font;

	if(!font)
	{
		ImGui::Text("Invalid font");
		return;
	}

	// readonly
	int descent = TTF_GetFontDescent(font);
	const char* family_name = TTF_GetFontFamilyName(font);

	const char* style_name = TTF_GetFontStyleName(font);
	int weight = TTF_GetFontWeight(font);

	// modifiable
	float size = TTF_GetFontSize(font);
	int line_skip = TTF_GetFontLineSkip(font);
	TTF_FontStyleFlags style_flags = TTF_GetFontStyle(font);

	ImGui::LabelText("font family (readonly)", family_name);
	ImGui::LabelText("font style (readonly)", style_name);
	ImGui::InputInt("weight (readonly)", &weight, 0, 0, ImGuiInputTextFlags_ReadOnly);

	if(ImGui::DragFloat("size", &size))
		TTF_SetFontSize(font, size);

	if(ImGui::DragInt("line skip", &line_skip))
		TTF_SetFontLineSkip(font, line_skip);

	bool style_dirty = false;
	style_dirty |= ImGui::CheckboxFlags("bold", &style_flags, TTF_STYLE_BOLD);
	style_dirty |= ImGui::CheckboxFlags("italic", &style_flags, TTF_STYLE_ITALIC);
	style_dirty |= ImGui::CheckboxFlags("underline", &style_flags, TTF_STYLE_UNDERLINE);
	style_dirty |= ImGui::CheckboxFlags("strikethrough", &style_flags, TTF_STYLE_STRIKETHROUGH);
	if(style_dirty)
		TTF_SetFontStyle(font, style_flags);
}

void itu_sys_rstorage_debug_render(SDLContext* context)
{
	static ITU_SysRstorageDebugDetailCategory detail_category = ITU_SYS_RSTORAGE_DETAIL_CATEGORY_MAX;
	static int loc_selected = -1;

	//ImGui::Begin("debug_rstorage", NULL, ImGuiWindowFlags_NoCollapse);

	// TODO: master/detail setup will be very common, think about a reusable solution for it
	ImGui::BeginChild("debug_rstorage_master", ImVec2(200, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	{
		if(ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int textures_count = stbds_hmlen(ctx_rstorage.storage_texture);
			if(ImGui::BeginTable("debug_rstorage_master_textures", 3, ImGuiTableFlags_SizingFixedFit))
			{

				ImGui::TableSetupColumn("");
				ImGui::TableSetupColumn("name");
				ImGui::TableSetupColumn("idx");
				ImGui::TableHeadersRow();
				for(int i = 0; i < textures_count; ++i)
				{
					ITU_IdTexture id = ctx_rstorage.storage_texture[i].key;

					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					char buf_id[48];
					SDL_snprintf(buf_id, 48, "%3d##debug_rstorage_master_textures", i);
					if(ImGui::Selectable(
						buf_id,
						detail_category == ITU_SYS_RSTORAGE_DETAIL_CATEGORY_TEXTURE && i == loc_selected,
						ImGuiSelectableFlags_SpanAllColumns
					))
					{
						loc_selected = i;
						detail_category = ITU_SYS_RSTORAGE_DETAIL_CATEGORY_TEXTURE;
					}

					ImGui::TableNextColumn();
					int pos_debug_name = stbds_hmgeti(ctx_rstorage.debug_names_texture, id);
					if(pos_debug_name != -1)
						ImGui::Text("%s", ctx_rstorage.debug_names_texture[pos_debug_name].value);

					ImGui::TableNextColumn();
					ImGui::Text("%d", id);
				}

				ImGui::EndTable();
			}
		}

		if(ImGui::CollapsingHeader("Audio clips", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// TODO
			ImGui::Text("TODO NotYetImplemented");
		}

		if(ImGui::Button("+##add_font"))
		{
			SDL_ShowOpenFileDialog(open_font_file_callback, context, NULL, font_filter, array_size(font_filter), context->working_dir, false);
		}
		ImGui::SameLine();
		if(ImGui::CollapsingHeader("Fonts", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int fonts_count = stbds_hmlen(ctx_rstorage.storage_font);
			if(ImGui::BeginTable("debug_rstorage_master_fonts", 3, ImGuiTableFlags_SizingFixedFit))
			{

				ImGui::TableSetupColumn("");
				ImGui::TableSetupColumn("name");
				ImGui::TableSetupColumn("idx");
				ImGui::TableHeadersRow();
				for(int i = 0; i < fonts_count; ++i)
				{
					ITU_IdFont id = ctx_rstorage.storage_font[i].key;

					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					char buf_id[48];
					SDL_snprintf(buf_id, 48, "%3d##debug_rstorage_master_fonts", i);
					if(ImGui::Selectable(
						buf_id,
						detail_category == ITU_SYS_RSTORAGE_DETAIL_CATEGORY_FONT && i == loc_selected,
						ImGuiSelectableFlags_SpanAllColumns
					))
					{
						loc_selected = i;
						detail_category = ITU_SYS_RSTORAGE_DETAIL_CATEGORY_FONT;
					}

					ImGui::TableNextColumn();
					int pos_debug_name = stbds_hmgeti(ctx_rstorage.debug_names_font, id);
					if(pos_debug_name != -1)
						ImGui::Text("%s", ctx_rstorage.debug_names_font[pos_debug_name].value);

					ImGui::TableNextColumn();
					ImGui::Text("%d", id);
				}

				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	ImGui::BeginChild("debug_rstorage_detail", ImVec2(0, 0), ImGuiChildFlags_Border);
	{
		if(loc_selected != -1)
			switch(detail_category)
			{
				case ITU_SYS_RSTORAGE_DETAIL_CATEGORY_TEXTURE: itu_sys_rstorage_debug_render_detail_texture(context, loc_selected); break;
				case ITU_SYS_RSTORAGE_DETAIL_CATEGORY_AUDIO  : itu_sys_rstorage_debug_render_detail_audio  (context, loc_selected); break;
				case ITU_SYS_RSTORAGE_DETAIL_CATEGORY_FONT   : itu_sys_rstorage_debug_render_detail_font   (context, loc_selected); break;
				default: /* do nothing */ break;
			}
		ImGui::EndChild();
	}

	//ImGui::End();
}

bool itu_sys_rstorage_debug_render_font(TTF_Font* font, TTF_Font** new_font)
{
	bool ret = false;

	ITU_IdFont font_id = itu_sys_rstorage_font_from_ptr(font);
	const char* font_name = itu_sys_rstorage_font_get_debug_name(font_id);

	if(ImGui::InputInt("font", (int*)&font_id))
	{
		*new_font = itu_sys_rstorage_font_get_ptr(font_id);
		ret = true;
	}
	ImGui::Text("\t%s", font_name);
	
	return ret;
}

bool itu_sys_rstorage_debug_render_texture(SDL_Texture* texture, SDL_Texture** new_texture, SDL_FRect* rect)
{
	bool ret = false;

	ITU_IdTexture texture_id = itu_sys_rstorage_texture_from_ptr(texture);
	const char* texture_name = itu_sys_rstorage_texture_get_debug_name(texture_id);

	if(ImGui::InputInt("texture", (int*)&texture_id))
	{
		*new_texture = itu_sys_rstorage_texture_get_ptr(texture_id);
		ret = true;
	}
	ImGui::Text("\t%s", texture_name);
	
	if(ImGui::IsItemHovered())
	{
		//if(ImGui::BeginTooltip())
		{
			ImVec2 uv0, uv1, size;
			if(rect != NULL)
			{
				uv0 = ImVec2 {
					rect->x / texture->w,
					rect->y / texture->h
				};
				uv1 = ImVec2 {
					(rect->x + rect->w) / texture->w,
					(rect->y + rect->h) / texture->h
				};
			}
			else
			{
				uv0 = ImVec2 { 0, 0 };
				uv1 = ImVec2 { 1, 1 };
			}

			float aspect_ratio = rect->h / rect->w;

			size.x = SDL_min(100, rect->w);
			size.y = size.x * aspect_ratio;
			ImGui::Image(texture, size, uv0, uv1);
		}
	}
	

	return ret;
}