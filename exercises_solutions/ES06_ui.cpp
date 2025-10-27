#define TEXTURE_PIXELS_PER_UNIT 128 // how many pixels of textures will be mapped to a single world unit
#define CAMERA_PIXELS_PER_UNIT  32  // how many pixels of windows will be used to render a single world unit

#define ENABLE_DIAGNOSTICS

// rendering framerate
#define TARGET_FRAMERATE_NS     (SECONDS(1) / 60)

// physics timestep
#define PHYSICS_TIMESTEP_NSECS  (SECONDS(1) / 60)
#define PHYSICS_TIMESTEP_SECS   NS_TO_SECONDS(PHYSICS_TIMESTEP_NSECS)
#define PHYSICS_MAX_TIMESTEPS_PER_FRAME 4
#define PHYSICS_MAX_CONTACTS_PER_ENTITY 16

#define WINDOW_W         1600
#define WINDOW_H         600

#include <itu_unity_include.hpp>

#define ENTITY_COUNT 4


// ui colors
#define EX6_COLOR_BTN_DEFAULT color { 0.5f, 0.5f, 0.5f, 1.0f }
#define EX6_COLOR_BTN_HOVER   color { 0.75f, 0.75f, 0.75f, 1.0f }
#define EX6_COLOR_BTN_CLICK   color { 1.0f, 1.0f, 1.0f, 1.0f }

float design_speed_linear;
float design_speed_rotational;

enum EX6_Tags
{
	TAG_CAMERA_TARGET,
	TAG_ASTEROID
};

struct EX6_PlayerData
{
	float curr_speed_linear;
	float curr_speed_rotational;

	ITU_EntityId target;
};
register_component(EX6_PlayerData)

struct EX6_Health
{
	float max;
	float curr;
};
register_component(EX6_Health)

struct EX6_HealthRenderer
{
	float widget_base_w;
	ITU_EntityId target;
};
register_component(EX6_HealthRenderer)

struct EX6_TransformScreen
{
	vec2f position;
	vec2f scale;
	float rotation;
};
register_component(EX6_TransformScreen)

struct EX6_Sprite9Patch
{
	SDL_Texture* texture;
	SDL_FRect    rect;
	vec2f        pivot;
	vec2f        size;
	vec2f        margins_hor;
	vec2f        margins_ver;
	color        tint;
};
register_component(EX6_Sprite9Patch)

struct EX6_ImageButton
{
	TTF_Text* ttf_text; // owned

	void (*fn_callback_hover)(SDLContext* context, ITU_EntityId id);
	void (*fn_callback_click)(SDLContext* context, ITU_EntityId id);
};
register_component(EX6_ImageButton)


static ITU_EntityId id_player;

static TTF_TextEngine* ttf_engine;

// ============================================================================================
// TMP methods
// ============================================================================================

void ex6_system_camera_target(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		Transform* transform = entity_get_data(id, Transform);

		context->camera_active->world_position = transform->position;
	}
}

void ex6_lib_sprite_render_camera(SDLContext* context, Sprite* sprite, EX6_TransformScreen* transform)
{
	SDL_FRect rect_src = sprite->rect;
	SDL_FRect rect_dst;

	rect_dst.w = transform->scale.x * rect_src.w;
	rect_dst.h = transform->scale.y * rect_src.h;
	rect_dst.x = transform->position.x - sprite->pivot.x * rect_dst.w;
	rect_dst.y = transform->position.y - sprite->pivot.y * rect_dst.h;


	SDL_FPoint pivot_dst;
	pivot_dst.x = sprite->pivot.x * rect_dst.w;
	pivot_dst.y = sprite->pivot.y * rect_dst.h;

	sdl_set_texture_tint(sprite->texture, sprite->tint);
	SDL_RenderTextureRotated(
		context->renderer,
		sprite->texture,
		&rect_src,
		&rect_dst,
		(-transform->rotation) * RAD_2_DEG,
		&pivot_dst,
		sprite->flip_horizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE
	);
}

