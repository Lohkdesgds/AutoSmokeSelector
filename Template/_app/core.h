#pragma once

#include <Lunaris/all.h>
#include "../resource.h"
#include "data_handling.h"
#include "../../_shared/package_definition.h"

//#define CONNECTION_VERBOSE
#define MYASST2(X, MSGGG) if (!(X)) {Lunaris::cout << Lunaris::console::color::RED << MSGGG; Lunaris::cout << Lunaris::console::color::RED << "AUTOMATIC ABORT BEGUN"; client.close_socket(); return; }

using namespace Lunaris;

const std::initializer_list<multi_pair<hybrid_memory<texture>, textures_enum>> default_textures = {
	{{}, textures_enum::MOUSE},
	{{}, textures_enum::LOADING},
	{{}, textures_enum::BUTTON_UP},
	{{}, textures_enum::BUTTON_DOWN},
};

class CoreWorker {
	struct _shared {
		std::atomic<stage_enum> screen = stage_enum::HOME; // OK
		hybrid_memory<file> file_font; // MEMFILE! // OK
		hybrid_memory<file> file_atlas; // MEMFILE! // OK
		fixed_multi_map_work<static_cast<size_t>(textures_enum::_SIZE), hybrid_memory<texture>, textures_enum> texture_map = default_textures; // OK
		sprite_pair_screen_vector casted_boys = get_default_sprite_map();

		std::atomic_bool kill_all = false;
		std::atomic_int kill_tries = 0;
		float generic_progressbar = 0.0f;
		std::atomic_bool progressbar_mode = false;

		mouse mouse_work;
		sprite_pair mouse_pair; // has block in it

		hybrid_memory<file> latest_esp32_file; // set after processing
		hybrid_memory<texture> latest_esp32_texture; // set after processing
		color background_color = color(16, 16, 16);

		_shared(std::function<ALLEGRO_TRANSFORM(void)>);

		memfile* get_file_font();
		memfile* get_file_atlas();
	};
	struct _display {
		display_async disp; // START // OK
		hybrid_memory<texture> src_atlas; // OK
		hybrid_memory<font> src_font; // OK
	};
	struct _esp32_communication {
		enum class package_status {NON_CONNECTED, IDLE, PROCESSING_IMAGE};

		thread commu; // this will keep the connection or keep searching for one (HANDLES: current, hosting, MUST KILL THEM ITSELF)
		thread procc; // this thread will handle the packages async / rule the thing // DOES NOT KILL CONNECTION, UNLESS SOME KIND OF FAIL

		struct host_stuff {
			TCP_client current; // keep talking indefinitely
			TCP_host hosting; // listen till 1. If current has socket, decline new connections
		};
		std::unique_ptr<host_stuff> con;

		package_status status = package_status::NON_CONNECTED; // to display

		safe_vector<esp_package> packages_to_send; // commu has to send this ones.
		hybrid_memory<file> package_combine_file; // for image load later on.

		void close_all();
	};

	_shared  shrd;
	_display dspy; // OK
	_esp32_communication espp;

	bool get_is_loading();

	void hook_display_load(); // simple bar, no resource needed
	void hook_display_default(); // default enum scene thing
	bool start_esp32_threads(); // hook lambdas and make it work!

	bool full_load(); // create display, hook basic idependent stuff
	
	void post_progress_val(const float); // 0..1, set progress val
	void _post_update_display(); // when resize event is detected it's possible to just call this
	void _post_update_display_one_o_one(); // 1:1 pixel transform
	
	void handle_display_event(const ALLEGRO_EVENT&); // handle explicit display event
	void handle_mouse_event(const int, const mouse::mouse_event&); // this is also the collision one because of the mouse click -> collision test optimization
	
	void full_close();


	//stage_enum screen_now = stage_enum::LOADING;
	//display_async display;
	//mouse mouseev;
	//keys shortcuts;
	//
	//memfile atlas_file, font_file;
	//hybrid_memory<texture> atlas_texture;
	//hybrid_memory<font> font_font;
	//thread thinking_thread;
	//
	//block mouse_spr;
	//std::unordered_map<std::string, hybrid_memory<texture>> texture_map;
	//
	//sprite_pair_screen_vector working_map;
	//
	//bool kill_display = false;
	//
	//std::unique_ptr<keyboard> kbev; // if needed, it's created, else ignore keyboard

	//bool __full_load_async();
	//
	//void setup_sprites();
	//
	//bool draw();
	//void think();
	////void communicate(); // host
	//
	//// local (this thread, no queue)?
	//void set_display_refresh_canvas(const bool = false);
	//
	//void handle_mouse(const int, const mouse::mouse_event&);
	//void handle_key(const keys::key_event&);
public:
	CoreWorker();

	bool work_and_yield();
};