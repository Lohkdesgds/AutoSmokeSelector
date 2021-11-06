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

color process_image(texture& txtur2, const config& conf)
{
	if (txtur2.empty()) return color(16, 16, 16); // clean almost black error-like color

	const auto __backpu = al_get_new_bitmap_flags();
	al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
	texture txtur = txtur2.duplicate();
	al_set_new_bitmap_flags(__backpu);	

	if (txtur.empty()) return color(16, 16, 16); // clean almost black error-like color

	ALLEGRO_LOCKED_REGION* region = al_lock_bitmap(txtur.get_raw_bitmap(), ALLEGRO_PIXEL_FORMAT_RGB_888, ALLEGRO_LOCK_READONLY);
	if (!region) {
		cout << console::color::RED << "[CLIENT] Can't open image as RGB888.";
		return color(16, 16, 16);
	}
	else {
		struct __rgb {
			unsigned char b, g, r;
		};

		__rgb* const beg = (__rgb*)region->data;
		if (region->pixel_size != sizeof(__rgb)) {
			cout << console::color::RED << "[CLIENT] Something is wrong.";
			al_unlock_bitmap(txtur.get_raw_bitmap());
			return color(16, 16, 16);
		}

		const auto pitch_width = (region->pitch < 0 ? -region->pitch : region->pitch) / region->pixel_size;

		unsigned long long red_depth = 0, green_depth = 0, blue_depth = 0; // count
		unsigned long long brightness_center = 0; // brightness of center

		cout << console::color::AQUA << "[CLIENT] Processing image...";

		const size_t min_x = 0.3 * txtur.get_width();
		const size_t max_x = 0.7 * txtur.get_width();
		const size_t min_y = 0.3 * txtur.get_height();
		const size_t max_y = 0.7 * txtur.get_height();

		size_t amount = 0;
		size_t amount_brightness = 0;

		for (size_t y = 0; y < txtur.get_height(); y++) {
			for (size_t x = 0; x < txtur.get_width() && x < pitch_width; x++) {

				if ((x > min_x && x < max_x) && (y > min_y && y < max_y)) {

					unsigned char max_val;

					max_val = beg[y * pitch_width + x].r;
					if (beg[y * pitch_width + x].g > max_val) max_val = beg[y * pitch_width + x].g;
					if (beg[y * pitch_width + x].b > max_val) max_val = beg[y * pitch_width + x].b;

					//min_val = beg[y * pitch_width + x].r;
					//if (beg[y * pitch_width + x].g < min_val) min_val = beg[y * pitch_width + x].g;
					//if (beg[y * pitch_width + x].b < min_val) min_val = beg[y * pitch_width + x].b;

					brightness_center += max_val; // static_cast<unsigned char>(0.5f * (max_val + min_val));


					red_depth += beg[y * pitch_width + x].r;
					green_depth += beg[y * pitch_width + x].g;
					blue_depth += beg[y * pitch_width + x].b;

					++amount_brightness;
					++amount;
				}
				else if ((x % 3) == 0 && (y % 3) == 0) { // per 9 do 1
					red_depth += beg[y * pitch_width + x].r;
					green_depth += beg[y * pitch_width + x].g;
					blue_depth += beg[y * pitch_width + x].b;
					++amount;
				}

			}
		}

		brightness_center /= amount_brightness;

		float resred = 0.0f, resgreen = 0.0f, resblue = 0.0f;
		const double propp			= conf.get_as<double>("processing", "saturation_compensation");		// how much sat compensation (default: 0.45)
		const double corr_to_high	= conf.get_as<double>("processing", "brightness_compensation");		// how much compensation to bright (default: 0.10 (10% increase))
		const double overflow_corr  = conf.get_as<double>("processing", "overflow_boost");				// 1.0f = no overflow, less = less brightness and color (default: 1.05)

		{
			auto lesser = red_depth;
			if (green_depth < lesser) lesser = green_depth;
			if (blue_depth < lesser) lesser = blue_depth;
			if (lesser == 0) lesser = 1;

			auto highest = red_depth + 1;
			if (green_depth > highest) highest = green_depth;
			if (blue_depth > highest) highest = blue_depth;

			double prop_bright = brightness_center * 1.0 / 0xFF;

			resred = static_cast<float>(((((red_depth - lesser + 1) * 1.0 / (highest - lesser + 1)) * prop_bright) * propp + ((red_depth * 1.0 / highest) * prop_bright) * (1.0 - propp))); // get highest of all of them then adjust to center brightness
			resgreen = static_cast<float>(((((green_depth - lesser + 1) * 1.0 / (highest - lesser + 1)) * prop_bright) * propp + ((green_depth * 1.0 / highest) * prop_bright) * (1.0 - propp))); // get highest of all of them then adjust to center brightness
			resblue = static_cast<float>(((((blue_depth - lesser + 1) * 1.0 / (highest - lesser + 1)) * prop_bright) * propp + ((blue_depth * 1.0 / highest) * prop_bright) * (1.0 - propp))); // get highest of all of them then adjust to center brightness

			//auto highflot = resred;
			//if (resgreen > highflot) highflot = resgreen;
			//if (resblue > highflot) highflot = resblue;

			resred += corr_to_high * (overflow_corr - resred);
			resgreen += corr_to_high * (overflow_corr - resgreen);
			resblue += corr_to_high * (overflow_corr - resblue);

			resred = resred > 1.0f ? 1.0f : resred;
			resgreen = resgreen > 1.0f ? 1.0f : resgreen;
			resblue = resblue > 1.0f ? 1.0f : resblue;
		}

		al_unlock_bitmap(txtur.get_raw_bitmap());
		return color(resred, resgreen, resblue);//static_cast<unsigned char>(red_depth), static_cast<unsigned char>(green_depth), static_cast<unsigned char>(blue_depth));
	}

	return color(16, 16, 16); // should never go here lol
}