void ex6_lib_sprite9patch_render_camera(SDLContext* context, EX6_Sprite9Patch* sprite, EX6_TransformScreen* transform)
{
	SDL_FRect rect_src = sprite->rect;
	SDL_FRect rect_dst;

	rect_dst.w = transform->scale.x * sprite->size.x;
	rect_dst.h = transform->scale.y * sprite->size.y;
	rect_dst.x = transform->position.x - sprite->pivot.x * rect_dst.w;
	rect_dst.y = transform->position.y - sprite->pivot.y * rect_dst.h;

	rect_dst.w = SDL_max(rect_dst.w, sprite->margins_hor.x + sprite->margins_hor.y);
	rect_dst.h = SDL_max(rect_dst.h, sprite->margins_ver.x + sprite->margins_ver.y);

	SDL_FPoint pivot_dst;
	pivot_dst.x = sprite->pivot.x * rect_dst.w;
	pivot_dst.y = sprite->pivot.y * rect_dst.h;

	sdl_set_texture_tint(sprite->texture, sprite->tint);
	SDL_RenderTexture9Grid(
		context->renderer,
		sprite->texture,
		&rect_src,
		sprite->margins_hor.x,
		sprite->margins_hor.y,
		sprite->margins_ver.x,
		sprite->margins_ver.y,
		transform->scale.x,
		&rect_dst
	);
}

void ex6_system_sprite_render_camera(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		EX6_TransformScreen* transform = entity_get_data(id, EX6_TransformScreen);
		Sprite*              sprite    = entity_get_data(id, Sprite);

		ex6_lib_sprite_render_camera(context, sprite, transform);
	}

	// outline render target
	sdl_set_render_draw_color(context, { 1, 0, 1, 1 });
	SDL_RenderRect(context->renderer, NULL);
}

void ex6_system_sprite9patch_render_camera(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		EX6_TransformScreen* transform = entity_get_data(id, EX6_TransformScreen);
		EX6_Sprite9Patch*    sprite = entity_get_data(id, EX6_Sprite9Patch);

		ex6_lib_sprite9patch_render_camera(context, sprite, transform);
	}
}

void ex6_system_imagebutton(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		EX6_TransformScreen* transform   = entity_get_data(id, EX6_TransformScreen);
		EX6_Sprite9Patch*    sprite      = entity_get_data(id, EX6_Sprite9Patch);
		EX6_ImageButton*     imagebutton = entity_get_data(id, EX6_ImageButton);

		SDL_FRect rect_dst;
		rect_dst.w = transform->scale.x * sprite->size.x;
		rect_dst.h = transform->scale.y * sprite->size.y;
		rect_dst.x = transform->position.x - sprite->pivot.x * rect_dst.w;
		rect_dst.y = transform->position.y - sprite->pivot.y * rect_dst.h;

		// // TMP debug btn area
		//sdl_set_render_draw_color(context, COLOR_YELLOW);
		//SDL_RenderRect(context->renderer, &rect_dst);

		TTF_DrawRendererText(imagebutton->ttf_text, rect_dst.x, rect_dst.y);

		vec2f mouse_camera_pos = point_window_to_screen(context, context->mouse_pos);

		if(SDL_PointInRectFloat((SDL_FPoint*)&mouse_camera_pos, &rect_dst))
		{
			sprite->tint = EX6_COLOR_BTN_HOVER;
			if(imagebutton->fn_callback_hover)
				imagebutton->fn_callback_hover(context, id);

			if(context->btn_isdown[BTN_TYPE_UI_SELECT])
				sprite->tint = EX6_COLOR_BTN_CLICK;

			if(context->btn_isjustpressed[BTN_TYPE_UI_SELECT] && imagebutton->fn_callback_click)
				imagebutton->fn_callback_click(context, id);
		}
		else
			sprite->tint = EX6_COLOR_BTN_DEFAULT;
	}
}

// ============================================================================================
// COMPONENT DEBUG UI RENDER methods
// ============================================================================================

