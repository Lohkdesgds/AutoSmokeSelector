#include "data_handling.h"

sprite_pair::sprite_pair(hybrid_memory<sprite>&& spr, std::function<void(sprite*)> tk, std::function<void(sprite*, const cond&)> al)
	: sprite_ref(std::move(spr)), all_ev(al), ticking(tk)
{
	if (!all_ev) throw std::runtime_error("Invalid empty function all_ev @ sprite_pair constructor");
	if (!ticking) throw std::runtime_error("Invalid empty function ticking @ sprite_pair constructor");
	if (!sprite_ref.get()) throw std::runtime_error("Invalid empty sprite @ sprite_pair constructor");
}

sprite_pair::sprite_pair(hybrid_memory<sprite>&& spr)
	: sprite_ref(std::move(spr))
{
	all_ev = [](auto,auto) {};
	ticking = [](auto) {};
	if (!sprite_ref.get()) throw std::runtime_error("Invalid empty sprite @ sprite_pair constructor");
}

void sprite_pair::set_func(std::function<void(sprite*, const cond&)> fc)
{
	if (!fc) all_ev = [](auto,auto) {}; // empty, but not empty
	else all_ev = fc;
}

void sprite_pair::set_tick(std::function<void(sprite*)> fe)
{
	if (!fe) ticking = [](auto) {}; // empty, but not empty
	else ticking = fe;
}
void sprite_pair::update(const cond& about)
{
	all_ev(sprite_ref.get(), about);
}

sprite* sprite_pair::get_sprite()
{
	ticking(sprite_ref.get());
	return sprite_ref.get();
}

sprite_pair_screen_vector get_default_sprite_map()
{
	sprite_pair_screen_vector mapp;
	for (size_t p = 0; p < static_cast<size_t>(stage_enum::_SIZE); p++) {
		mapp[static_cast<stage_enum>(p)] = {}; // construct
	}
	return mapp;
}
