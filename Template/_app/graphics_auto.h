#pragma once

#include <Lunaris/all.h>
#include "data_handling.h"
#include "storage.h"
#include "asynchronous.h"
#include "communication.h"
#include "../resource.h"

using namespace Lunaris;

const std::initializer_list<multi_pair<hybrid_memory<texture>, textures_enum>> default_textures = {
	{{}, textures_enum::MOUSE},
	{{}, textures_enum::MOUSE_LOADING},
	{{}, textures_enum::BUTTON_UP},
	{{}, textures_enum::BUTTON_DOWN},
	{{}, textures_enum::WIFI_SEARCHING},
	{{}, textures_enum::WIFI_IDLE},
	{{}, textures_enum::WIFI_TRANSFER},
	{{}, textures_enum::WIFI_THINKING},
	{{}, textures_enum::WIFI_COMMAND},
};

const float text_updates_per_sec = 0.0f;

class GraphicInterface : public NonCopyable, public NonMovable {
	display_async disp;
	display_event_handler dispev;
	thread update_pos;
	Storage& stor;
	Communication& comm;
	AsyncSinglePoll async;

	hybrid_memory<texture> src_atlas;
	hybrid_memory<font> src_font;
	hybrid_memory<texture> latest_esp32_texture; // set after processing

	mouse mouse_work;
	sprite_pair mouse_pair; // has block in it
	sprite_pair wifi_blk; // has block in it

	std::atomic<stage_enum> screen = stage_enum::HOME; // OK
	fixed_multi_map_work<static_cast<size_t>(textures_enum::_SIZE), hybrid_memory<texture>, textures_enum> texture_map = default_textures; // OK
	sprite_pair_screen_vector casted_boys = get_default_sprite_map();
	std::unordered_map<stage_enum, stage_set> screen_set; // rules for screen
	std::atomic_bool update_display = true;
	float current_offy = 0.0f;

	safe_data<std::function<void(void)>> on_quit_f;

	void draw();

	// handlers
	void handle_display_event(const display_event&);
	void handle_mouse_event(const int, const mouse::mouse_event&);

	// event
	void auto_handle_process_image(const bool = true);
public:
	GraphicInterface(Storage&, Communication&);

	bool start(std::function<void(void)>);
	void stop();

};