void ex6_debug_ui_render_playerdata(SDLContext* context, void* data)
{
	EX6_PlayerData* data_player = (EX6_PlayerData*)data;

	ImGui::DragFloat("curr. linear speed", &data_player->curr_speed_linear);
	ImGui::DragFloat("curr. rotational speed", &data_player->curr_speed_rotational);

	itu_debug_ui_widget_entityid("target", data_player->target);
}

void ex6_debug_ui_render_health(SDLContext* context, void* data)
{
	EX6_Health* data_health = (EX6_Health*)data;

	ImGui::DragFloat("max", &data_health->max);
	ImGui::DragFloat("curr", &data_health->curr, 1, 0, data_health->max);
}

void ex6_debug_ui_render_healthrenderer(SDLContext* context, void* data)
{
	EX6_HealthRenderer* data_renderer = (EX6_HealthRenderer*)data;

	itu_debug_ui_widget_entityid("target", data_renderer->target);
	ImGui::DragFloat("base widget width", &data_renderer->widget_base_w);
}

void ex6_debug_ui_render_transformscreen(SDLContext* context, void* data)
{
	EX6_TransformScreen* data_transform = (EX6_TransformScreen*)data;

	ImGui::DragFloat2("position", &data_transform->position.x);
	ImGui::DragFloat2("scale", &data_transform->scale.x);

	float rotation_deg = data_transform->rotation * RAD_2_DEG;
	if(ImGui::DragFloat("rotation", &rotation_deg))
		data_transform->rotation = rotation_deg * DEG_2_RAD;

	itu_lib_render_draw_point(context->renderer, data_transform->position, 5, COLOR_YELLOW);
}

void ex6_debug_ui_render_sprite9patch(SDLContext* context, void* data)
{
	EX6_Sprite9Patch* data_sprite = (EX6_Sprite9Patch*)data;

	itu_sys_rstorage_debug_render_texture(data_sprite->texture, &data_sprite->texture, &data_sprite->rect);

	ImGui::DragFloat4("texture rect", &data_sprite->rect.x);
	ImGui::DragFloat2("pivot", &data_sprite->pivot.x);

	ImGui::DragFloat2("size", &data_sprite->size.x);
	ImGui::DragFloat2("margins ver.", &data_sprite->margins_hor.x);
	ImGui::DragFloat2("margins hor.", &data_sprite->margins_ver.x);

	ImGui::ColorEdit4("tint", &data_sprite->tint.r);
}


void ex6_debug_ui_render_imagebutton(SDLContext* context, void* data)
{
	EX6_ImageButton* data_imagebutton = (EX6_ImageButton*)data;
	//char* buf;
	//
	//TTF_SetTextString
	//ImGui::InputTextMultiline("text", buf, 1024);
	ImGui::LabelText("hover callback", "%p", data_imagebutton->fn_callback_hover);
	ImGui::LabelText("click callback", "%p", data_imagebutton->fn_callback_click);

	int wrap_width;
	TTF_GetTextWrapWidth(data_imagebutton->ttf_text, &wrap_width);

	int size[2];
	TTF_GetTextSize(data_imagebutton->ttf_text, &size[0], &size[1]);

	color c;
	TTF_GetTextColorFloat(data_imagebutton->ttf_text, &c.r, &c.g, &c.b, &c.a);

	TTF_Font* font = TTF_GetTextFont(data_imagebutton->ttf_text);
	TTF_Font* new_font;
	if(itu_sys_rstorage_debug_render_font(font, &new_font))
		TTF_SetTextFont(data_imagebutton->ttf_text, new_font);
	

	ImGui::InputInt2("size (readonly)", size);

	if(ImGui::DragInt("wrap width", &wrap_width))
		TTF_SetTextWrapWidth(data_imagebutton->ttf_text, wrap_width);

	if(ImGui::ColorEdit4("color", &c.r))
		TTF_SetTextColorFloat(data_imagebutton->ttf_text, c.r, c.g, c.b, c.a);
}

// ============================================================================================
// 
// ============================================================================================

// NOTE: we reached the GameState nirvana (all resources are handled by a dedicated system, more of a "game engine" approach)
struct GameState
{
	// // SDL-allocated structures
	// SDL_Texture* atlas_space;
	// SDL_Texture* ui_healtbar;
};

