#pragma once

#include <Lunaris/all.h>
#include "../resource.h"
#include "data_handling.h"
#include "../../_shared/package_definition.h"

#include <allegro5/allegro_native_dialog.h>

//#define CONNECTION_VERBOSE
#define MYASST2(X, MSGGG) if (!(X)) {Lunaris::cout << Lunaris::console::color::RED << MSGGG; Lunaris::cout << Lunaris::console::color::RED << "AUTOMATIC ABORT BEGUN"; client.close_socket(); return; }

const size_t max_async_queue_size = 3;

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

class CoreWorker {
	struct _shared {
		std::atomic<stage_enum> screen = stage_enum::HOME; // OK

		safe_vector<std::function<void(void)>> task_queue;
		thread async_queue;

		std::unordered_map<stage_enum, stage_set> screen_set; // rules for screen
		float current_offy = 0.0f; // offset right now

		config conf; // general configuration for this app

		hybrid_memory<file> file_font; // MEMFILE! // OK
		hybrid_memory<file> file_atlas; // MEMFILE! // OK
		fixed_multi_map_work<static_cast<size_t>(textures_enum::_SIZE), hybrid_memory<texture>, textures_enum> texture_map = default_textures; // OK
		sprite_pair_screen_vector casted_boys = get_default_sprite_map();

		color bad_plant, good_plant;
		float bad_perc = 0.0f, good_perc = 0.0f;

		std::atomic_bool kill_all = false;
		std::atomic_int kill_tries = 0;
		float generic_progressbar = 0.0f;
		std::atomic_bool progressbar_mode = false;

		mouse mouse_work;
		sprite_pair mouse_pair; // has block in it
		block wifi_blk; // has block in it

		hybrid_memory<file> latest_esp32_file; // set after processing
		hybrid_memory<texture> latest_esp32_texture; // set after processing
		hybrid_memory<texture> latest_esp32_texture_orig; // memory bitmap in memory already baby
		color background_color = color(16, 16, 16);

		_shared(std::function<ALLEGRO_TRANSFORM(void)>);

		void __async_queue();

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
	void auto_handle_process_image(const bool = true);
	// task, wait if number of tasks is bigger than
	void async_task(std::function<void(void)>, const size_t = max_async_queue_size);
	void auto_update_wifi_icon(const textures_enum);

	void hook_display_load(); // simple bar, no resource needed
	void hook_display_default(); // default enum scene thing
	bool start_esp32_threads(); // hook lambdas and make it work!

	bool full_load(); // create display, hook basic idependent stuff
	
	void post_progress_val(const float); // 0..1, set progress val
	void _post_update_display(); // when resize event is detected it's possible to just call this
	void _post_update_display_one_o_one(); // 1:1 pixel transform
	void post_update_display_auto();
	
	void handle_display_event(const ALLEGRO_EVENT&); // handle explicit display event
	void handle_mouse_event(const int, const mouse::mouse_event&); // this is also the collision one because of the mouse click -> collision test optimization
	
	// if string, warn is shown with this
	void full_close(const std::string& = "");
public:
	CoreWorker();
	bool work_and_yield();
};