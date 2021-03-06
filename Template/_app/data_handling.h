#pragma once

#include <Lunaris/all.h>

using namespace Lunaris;

enum class stage_enum {HOME, HOME_OPEN, CONFIG, CONFIG_FIX, PREVIEW, _SIZE};
enum class textures_enum{
	MOUSE,			// Common mouse
	MOUSE_LOADING,	// Mouse when loading, circle loading
	BUTTON_UP,		// Bar, when not pressed
	BUTTON_DOWN,	// Bar, when pressed
	// related to esp32 on core
	WIFI_SEARCHING,	// WiFi module is not connected
	WIFI_IDLE,		// WiFi module is not working right now
	WIFI_TRANSFER,	// WiFi module is downloading an image
	WIFI_THINKING,	// WiFi module is thinking about colors
	WIFI_COMMAND,	// WiFi module is sending a command
	// end of related to esp32 on core
	_SIZE,
	// ASSIST ONES
	__FIRST_WIFI = WIFI_SEARCHING,
	__LAST_WIFI = WIFI_COMMAND
};

template<typename T>
inline T limit_range_of(const T& val, const T& min, const T& max) { return val > max ? max : (val < min ? min : val); }

struct stage_set {
	float min_y = 0.0f;
	float max_y = 0.0f;
};

const double last_call_considered_off = 0.2; // after 1 second no draw next get_sprite resets its params using on_reset

class sprite_pair {
public:
	struct cond {
		bool is_mouse_on_it = false;
		bool is_mouse_pressed = false;
		bool is_click = false;
		bool is_unclick = false;
		mouse::mouse_event cpy;
	};
private:
	hybrid_memory<sprite> sprite_ref;
	std::function<void(sprite*, const cond&)> all_ev;
	std::function<void(sprite*)> ticking;
	std::function<void(sprite*)> on_reset;
	double last_call = -0.1 - last_call_considered_off;
	stage_enum stg = stage_enum::_SIZE;
public:
	sprite_pair(hybrid_memory<sprite>&&, std::function<void(sprite*)>, std::function<void(sprite*, const cond&)>, const std::function<void(sprite*)> = [](auto) {}, const stage_enum& = stage_enum::_SIZE);
	sprite_pair(hybrid_memory<sprite>&&);

	void set_func(std::function<void(sprite*, const cond&)>);
	void set_tick(std::function<void(sprite*)>);
	void set_on_reset(std::function<void(sprite*)>);

	void update(const cond&);

	void lock_work(const stage_enum&);
	bool does_work_on(const stage_enum&) const;

	sprite* get_think();
	sprite* get_notick();
};

using sprite_pair_screen_vector = std::unordered_map<stage_enum, safe_vector<hybrid_memory<sprite_pair>>>;

sprite_pair_screen_vector get_default_sprite_map();

color process_image(texture&, const config&);

color config_to_color(const config&, const std::string&, const std::string&);
void color_to_config(config&, const std::string&, const std::string&, const color&);

float how_far(const color&, const color&);

std::string get_default_config_path();

void ensure_default_conf(config&);