void ex6_system_assign_player_target(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	if(!itu_entity_is_valid(id_player))
		return;

	EX6_PlayerData* player_data = entity_get_data(id_player, EX6_PlayerData);
	Transform* player_transform = entity_get_data(id_player, Transform);
	vec2f player_pos = player_transform->position;

	ITU_EntityId id_closest = ITU_ENTITY_ID_NULL;
	float closest_distance_sq = FLOAT_MAX_VAL;

	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];

		Transform*   transform = entity_get_data(id, Transform);

		float curr_distance_sq = distance_sq(player_pos, transform->position);

		if(curr_distance_sq < closest_distance_sq)
		{
			id_closest = id;
			closest_distance_sq = curr_distance_sq;
		}
	}

	player_data->target = id_closest;
}

void ex6_system_player_update(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		Transform*      transform    = entity_get_data(id, Transform);
		EX6_PlayerData* data         = entity_get_data(id, EX6_PlayerData);
		PhysicsData*    physics_data = entity_get_data(id, PhysicsData);

		vec2f dir = VEC2F_ZERO;
		if(context->btn_isdown[BTN_TYPE_UP])
			dir.y += 1;
		if(context->btn_isdown[BTN_TYPE_DOWN])
			dir.y -= 1;
		if(context->btn_isdown[BTN_TYPE_LEFT])
			dir.x -= 1;
		if(context->btn_isdown[BTN_TYPE_RIGHT])
			dir.x += 1;

		physics_data->velocity = normalize(dir) * 5;

		float target_rotation = 0.0f;
		if(itu_entity_is_valid(data->target))
		{
			Transform* target_transform = entity_get_data(data->target, Transform);
			vec2f lookat = normalize(target_transform->position - transform->position);
			target_rotation = SDL_atan2f(lookat.y, lookat.x) - PI_HALF;
		}
		// asymptotic approach
		transform->rotation = lerp(transform->rotation, target_rotation, 0.15f);
	}
}

void ex6_system_health(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		EX6_HealthRenderer* renderer = entity_get_data(id, EX6_HealthRenderer);

		if(!itu_entity_is_valid(renderer->target))
			continue;

		EX6_Sprite9Patch* sprite = entity_get_data(id, EX6_Sprite9Patch);
		EX6_Health* health = entity_get_data(renderer->target, EX6_Health);

		if(context->btn_isjustpressed[BTN_TYPE_SPACE])
			health->curr = SDL_clamp(health->curr - health->max / 10, 0, 100);

		sprite->size.x = renderer->widget_base_w * (health->curr / health->max);
	}
}

