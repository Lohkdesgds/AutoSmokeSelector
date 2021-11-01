#pragma once

#include <Lunaris/all.h>

using namespace Lunaris;

enum class stage_enum {HOME, OPTIONS, _SIZE};
enum class textures_enum{MOUSE, LOADING, BUTTON_UP, BUTTON_DOWN, _SIZE};

class sprite_pair {
public:
	struct cond {
		bool is_mouse_on_it = false;
		bool is_mouse_pressed = false;
		bool is_click = false;
		bool is_unclick = false;
	};
private:
	hybrid_memory<sprite> sprite_ref;
	std::function<void(sprite*, const cond&)> all_ev;
	std::function<void(sprite*)> ticking;
public:
	sprite_pair(hybrid_memory<sprite>&&, std::function<void(sprite*)>, std::function<void(sprite*, const cond&)>);
	sprite_pair(hybrid_memory<sprite>&&);

	void set_func(std::function<void(sprite*, const cond&)>);
	void set_tick(std::function<void(sprite*)>);

	void update(const cond&);

	sprite* get_sprite();
};

using sprite_pair_screen_vector = std::unordered_map<stage_enum, safe_vector<sprite_pair>>;

sprite_pair_screen_vector get_default_sprite_map();