static void game_init(SDLContext* context, GameState* state)
{
	itu_sys_rstorage_texture_load(context, "data/kenney/simpleSpace_tilesheet_2.png", SDL_SCALEMODE_LINEAR);
	itu_sys_rstorage_texture_load(context, "data/kenney/UI/bar_round_gloss_small_red.png", SDL_SCALEMODE_LINEAR);
	itu_sys_rstorage_texture_load(context, "data/kenney/UI/panel_square.png", SDL_SCALEMODE_LINEAR);
	itu_sys_rstorage_font_load(context, "data/ARIAL.TTF", 42);
	itu_sys_rstorage_font_load(context, "data/ARIALI.TTF", 42);
	itu_sys_rstorage_font_load(context, "data/ARIALBD.TTF", 42);

	ttf_engine = TTF_CreateRendererTextEngine(context->renderer);

	itu_sys_estorage_init(512);
	itu_sys_physics_init(context);

	enable_component(EX6_PlayerData);
	enable_component(EX6_Health);
	enable_component(EX6_HealthRenderer);
	enable_component(EX6_TransformScreen);
	enable_component(EX6_Sprite9Patch);
	enable_component(EX6_ImageButton);

	add_component_debug_ui_render(EX6_PlayerData, ex6_debug_ui_render_playerdata);
	add_component_debug_ui_render(EX6_Health, ex6_debug_ui_render_health);
	add_component_debug_ui_render(EX6_HealthRenderer, ex6_debug_ui_render_healthrenderer);
	add_component_debug_ui_render(EX6_TransformScreen, ex6_debug_ui_render_transformscreen);
	add_component_debug_ui_render(EX6_Sprite9Patch, ex6_debug_ui_render_sprite9patch);
	add_component_debug_ui_render(EX6_ImageButton, ex6_debug_ui_render_imagebutton);

	itu_sys_estorage_tag_set_debug_name(TAG_CAMERA_TARGET, "camera target");
	itu_sys_estorage_tag_set_debug_name(TAG_ASTEROID, "asteroid");
	
	add_system(ex6_system_assign_player_target      , component_mask(Transform), tag_mask(TAG_ASTEROID));
	add_system(ex6_system_player_update             , component_mask(Transform) | component_mask(PhysicsData) | component_mask(EX6_PlayerData)  , 0);
	add_system(ex6_system_health                    , component_mask(EX6_HealthRenderer)  | component_mask(EX6_Sprite9Patch), 0);
	add_system(ex6_system_sprite_render_camera      , component_mask(EX6_TransformScreen) | component_mask(Sprite)          , 0);
	add_system(ex6_system_sprite9patch_render_camera, component_mask(EX6_TransformScreen) | component_mask(EX6_Sprite9Patch), 0);
	add_system(ex6_system_imagebutton               , component_mask(EX6_TransformScreen) | component_mask(EX6_Sprite9Patch) | component_mask(EX6_ImageButton) , 0);
	add_system(ex6_system_camera_target             , component_mask(Transform), tag_mask(TAG_CAMERA_TARGET));
}

void TMP_btn_callback_hover(SDLContext* context, ITU_EntityId id) { SDL_Log(""); }
void TMP_btn_callback_click(SDLContext* context, ITU_EntityId id) { SDL_Log("click"); }

static void game_reset(SDLContext* context, GameState* state)
{
	// TMP get textures pointers
	//     these should come from a serialized file
	SDL_Texture* tex_space     = itu_sys_rstorage_texture_get_ptr(0);
	SDL_Texture* tex_healthbar = itu_sys_rstorage_texture_get_ptr(1);
	SDL_Texture* tex_button    = itu_sys_rstorage_texture_get_ptr(2);
	TTF_Font*    font_bold     = itu_sys_rstorage_font_get_ptr(2);

	itu_sys_estorage_clear_all_entities();

	b2WorldDef world_def = b2DefaultWorldDef();
	world_def.gravity.y = 0;
	itu_sys_physics_reset(&world_def);

	SDL_assert(ENTITY_COUNT <= ENTITIES_COUNT_MAX);

	b2BodyDef body_def = b2DefaultBodyDef();
	b2ShapeDef shape_def = b2DefaultShapeDef();
	b2Circle circle = { 0 };
	circle.radius = 0.25f;

	// player
	{
		id_player = itu_entity_create();
		itu_entity_set_debug_name(id_player, "player");
		Transform transform = TRANSFORM_DEFAULT;
		transform.position.y = -7;

		Sprite sprite;
		itu_lib_sprite_init(&sprite, tex_space, itu_lib_sprite_get_rect(0, 1, 128, 128));

		EX6_PlayerData data = { 0 };

		// FIXME this is thrash
		PhysicsData physics_data = { 0 };
		physics_data.ignore_rotation = true;
		body_def.position = value_cast(b2Vec2, transform.position);
		body_def.type = b2_dynamicBody;
		physics_data.body_id = itu_sys_physics_add_body(value_cast(void*, id_player), &body_def);
		
		ShapeData shape_data;
		shape_data.shape_id = b2CreateCircleShape(physics_data.body_id, &shape_def, &circle);

		
		EX6_Health health;
		health.max = 100;
		health.curr = 100;


		entity_add_component(id_player, Transform     , transform);
		entity_add_component(id_player, Sprite        , sprite);
		entity_add_component(id_player, EX6_PlayerData, data);
		entity_add_component(id_player, PhysicsData   , physics_data);
		entity_add_component(id_player, ShapeData     , shape_data);
		entity_add_component(id_player, EX6_Health    , health);
		itu_entity_tag_add(id_player, TAG_CAMERA_TARGET);
	}

	// entities
	for(int i = 0; i < ENTITY_COUNT; ++i)
	{
		ITU_EntityId id = itu_entity_create();
		char name_buf[16];
		SDL_snprintf(name_buf, 16, "asteroid_%d", i);
		itu_entity_set_debug_name(id, name_buf);

		Transform transform = { 0 };
		Sprite sprite;

		transform.scale = VEC2F_ONE;
		transform.position.x = SDL_randf() * 16 - 8;
		transform.position.y = SDL_randf() * 16 - 8;

		itu_lib_sprite_init(&sprite, tex_space, itu_lib_sprite_get_rect(0, 4, 128, 128));

		// FIXME this is thrash
		PhysicsStaticData physics_data = { 0 };
		body_def.position = value_cast(b2Vec2, transform.position);
		body_def.type = b2_staticBody;
		physics_data.body_id = itu_sys_physics_add_body(value_cast(void*, id), &body_def);

		
		ShapeData shape_data;
		shape_data.shape_id = b2CreateCircleShape(physics_data.body_id, &shape_def, &circle);

		entity_add_component(id, Transform,   transform);
		entity_add_component(id, Sprite,      sprite);
		entity_add_component(id, PhysicsStaticData, physics_data);
		entity_add_component(id, ShapeData, shape_data);
		itu_entity_tag_add(id, TAG_ASTEROID);

	}

	// healtbar
	{
		ITU_EntityId id = itu_entity_create();
		itu_entity_set_debug_name(id, "UI-healtbar");
		EX6_TransformScreen transform = { 0 };
		transform.scale = VEC2F_ONE;
		transform.position = { 20, 18 };

		EX6_Sprite9Patch   sprite;
		sprite.rect = { 0, 0, 96, 16 };
		sprite.texture = tex_healthbar;
		sprite.size = { 760, 16 };
		sprite.margins_hor = { 8, 8 };
		sprite.margins_ver = { 8, 8 };
		sprite.pivot.x = 0.0f;
		sprite.pivot.y = 0.0f;
		sprite.tint = COLOR_WHITE;

		EX6_HealthRenderer renderer;
		renderer.target = id_player;
		renderer.widget_base_w = sprite.size.x;

		entity_add_component(id, EX6_TransformScreen, transform);
		entity_add_component(id, EX6_Sprite9Patch, sprite);
		entity_add_component(id, EX6_HealthRenderer, renderer);
	}

	// button
	{
		ITU_EntityId id = itu_entity_create();
		itu_entity_set_debug_name(id, "UI-Button");
		EX6_TransformScreen transform = { 0 };
		transform.scale = VEC2F_ONE;
		transform.position = { 20, context->window_h - 18 };

		EX6_Sprite9Patch sprite;
		sprite.rect = { 0, 0, 64, 64 };
		sprite.texture = tex_button;
		sprite.size = { 280, 48 };
		sprite.margins_hor = { 8, 8 };
		sprite.margins_ver = { 8, 8 };
		sprite.pivot.x = 0.0f;
		sprite.pivot.y = 1.0f;
		sprite.tint = COLOR_WHITE;

		const char btn_text[] = "I am a button!";
		EX6_ImageButton imagebutton = { 0 };
		imagebutton.fn_callback_click = TMP_btn_callback_click;
		imagebutton.ttf_text = TTF_CreateText(ttf_engine, font_bold, btn_text, SDL_strlen(btn_text));

		entity_add_component(id, EX6_TransformScreen, transform);
		entity_add_component(id, EX6_Sprite9Patch, sprite);
		entity_add_component(id, EX6_ImageButton, imagebutton);
	}
}

int main(void)
{
	bool quit = false;
	SDLContext context = { 0 };
	GameState  state   = { };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	TTF_Init();

	context.working_dir = SDL_GetCurrentDirectory();
	context.window = SDL_CreateWindow("ES06 - UI", WINDOW_W, WINDOW_H, 0);
	context.renderer = SDL_CreateRenderer(context.window, "vulkan");
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	
	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		context.zoom = 1;
		context.window_w /= context.zoom;
		context.window_h /= context.zoom;
		SDL_SetRenderScale(context.renderer, context.zoom, context.zoom);
	}
	
	itu_lib_imgui_setup(context.window, &context, true);

	context.camera_default.normalized_screen_size.x = 0.5f;
	context.camera_default.normalized_screen_size.y = 1.0f;
	context.camera_default.normalized_screen_offset.x = 0.5f;
	context.camera_default.zoom = 1;
	context.camera_default.pixels_per_unit = CAMERA_PIXELS_PER_UNIT;
	camera_set_active(&context, &context.camera_default);

	// set degu UI shown by default (new and shiny, let's showcase it)
	context.debug_ui_show = true;

	game_init(&context, &state);
	game_reset(&context, &state);

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_frame_end;
	SDL_Time walltime_work_end;
	SDL_Time elapsed_work = 0;
	SDL_Time elapsed_frame = 0;

	SDL_GetCurrentTime(&walltime_frame_beg);
	walltime_frame_end = walltime_frame_beg;

	sdl_input_set_mapping_keyboard(&context, SDLK_W,     BTN_TYPE_UP);
	sdl_input_set_mapping_keyboard(&context, SDLK_A,     BTN_TYPE_LEFT);
	sdl_input_set_mapping_keyboard(&context, SDLK_S,     BTN_TYPE_DOWN);
	sdl_input_set_mapping_keyboard(&context, SDLK_D,     BTN_TYPE_RIGHT);
	sdl_input_set_mapping_keyboard(&context, SDLK_Q,     BTN_TYPE_ACTION_0);
	sdl_input_set_mapping_keyboard(&context, SDLK_E,     BTN_TYPE_ACTION_1);
	sdl_input_set_mapping_keyboard(&context, SDLK_SPACE, BTN_TYPE_SPACE);

	sdl_input_set_mapping_mouse(&context, 1, BTN_TYPE_UI_SELECT);
	sdl_input_set_mapping_mouse(&context, 3, BTN_TYPE_UI_EXTRA);

	while(!quit)
	{
		quit = sdl_process_events(&context);

		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);
		
		itu_lib_imgui_frame_begin();

		// update
		itu_sys_estorage_systems_update(&context);
#ifdef ENABLE_DIAGNOSTICS
		{
			//ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4 { 33/255.0f, 33/255.0f, 33/255.0f, 255/255.0f });
			if(context.debug_ui_show)
			{
				if(ImGui::Begin("Debug UI", &context.debug_ui_show, ImGuiWindowFlags_NoCollapse))
				{
					ImGui::BeginTabBar("debug_ui_tab");
					if(ImGui::BeginTabItem("Context"))
					{
						//ImGui::Begin("itu_diagnostics");
						ImGui::Text("Timing");
						ImGui::LabelText("work", "%6.3f ms/f", (float)elapsed_work  / (float)MILLIS(1));
						ImGui::LabelText("tot",  "%6.3f ms/f", (float)elapsed_frame / (float)MILLIS(1));
						ImGui::LabelText("physics steps",  "%d", context.physics_steps_count);

						ImGui::EndTabItem();
					}
					if(ImGui::BeginTabItem("Entities"))
					{
						itu_sys_estorage_debug_render(&context);
						ImGui::EndTabItem();
					}
					if(ImGui::BeginTabItem("Resources"))
					{
						itu_sys_rstorage_debug_render(&context);
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
				ImGui::End();
			}
		}
#endif

		itu_lib_imgui_frame_end(&context);

		SDL_GetCurrentTime(&walltime_work_end);
		elapsed_work = walltime_work_end - walltime_frame_beg;

		if(elapsed_work < TARGET_FRAMERATE_NS)
			SDL_DelayNS(TARGET_FRAMERATE_NS - elapsed_work);

		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;

		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		context.elapsed_frame = elapsed_frame;
		walltime_frame_beg = walltime_frame_end;
	}
}
