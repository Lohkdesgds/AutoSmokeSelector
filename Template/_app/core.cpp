#include "core.h"

CoreWorker::_shared::_shared(std::function<ALLEGRO_TRANSFORM(void)> f)
	: mouse_work(f), mouse_pair(make_hybrid_derived<sprite, block>())
{
}

void CoreWorker::_shared::__async_queue()
{
	if (task_queue.size()) {
		if (auto& it = task_queue.index(0); it) it();
		task_queue.erase(0);
	}
}

memfile* CoreWorker::_shared::get_file_font()
{
	return (memfile*)file_font.get();
}

memfile* CoreWorker::_shared::get_file_atlas()
{
	return (memfile*)file_atlas.get();
}

void CoreWorker::_esp32_communication::close_all()
{
	package_combine_file.reset_this();
	packages_to_send.clear();

	con.reset();
}

bool CoreWorker::get_is_loading()
{
	//return espp.con ? !espp.con->current.has_socket() : true;
	return shrd.task_queue.size() > 0;
}

void CoreWorker::auto_handle_process_image(const bool asyncr)
{
	if (shrd.latest_esp32_texture_orig.empty()) return;

	const auto ff = [&] {
		shrd.background_color = process_image(*shrd.latest_esp32_texture_orig, shrd.conf);
		shrd.bad_perc = how_far(shrd.bad_plant, shrd.background_color);
		shrd.good_perc = how_far(shrd.good_plant, shrd.background_color);
	};

	if (asyncr) async_task(ff, 0);
	else ff();
}

void CoreWorker::async_task(std::function<void(void)> f, const size_t max_waiting_tasks)
{
	while (shrd.task_queue.size() > max_waiting_tasks) std::this_thread::sleep_for(std::chrono::milliseconds(50));
	shrd.task_queue.push_back(std::move(f));
}

void CoreWorker::auto_update_wifi_icon(const textures_enum mod)
{
	int corr = static_cast<int>(mod);
	if (corr < static_cast<int>(textures_enum::__FIRST_WIFI) || corr > static_cast<int>(textures_enum::__LAST_WIFI)) return;
	corr -= static_cast<int>(textures_enum::__FIRST_WIFI);
	shrd.wifi_blk.set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(corr));
}

void CoreWorker::hook_display_load()
{
	shrd.progressbar_mode = true;
	_post_update_display_one_o_one();
	al_init_primitives_addon();
	dspy.disp.hook_draw_function([&](const display_async& self) {

		const auto timm = std::chrono::system_clock::now() + std::chrono::milliseconds(self.get_is_economy_mode_activated() ? 33 : 3);

		const float min_x = self.get_width() * 0.2;
		const float max_x = self.get_width() * 0.8;
		const float min_y = self.get_height() * 0.75;
		const float max_y = self.get_height() * 0.8;
		const float& cp = shrd.generic_progressbar;// (shrd.generic_progressbar < 0.0f ? 0.0 : (shrd.generic_progressbar > 1.0f ? 1.0f : shrd.generic_progressbar));

		const float x_off = min_x + static_cast<float>(max_x - min_x) * cp;

		ALLEGRO_VERTEX vf[] = {
			ALLEGRO_VERTEX{min_x, min_y, 0, 0, 0, color(0.25f + 0.05f * cp, 0.25f + 0.05f * cp, 0.25f + 0.05f * cp) },
			ALLEGRO_VERTEX{max_x, min_y, 0, 0, 0, color(0.25f + 0.05f * cp, 0.25f + 0.05f * cp, 0.25f + 0.05f * cp) },
			ALLEGRO_VERTEX{max_x, max_y, 0, 0, 0, color(0.15f + 0.05f * cp, 0.15f + 0.05f * cp, 0.15f + 0.05f * cp) },
			ALLEGRO_VERTEX{min_x, max_y, 0, 0, 0, color(0.15f + 0.05f * cp, 0.15f + 0.05f * cp, 0.15f + 0.05f * cp) }
		};
		ALLEGRO_VERTEX v1[] = {
			ALLEGRO_VERTEX{min_x, min_y, 0, 0, 0, color(0.65f - 0.3f * cp, 0.65f - 0.1f * cp, 0.65f - 0.3f * cp) },
			ALLEGRO_VERTEX{x_off, min_y, 0, 0, 0, color(0.73f            , 0.73f + 0.5f * cp, 0.73f            ) },
			ALLEGRO_VERTEX{x_off, max_y, 0, 0, 0, color(0.43f            , 0.43f + 0.5f * cp, 0.43f            ) },
			ALLEGRO_VERTEX{min_x, max_y, 0, 0, 0, color(0.35f - 0.3f * cp, 0.35f - 0.1f * cp, 0.35f - 0.3f * cp) }
		};

		color(0, 0, 0).clear_to_this();
		int indices1[] = { 0, 1, 2, 3 };
		al_draw_indexed_prim((void*)vf, nullptr, nullptr, indices1, 4, ALLEGRO_PRIM_TRIANGLE_FAN);
		al_draw_indexed_prim((void*)v1, nullptr, nullptr, indices1, 4, ALLEGRO_PRIM_TRIANGLE_FAN);

		std::this_thread::sleep_until(timm);
	});
}

void CoreWorker::hook_display_default()
{
	shrd.progressbar_mode = false;
	_post_update_display();
	dspy.disp.hook_draw_function([&](const display_async& self) {

		try {
			const auto timm = std::chrono::system_clock::now() + std::chrono::milliseconds(self.get_is_economy_mode_activated() ? 33 : 3);
			
			shrd.background_color.clear_to_this();

			shrd.casted_boys[shrd.screen].safe([&](std::vector<sprite_pair>& mypairs) {
				for (auto& i : mypairs) {
					i.get_sprite()->think();
					i.get_sprite()->draw();
				}
				});

			{
				shrd.wifi_blk.think();
				shrd.wifi_blk.draw();

				const auto mous = shrd.mouse_pair.get_sprite();
				mous->think();
				mous->draw();
			}

			std::this_thread::sleep_until(timm);
		}
		catch (const std::exception& e) {
			cout << console::color::DARK_RED << "Exception @ drawing thread: " << e.what();
		}
	});
}

bool CoreWorker::start_esp32_threads()
{
	espp.close_all();
	espp.con = std::make_unique<_esp32_communication::host_stuff>();

	if (!espp.con->hosting.setup(socket_config().set_port(ESP_HOST_PORT))) return false;

	espp.commu.task_async([&] {
		auto cli = espp.con->hosting.listen(5);
		if (cli.has_socket()) {
			if (espp.con->current.has_socket()) {
				cli.close_socket(); // abort new unwanted ones
			}
			else { // has no connection, accept new.
				cout << console::color::AQUA << "[CLIENT] New client!";
				espp.con->current = std::move(cli);
				espp.status = _esp32_communication::package_status::IDLE;
				auto_update_wifi_icon(textures_enum::WIFI_IDLE);
			}
		}
		if (!espp.con->current.has_socket() && espp.status != _esp32_communication::package_status::NON_CONNECTED) {
			espp.status = _esp32_communication::package_status::NON_CONNECTED;
			auto_update_wifi_icon(textures_enum::WIFI_SEARCHING);
			cout << console::color::AQUA << "[CLIENT] Client disconnected.";
		}
	}, thread::speed::INTERVAL, 1.0);

	espp.procc.task_async([&] {
		if (!espp.con->current.has_socket()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			return;
		}
		else {
			TCP_client& client = espp.con->current;

#ifdef CONNECTION_VERBOSE
			cout << console::color::DARK_PURPLE << "ESP32 SEND";
#endif
			// ############################################
			// # SEND
			// ############################################
			esp_package pkg, spkg;

			if (espp.packages_to_send.size()) {
				espp.packages_to_send.safe([&](std::vector<esp_package>& vec) {spkg = vec.front(); vec.erase(vec.begin()); });
			}
			else {
				MYASST2(build_both_empty(&spkg) == 1, "Can't prepare empty package.");
			}
			MYASST2(client.send((char*)&spkg, sizeof(spkg)), "Can't prepare empty package.");

#ifdef CONNECTION_VERBOSE
			cout << console::color::DARK_PURPLE << "ESP32 RECV";
#endif
			// ############################################
			// # RECV
			// ############################################
			MYASST2(client.recv(pkg), "Failed recv client.");

#ifdef CONNECTION_VERBOSE
			cout << console::color::DARK_PURPLE << "ESP32 HANDLE";
#endif

			switch (pkg.type) {
			case PKG_ESP_DETECTED_SOMETHING:
			{
				cout << console::color::AQUA << "[CLIENT] ESP32 said it detected something! Expecting/receiving a file...";
				
				espp.package_combine_file = make_hybrid_derived<file, tempfile>();
				//espp.package_combine_file = make_hybrid<file>();
				if (!((tempfile*)espp.package_combine_file.get())->open("ass_temp_handling_texture_XXXXXX.jpg")) {
				//if (!(espp.package_combine_file->open("ass_temp_handling_texture_" + std::to_string((int)al_get_time()) + ".jpg", file::open_mode_e::READWRITE_REPLACE))) {
					cout << console::color::RED << "[CLIENT] I can't open a temporary file. Sorry! ABORTING!";
					espp.status = _esp32_communication::package_status::NON_CONNECTED;
					auto_update_wifi_icon(textures_enum::WIFI_SEARCHING);
					client.close_socket();
					return;
				}
				espp.status = _esp32_communication::package_status::PROCESSING_IMAGE;
				auto_update_wifi_icon(textures_enum::WIFI_TRANSFER);
			}
			break;
			case PKG_ESP_JPG_SEND:
			{
				//std::string mov;
				//for (unsigned long a = 0; a < pkg.len; a++) mov += pkg.data.esp.raw_data[a];
				//cout << console::color::AQUA << "[CLIENT] ESP32 block of data:";
				//cout << console::color::AQUA << "[CLIENT] {" << mov << "}";
				//cout << console::color::AQUA << "[CLIENT] Remaining: " << pkg.left;
				//cout << console::color::AQUA << "[CLIENT] Size of this: " << pkg.len;
				//cout << console::color::AQUA << "[CLIENT] Receiving packages... (remaining: " << pkg.left << " package(s), max size remaining: " << (pkg.left * PACKAGE_SIZE) << ")";
				if (espp.package_combine_file.empty()) {
					cout << console::color::RED << "[CLIENT] The package order wasn't right! File was not opened! ABORTING!";
					espp.status = _esp32_communication::package_status::NON_CONNECTED;
					auto_update_wifi_icon(textures_enum::WIFI_SEARCHING);
					client.close_socket();
					return;
				}

				if (espp.package_combine_file->write(pkg.data.raw, pkg.len) != pkg.len) {
					cout << console::color::RED << "[CLIENT] Temporary file has stopped working?! ABORTING!";
					espp.status = _esp32_communication::package_status::NON_CONNECTED;
					auto_update_wifi_icon(textures_enum::WIFI_SEARCHING);
					client.close_socket();
					return;
				}

				if (pkg.left == 0) {
					cout << console::color::RED << "[CLIENT] End of file!";
					espp.package_combine_file->flush();
					auto_update_wifi_icon(textures_enum::WIFI_THINKING);

					cout << console::color::AQUA << "[CLIENT] Opening file and analysing...";

					for (size_t fails = 0; fails < 50 && !espp.package_combine_file->flush(); fails++) {
						cout << console::color::AQUA << "[CLIENT] Can't flush! Trying again.";
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}

					cout << console::color::AQUA << "[CLIENT] File has " << espp.package_combine_file->size() << " bytes.";

					espp.package_combine_file->seek(0, file::seek_mode_e::BEGIN);

					{
						std::vector<char> buff;
						char minibuf[256];
						size_t herr = 0;
						while (herr = espp.package_combine_file->read(minibuf, 256)) buff.insert(buff.end(), minibuf, minibuf + herr);

						cout << console::color::AQUA << "Check HASH: " << sha256(buff) << ", size: " << buff.size();
					}

					espp.package_combine_file->seek(0, file::seek_mode_e::BEGIN);

					bool gud = false;

					auto txtur = make_hybrid<texture>();

					//dspy.disp.add_run_once_in_drawing_thread([&] {
					//	gud = txtur->load(espp.package_combine_file);
					//}).wait();
					gud = txtur->load(espp.package_combine_file);

					if (!gud) { // txtur->load(texture_config().set_file(espp.package_combine_file).set_flags(ALLEGRO_MEMORY_BITMAP).set_format(ALLEGRO_PIXEL_FORMAT_ANY))
						cout << console::color::RED << "[CLIENT] Could not load the file as texture! ABORT!";
						espp.status = _esp32_communication::package_status::NON_CONNECTED;
						auto_update_wifi_icon(textures_enum::WIFI_SEARCHING);
						client.close_socket();
						return;
					}

					cout << console::color::AQUA << "[CLIENT] Working on image...";

					// do analysis
					//shrd.is_tasking_image = true;
					//shrd.background_color = process_image(*txtur, shrd.conf);
					//shrd.is_tasking_image = false;

					shrd.latest_esp32_texture.replace_shared(std::move(txtur->duplicate())); // gonna be translated to VIDEO_BITMAP
					shrd.latest_esp32_texture_orig.replace_shared(std::move(txtur.reset_shared())); // this is MEMORY_BITMAP
					shrd.latest_esp32_file.replace_shared(std::move(espp.package_combine_file.reset_shared()));

					auto_handle_process_image(false); // has to wait

					auto_update_wifi_icon(textures_enum::WIFI_COMMAND);

					// do something
					build_pc_good_or_bad(&spkg, shrd.good_perc >= shrd.bad_perc ? 1 : 0);
					espp.packages_to_send.push_back(std::move(spkg));
					std::this_thread::sleep_for(std::chrono::milliseconds(60));
				}

				espp.status = pkg.left > 0 ? _esp32_communication::package_status::PROCESSING_IMAGE : _esp32_communication::package_status::IDLE;
				auto_update_wifi_icon(pkg.left > 0 ? textures_enum::WIFI_TRANSFER : textures_enum::WIFI_IDLE);
			}
			break;
			case PKG_ESP_WEIGHT_INFO:
			{
				cout << console::color::AQUA << "[CLIENT] ESP32 got weight info:";
				cout << console::color::AQUA << "[CLIENT] Good: " << pkg.data.esp.weight_info.good_side_kg << " kg";
				cout << console::color::AQUA << "[CLIENT] Bad:  " << pkg.data.esp.weight_info.bad_side_kg << " kg";
				espp.status = _esp32_communication::package_status::IDLE;
				auto_update_wifi_icon(textures_enum::WIFI_IDLE);
			}
			break;
			default:
				espp.status = _esp32_communication::package_status::IDLE;
				auto_update_wifi_icon(textures_enum::WIFI_IDLE);
#ifdef CONNECTION_VERBOSE
				cout << console::color::DARK_PURPLE << "ESP32 NO DATA";
#endif
				break;
			}
		}
	}, thread::speed::HIGH_PERFORMANCE);

	return true;
}

bool CoreWorker::full_load()
{
	set_app_name("AutoSmokeSelector APP");

	post_progress_val(0.00f);
	cout << console::color::DARK_GRAY << "Creating display...";

	if (!dspy.disp.create(
		display_config()
		.set_display_mode(
			display_options()
			.set_width(800)
			.set_height(800)
		)
		.set_extra_flags(/*ALLEGRO_RESIZABLE |*/ ALLEGRO_OPENGL/* | ALLEGRO_NOFRAME*/)
		.set_window_title(get_app_name())
		.set_fullscreen(false)
		//.set_vsync(true)
		.set_use_basic_internal_event_system(true)
	)) {
		cout << console::color::DARK_RED << "Failed creating display.";
		full_close("Can't create display!");
		return false;
	}

	cout << console::color::DARK_GRAY << "Hooking progress bar...";

	hook_display_load();
	post_progress_val(0.01f);

	cout << console::color::DARK_GRAY << "Initializing assist thread...";
	shrd.async_queue.task_async([&] {shrd.__async_queue(); });

	cout << console::color::DARK_GRAY << "Setting up window icon...";

	dspy.disp.set_icon_from_icon_resource(IDI_ICON1);
	post_progress_val(0.05f);

	cout << console::color::DARK_GRAY << "Loading configuration file...";

	make_path(get_standard_path());

	if (!shrd.conf.load(get_standard_path() + "config.ini")) {
		cout << console::color::YELLOW << "Creating new config file...";
		shrd.conf.save_path(get_standard_path() + "config.ini");
	}

	shrd.conf.ensure("processing", "saturation_compensation", 0.45, config::config_section_mode::SAVE);// how much sat compensation
	shrd.conf.ensure("processing", "brightness_compensation", 0.10, config::config_section_mode::SAVE);// how much compensation to bright (up to +10%)
	shrd.conf.ensure("processing", "overflow_boost", 1.05, config::config_section_mode::SAVE);// 1.0f = no overflow, less = less brightness and color
	shrd.conf.ensure("processing", "area_center_x", 0.3f, config::config_section_mode::SAVE);// width of the center to consider
	shrd.conf.ensure("processing", "area_center_y", 0.3f, config::config_section_mode::SAVE);// height of the center to consider
	shrd.conf.ensure("processing", "center_weight", 9, config::config_section_mode::SAVE);// average weight of the center. More = more times considered
	shrd.conf.ensure("reference", "good_plant", { (231.0f / 255.0f),(135.0f / 255.0f), (65.0f / 255.0f) }, config::config_section_mode::SAVE);
	shrd.conf.ensure("reference", "bad_plant", { (19.0f / 255.0f),(140.0f / 255.0f), (74.0f / 255.0f) }, config::config_section_mode::SAVE);

	if (!shrd.conf.flush()) {
		cout << console::color::DARK_RED << "Failed saving config at least once!";
		full_close("Could not create/load configuration file properly!");
		return false;
	}
	shrd.conf.auto_save(true);

	shrd.good_plant = config_to_color(shrd.conf, "reference", "good_plant");
	shrd.bad_plant = config_to_color(shrd.conf, "reference", "bad_plant");

	cout << console::color::DARK_GRAY << "Preparing event handlers...";

	dspy.disp.hook_event_handler([&](const ALLEGRO_EVENT& ev) {handle_display_event(ev); });
	shrd.mouse_work.hook_event([&](const int tp, const mouse::mouse_event& me) {handle_mouse_event(tp, me); });
	post_progress_val(0.10f);

	cout << console::color::DARK_GRAY << "Hiding mouse...";

	dspy.disp.add_run_once_in_drawing_thread([&] { al_hide_mouse_cursor(dspy.disp.get_raw_display()); });
	post_progress_val(0.12f);

	cout << console::color::DARK_GRAY << "Loading font resource...";

	shrd.file_font = make_hybrid_derived<file, memfile>();
	*shrd.get_file_font() = get_executable_resource_as_memfile(IDR_FONT1, resource_type_e::FONT);
	if (shrd.get_file_font()->size() == 0) {
		cout << console::color::DARK_RED << "Failed loading font resource.";
		full_close("Could not load font resource properly!");
		return false;
	}
	post_progress_val(0.20f);

	cout << console::color::DARK_GRAY << "Loading atlas resource...";

	shrd.file_atlas = make_hybrid_derived<file, memfile>();
	*shrd.get_file_atlas() = get_executable_resource_as_memfile(IDB_PNG1, (WinString)L"PNG");
	if (shrd.get_file_atlas()->size() == 0) {
		cout << console::color::DARK_RED << "Failed loading atlas resource.";
		full_close("Could not load atlas png resource properly!");
		return false;
	}
	post_progress_val(0.40f);

	cout << console::color::DARK_GRAY << "Linking font resource...";

	dspy.src_font = make_hybrid<font>();
	if (!dspy.src_font->load(font_config().set_file(shrd.file_font).set_is_ttf(true).set_resolution(150))) {
		cout << console::color::DARK_RED << "Failed linking font resource.";
		full_close("Can't link font resource!");
		return false;
	}
	post_progress_val(0.45f);

	cout << console::color::DARK_GRAY << "Linking atlas resource...";

	dspy.src_atlas = make_hybrid<texture>();
	if (!dspy.src_atlas->load(texture_config().set_file(shrd.file_atlas).set_flags(ALLEGRO_MAG_LINEAR|ALLEGRO_MIN_LINEAR))) {
		cout << console::color::DARK_RED << "Failed linking atlas resource.";
		full_close("Can't link atlas png resource!");
		return false;
	}
	post_progress_val(0.50f);

	cout << console::color::DARK_GRAY << "Creating sub-bitmaps...";

	shrd.texture_map[textures_enum::MOUSE_LOADING]		= make_hybrid<texture>(dspy.src_atlas->create_sub(0, 0, 200, 200));
	//shrd.texture_map["closex"]						= make_hybrid<texture>(dspy.src_atlas->create_sub(200, 0, 150, 150));
	shrd.texture_map[textures_enum::MOUSE]				= make_hybrid<texture>(dspy.src_atlas->create_sub(0, 200, 200, 200));
	shrd.texture_map[textures_enum::BUTTON_UP]			= make_hybrid<texture>(dspy.src_atlas->create_sub(350, 0, 600, 90));
	shrd.texture_map[textures_enum::BUTTON_DOWN]		= make_hybrid<texture>(dspy.src_atlas->create_sub(350, 90, 600, 90));

	shrd.texture_map[textures_enum::WIFI_SEARCHING]		= make_hybrid<texture>(dspy.src_atlas->create_sub(350, 180, 100, 100));
	shrd.texture_map[textures_enum::WIFI_IDLE]			= make_hybrid<texture>(dspy.src_atlas->create_sub(450, 180, 100, 100));
	shrd.texture_map[textures_enum::WIFI_TRANSFER]		= make_hybrid<texture>(dspy.src_atlas->create_sub(550, 180, 100, 100));
	shrd.texture_map[textures_enum::WIFI_THINKING]		= make_hybrid<texture>(dspy.src_atlas->create_sub(650, 180, 100, 100));
	shrd.texture_map[textures_enum::WIFI_COMMAND]		= make_hybrid<texture>(dspy.src_atlas->create_sub(750, 180, 100, 100));
	
	shrd.latest_esp32_texture = make_hybrid<texture>(); // just so it gets replaced later
	//shrd.latest_esp32_texture->create(800, 600);

	post_progress_val(0.75f);

	cout << console::color::DARK_GRAY << "Preparing scenes...";
	{
		hybrid_memory<sprite> each;
		text* txt = nullptr;
		block* blk = nullptr;

		const auto make_txt = [&] {
			each = make_hybrid_derived<sprite, text>();
			txt = (text*)(each.get());
		};
		const auto make_blk = [&] {
			each = make_hybrid_derived<sprite, block>();
			blk = (block*)(each.get());
		};


		cout << console::color::DARK_GRAY << "Building global sprites...";
		{
			blk = (block*)shrd.mouse_pair.get_sprite();
			blk->texture_insert(shrd.texture_map[textures_enum::MOUSE]);
			blk->texture_insert(shrd.texture_map[textures_enum::MOUSE_LOADING]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.0f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 4.0f);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.mouse_pair.set_tick([&](sprite* raw) {
				block* bl = (block*)raw;
				bl->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, get_is_loading() ? 1 : 0);
				bl->set<float>(enum_sprite_float_e::ROTATION, get_is_loading() ? static_cast<float>(std::fmod(al_get_time(), ALLEGRO_PI * 2.0)) : 0.0f);
			});

						
			shrd.wifi_blk.texture_insert(shrd.texture_map[textures_enum::WIFI_SEARCHING]);
			shrd.wifi_blk.texture_insert(shrd.texture_map[textures_enum::WIFI_IDLE]);
			shrd.wifi_blk.texture_insert(shrd.texture_map[textures_enum::WIFI_TRANSFER]);
			shrd.wifi_blk.texture_insert(shrd.texture_map[textures_enum::WIFI_THINKING]);
			shrd.wifi_blk.texture_insert(shrd.texture_map[textures_enum::WIFI_COMMAND]);
			shrd.wifi_blk.set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			shrd.wifi_blk.set<float>(enum_sprite_float_e::POS_X, 0.85f);
			shrd.wifi_blk.set<float>(enum_sprite_float_e::POS_Y, -0.85f);
			shrd.wifi_blk.set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			//shrd.wifi_blk.set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 4.0f);
			//shrd.wifi_blk.set<bool>(enum_sprite_boolean_e::DRAW_TRANSFORM_COORDS_KEEP_SCALE, true);
			shrd.wifi_blk.set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
		}
		post_progress_val(0.80f);

		cout << console::color::DARK_GRAY << "Building HOME sprites...";
		{
			shrd.screen_set[stage_enum::HOME].min_y = 0.0f;
			shrd.screen_set[stage_enum::HOME].max_y = 0.0f;

			// Darken everything
			make_blk();
			blk->set<float>(enum_sprite_float_e::SCALE_G, 2.0f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.0f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.1f, 0.1f, 0.1f, 0.6f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			shrd.casted_boys[stage_enum::HOME].push_back({ std::move(each) });

			// Testing zone text home
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.25f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, -0.6f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Auto Smoke Selector"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::HOME].push_back({ std::move(each) });

			// Check work button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 2.97f);
			blk->set<float>(enum_sprite_float_e::POS_X, -0.37f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.2f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(25, 200, 25));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::HOME].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0); if (down.is_unclick && down.is_mouse_on_it) { 
					shrd.screen = stage_enum::CONFIG; } }
			});
			// Check work text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.15f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, -0.37f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Setup"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::HOME].push_back({ std::move(each) });

			// Only preview button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 2.97f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.37f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.2f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(25, 200, 25));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::HOME].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0); if (down.is_unclick && down.is_mouse_on_it) { 
					shrd.screen = stage_enum::PREVIEW; } }
			});
			// Only preview text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.15f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.37f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Preview"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::HOME].push_back({ std::move(each) });

			// Import image
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 6.6667f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(25, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::HOME].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_unclick && down.is_mouse_on_it) { 
						async_task([&] {
							cout << console::color::AQUA << "[FILECHOOSER] Asking for file...";

							auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(
								al_create_native_file_dialog(
									nullptr,
									"Choose one image file!",
									"*.jpg;*.png;*.bmp;*.jpeg",
									ALLEGRO_FILECHOOSER_FILE_MUST_EXIST | ALLEGRO_FILECHOOSER_PICTURES
								), al_destroy_native_file_dialog);

							bool got_something = al_show_native_file_dialog(nullptr, dialog.get());
							int amount_of_items_selected = got_something ? al_get_native_file_dialog_count(dialog.get()) : 0; // must be 1 or bigger.

							cout << console::color::AQUA << "[FILECHOOSER] File check: HAS? " << (got_something ? "TRUE" : "FALSE") << " SIZE=" << amount_of_items_selected;

							if (got_something && amount_of_items_selected > 0) {
								std::string path = al_get_native_file_dialog_path(dialog.get(), 0);
								cout << console::color::AQUA << "[FILECHOOSER] Managing path...";

								auto fpp = make_hybrid<file>();
								auto txtur = make_hybrid<texture>();

								if (!fpp->open(path.c_str(), file::open_mode_e::READ_TRY)) {
									al_show_native_message_box(nullptr, "Failed", "Failed opening file.", ("I could not open '" + path + "'. Sorry!").c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
									return;
								}

								bool gud = txtur->load(fpp);

								if (!gud) {
									al_show_native_message_box(nullptr, "Failed", "Failed opening file.", ("I could not load '" + path + "' as an image. Sorry!").c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
									return;
								}

								cout << console::color::AQUA << "[FILECHOOSER] Processing image...";

								shrd.latest_esp32_texture.replace_shared(std::move(txtur->duplicate())); // gonna be translated to VIDEO_BITMAP
								shrd.latest_esp32_texture_orig.replace_shared(std::move(txtur.reset_shared())); // this is MEMORY_BITMAP
								shrd.latest_esp32_file.replace_shared(std::move(fpp.reset_shared()));

								auto_handle_process_image(false);

								cout << console::color::AQUA << "[FILECHOOSER] Done.";

								shrd.screen = stage_enum::CONFIG;
							}
							else if (got_something && amount_of_items_selected == 0) {
								cout << console::color::AQUA << "[FILECHOOSER] Showing error expected file but list is empty.";
								al_show_native_message_box(nullptr, "Failed", "Failed opening file.", "I didn't get any files! This looks like a bug to me.", nullptr, ALLEGRO_MESSAGEBOX_ERROR);
							}
							cout << console::color::AQUA << "[FILECHOOSER] Ended.";
						});
					} }
			});
			// Import image text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.15f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.43f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Open custom..."));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::HOME].push_back({ std::move(each) });

			// Exit button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 6.6667f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.8f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(45, 45, 65));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::HOME].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0); if (down.is_unclick && down.is_mouse_on_it) { shrd.kill_all = true; } }
			});
			// Exit button text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.15f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.73f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Exit"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::HOME].push_back({ std::move(each) });

		}
		post_progress_val(0.85f);

		cout << console::color::DARK_GRAY << "Building CONFIG sprites...";
		{
			shrd.screen_set[stage_enum::CONFIG].min_y = 0.0f;
			shrd.screen_set[stage_enum::CONFIG].max_y = 1.2f;

			// Image rendering
			make_blk();
			blk->texture_insert(shrd.latest_esp32_texture);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 1.5f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.7f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.15f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.5f, 0.5f, 0.5f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// Go Home button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 6.6667f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.85f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 25, 25));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0); if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::HOME; } }
				});
			// Go Home text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.15f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, -0.92f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Back"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// Set bad button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.13f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 6.0f);
			blk->set<float>(enum_sprite_float_e::POS_X, -0.45f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.50f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(shrd.conf, "reference", "bad_plant"));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					s->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(shrd.conf, "reference", "bad_plant"));
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_unclick && down.is_mouse_on_it) {
						color_to_config(shrd.conf, "reference", "bad_plant", shrd.background_color);
						shrd.bad_plant = shrd.background_color;
						shrd.bad_perc = how_far(shrd.bad_plant, shrd.background_color);
						shrd.good_perc = how_far(shrd.good_plant, shrd.background_color);
					}
				}
				});
			// Set bad text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, -0.45f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.465f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Set this as \"BAD\" color"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// Set good button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.13f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 6.0f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.45f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.50f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(shrd.conf, "reference", "good_plant"));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					s->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(shrd.conf, "reference", "good_plant"));
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_unclick && down.is_mouse_on_it) {
						color_to_config(shrd.conf, "reference", "good_plant", shrd.background_color);
						shrd.bad_plant = shrd.background_color;
						shrd.bad_perc = how_far(shrd.bad_plant, shrd.background_color);
						shrd.good_perc = how_far(shrd.good_plant, shrd.background_color);
					}
				}
				});
			// Set good text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.45f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.465f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Set this as \"GOOD\" color"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });


			// Resume of percentage bad/good
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, -0.65f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Thinking..."));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			txt->shadow_insert(text_shadow{ 0.005f, 0.080f, color(0,0,0) });

			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {text* t = (text*)s; t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::to_string(shrd.good_perc * 100.0f) + "% good / " + std::to_string(shrd.bad_perc * 100.0f) + "% bad"); },
				[](auto,auto) {}
				});


			float offauto = 0.8f;

			// ================ SATURATION BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (shrd.conf.get_as<float>("processing", "saturation_compensation") * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.4f,
						0.4f + 0.6f * shrd.conf.get_as<float>("processing", "saturation_compensation"),
						0.4f));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						shrd.conf.set("processing", "saturation_compensation", (down.cpy.real_posx + 0.7f) / 1.4f);
					}
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							//dspy.disp.add_run_once_in_drawing_thread([&] {shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf); });
							//shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf);
							auto_handle_process_image();
						}
					}
				}
				});

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Saturation: +00% boost"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Saturation: +" + std::to_string((int)((shrd.conf.get_as<float>("processing", "saturation_compensation")) * 100.0f)) + "% boost"
					));
				},
				[&](auto, auto) {} });

			offauto += 0.22f;
			// ================ BRIGHTNESS BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + ((shrd.conf.get_as<float>("processing", "brightness_compensation") * 2.0f) * 1.4f)); // up to 0.5
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "brightness_compensation") * 2.0f),
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "brightness_compensation") * 2.0f),
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "brightness_compensation") * 2.0f)));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						shrd.conf.set("processing", "brightness_compensation", 0.5f * (down.cpy.real_posx + 0.7f) / 1.4f);
					}
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							//dspy.disp.add_run_once_in_drawing_thread([&] {shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf); });
							//shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf);
							auto_handle_process_image();
						}
					}
				}
				});

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Brightness: +00% boost"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Brightness: +" + std::to_string((int)((shrd.conf.get_as<float>("processing", "brightness_compensation")) * 100.0f)) + "% boost"
					));
				},
				[&](auto, auto) {} });

			offauto += 0.22f;
			// ================ OVERFLOW BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + ((shrd.conf.get_as<float>("processing", "overflow_boost") - 1.0f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.4f - 0.4f * (shrd.conf.get_as<float>("processing", "overflow_boost") - 1.0f),
						0.7f + 0.3f * (shrd.conf.get_as<float>("processing", "overflow_boost") - 1.0f),
						0.4f - 0.4f * (shrd.conf.get_as<float>("processing", "overflow_boost") - 1.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						shrd.conf.set("processing", "overflow_boost", 1.0f + (down.cpy.real_posx + 0.7f) / 1.4f);
					}
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							//dspy.disp.add_run_once_in_drawing_thread([&] {shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf); });
							//shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf);
							auto_handle_process_image();
						}
					}
				}
				});

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Gain: +00%"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Gain: +" + std::to_string((int)((shrd.conf.get_as<float>("processing", "overflow_boost") - 1.0f) * 100.0f)) + "%"
					));
				},
				[&](auto, auto) {} });

			offauto += 0.22f;
			// ================ CENTER WEIGHT BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (((shrd.conf.get_as<float>("processing", "center_weight") - 1.0f) / 29.0f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * ((shrd.conf.get_as<float>("processing", "center_weight") - 1.0f) / 29.0f),
						0.6f,
						0.6f + 0.4f * ((shrd.conf.get_as<float>("processing", "center_weight") - 1.0f) / 29.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						shrd.conf.set("processing", "center_weight", static_cast<int>(1.1f + 29.0f * (down.cpy.real_posx + 0.7f) / 1.4f));
					}
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							//dspy.disp.add_run_once_in_drawing_thread([&] {shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf); });
							//shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf);
							auto_handle_process_image();
						}
					}
				}
				});

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Center weight: x1"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Center weight: x" + std::to_string((int)(shrd.conf.get_as<float>("processing", "center_weight")))
					));
				},
				[&](auto, auto) {} });

			offauto += 0.22f;
			// ================ X AXIS BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (((shrd.conf.get_as<float>("processing", "area_center_x") - 0.1f) / 0.9f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "area_center_x") - 1.0f),
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "area_center_x") - 1.0f),
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "area_center_x") - 1.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						shrd.conf.set("processing", "area_center_x", 0.1f + 0.9f * (down.cpy.real_posx + 0.7f) / 1.4f);
					}
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							//dspy.disp.add_run_once_in_drawing_thread([&] {shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf); });
							//shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf);
							auto_handle_process_image();
						}
					}
				}
				});

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Center width: 00%"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Center width: " + std::to_string((int)(shrd.conf.get_as<float>("processing", "area_center_x") * 100.0f)) + "%"
					));
				},
				[&](auto, auto) {} });

			offauto += 0.22f;
			// ================ Y AXIS BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({ std::move(each) });

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (((shrd.conf.get_as<float>("processing", "area_center_y") - 0.1f) / 0.9f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "area_center_y") - 1.0f),
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "area_center_y") - 1.0f),
						0.6f + 0.4f * (shrd.conf.get_as<float>("processing", "area_center_y") - 1.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						shrd.conf.set("processing", "area_center_y", 0.1f + 0.9f * (down.cpy.real_posx + 0.7f) / 1.4f);
					}
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							//dspy.disp.add_run_once_in_drawing_thread([&] {shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf); });
							//shrd.background_color = process_image(*shrd.latest_esp32_texture, shrd.conf);
							auto_handle_process_image();
						}
					}
				}
				});

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.13f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Center height: 00%"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::CONFIG].push_back({
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Center height: " + std::to_string((int)(shrd.conf.get_as<float>("processing", "area_center_y") * 100.0f)) + "%"
					));
				},
				[&](auto, auto) {} });
		}
		post_progress_val(0.875f);

		cout << console::color::DARK_GRAY << "Building PREVIEW sprites...";
		{
			shrd.screen_set[stage_enum::PREVIEW].min_y = 0.0f;
			shrd.screen_set[stage_enum::PREVIEW].max_y = 0.0f;

			// Darken everything
			make_blk();
			blk->set<float>(enum_sprite_float_e::SCALE_G, 2.0f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.0f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.1f, 0.1f, 0.1f, 0.6f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			shrd.casted_boys[stage_enum::PREVIEW].push_back({ std::move(each) });

			// Image rendering
			make_blk();
			blk->texture_insert(shrd.latest_esp32_texture);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 1.8f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.7f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.05f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.5f, 0.5f, 0.5f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			shrd.casted_boys[stage_enum::PREVIEW].push_back({ std::move(each) });

			// Go Home button
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 6.6667f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.8f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 25, 25));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[stage_enum::PREVIEW].push_back({
				std::move(each),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0); if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::HOME; } }
			});
			// Go Home text
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.15f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, -0.87f);
			txt->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Back"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[stage_enum::PREVIEW].push_back({ std::move(each) });



			// Resume of percentage bad/good
			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, 0.73f);
			blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Thinking..."));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			txt->shadow_insert(text_shadow{ 0.005f, 0.080f, color(0,0,0) });

			shrd.casted_boys[stage_enum::PREVIEW].push_back({
				std::move(each),
				[&](sprite* s) {text* t = (text*)s; t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::to_string(shrd.good_perc * 100.0f) + "% good / " + std::to_string(shrd.bad_perc * 100.0f) + "% bad\nConsidered good? " + (shrd.good_perc >= shrd.bad_perc ? "YES" : "NO")); },
				[](auto,auto) {}
			});
		}
		post_progress_val(0.90f);
	}
	cout << console::color::DARK_GRAY << "Starting esp32 communication...";

	if (!start_esp32_threads()) {
		cout << console::color::DARK_RED << "Failed to setup host! That's sad.";
		full_close("Can't start host to ESP32 connection!");
		return false;
	}

	cout << console::color::DARK_GRAY << "Done.";
	post_progress_val(1.00f);
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	hook_display_default();
	return true;
}

void CoreWorker::post_progress_val(const float v)
{
	shrd.generic_progressbar = v > 1.0f ? 1.0f : (v < 0.0f ? 0.0f : v);
}

void CoreWorker::_post_update_display()
{
	dspy.disp.add_run_once_in_drawing_thread([&] {
		transform transf;
		transf.build_classic_fixed_proportion_auto(1.0f);
		transf.translate_inverse(0.0f, shrd.current_offy);
		shrd.wifi_blk.set<float>(enum_sprite_float_e::POS_Y, -0.85f + shrd.current_offy);
		transf.apply();
	});
}

void CoreWorker::_post_update_display_one_o_one()
{
	dspy.disp.add_run_once_in_drawing_thread([] {
		transform transf;
		transf.identity();
		transf.apply();
	});
}

void CoreWorker::post_update_display_auto()
{
	if (shrd.progressbar_mode) _post_update_display_one_o_one();
	else _post_update_display();
}

void CoreWorker::handle_display_event(const ALLEGRO_EVENT& ev)
{
	switch (ev.type) {
	case ALLEGRO_EVENT_DISPLAY_CLOSE:
		shrd.kill_all = true;
		if (++shrd.kill_tries > 5) std::terminate(); // abort totally
		break;
	case ALLEGRO_EVENT_DISPLAY_RESIZE:
	{
		post_update_display_auto();
	}
	break;
	}
}

void CoreWorker::handle_mouse_event(const int tp, const mouse::mouse_event& ev)
{
	block* mous = (block*)shrd.mouse_pair.get_sprite();
	if (!mous) return;

	switch (tp) {
	case ALLEGRO_EVENT_MOUSE_AXES:
		mous->set<float>(enum_sprite_float_e::POS_X, ev.real_posx);
		mous->set<float>(enum_sprite_float_e::POS_Y, ev.real_posy);

		if (ev.scroll_event_id(1)) {
			if (ev.scroll_event_id(1) > 0) shrd.current_offy -= 0.25f;
			else shrd.current_offy += 0.25f;
			post_update_display_auto();
		}

		shrd.casted_boys[shrd.screen].safe([&](std::vector<sprite_pair>& mypairs) {
			for (auto& i : mypairs) {
				collisionable thus(*i.get_sprite());
				thus.reset();
				sprite_pair::cond cond;

				cond.is_click = false;
				cond.is_unclick = false;
				cond.is_mouse_on_it = thus.quick_one_sprite_overlap(*mous).dir_to != 0;
				cond.is_mouse_pressed = ev.buttons_pressed != 0;
				cond.cpy = ev;

				i.update(cond);
			}
		});
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
		shrd.casted_boys[shrd.screen].safe([&](std::vector<sprite_pair>& mypairs) {
			for (auto& i : mypairs) {
				collisionable thus(*i.get_sprite());
				thus.reset();
				sprite_pair::cond cond;

				cond.is_click = true;
				cond.is_unclick = false;
				cond.is_mouse_on_it = thus.quick_one_sprite_overlap(*mous).dir_to != 0;
				cond.is_mouse_pressed = true;
				cond.cpy = ev;

				i.update(cond);
			}
		});
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
		shrd.casted_boys[shrd.screen].safe([&](std::vector<sprite_pair>& mypairs) {
			for (auto& i : mypairs) {
				collisionable thus(*i.get_sprite());
				thus.reset();
				sprite_pair::cond cond;

				cond.is_click = false;
				cond.is_unclick = true;
				cond.is_mouse_on_it = thus.quick_one_sprite_overlap(*mous).dir_to != 0;
				cond.is_mouse_pressed = false;
				cond.cpy = ev;

				i.update(cond);
			}
		});
		break;
	case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
		mous->set<bool>(enum_sprite_boolean_e::DRAW_SHOULD_DRAW, false);
		break;
	case ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY:
		mous->set<bool>(enum_sprite_boolean_e::DRAW_SHOULD_DRAW, true);
		break;
	}

	if (shrd.current_offy < shrd.screen_set[shrd.screen].min_y) {
		shrd.current_offy = shrd.screen_set[shrd.screen].min_y;
		post_update_display_auto();
	}
	else if (shrd.current_offy > shrd.screen_set[shrd.screen].max_y) {
		shrd.current_offy = shrd.screen_set[shrd.screen].max_y;
		post_update_display_auto();
	}
}

void CoreWorker::full_close(const std::string& warnstr)
{
	cout << console::color::YELLOW << "Full close called. Closing all data in order...";

	post_progress_val(0.00f);
	hook_display_load(); // no resources used here

	shrd.async_queue.signal_stop();

	// _esp32_communication
	cout << console::color::YELLOW << "- ESP32 COMMUNICATION -";

	espp.procc.join();
	espp.commu.join();
	espp.status = _esp32_communication::package_status::NON_CONNECTED;

	post_progress_val(0.10f);

	espp.close_all();

	post_progress_val(0.25f);

	// _display (textures & font)
	cout << console::color::YELLOW << "- DISPLAY (textures and font) -";

	dspy.disp.add_run_once_in_drawing_thread([&] {
		for (auto& i : shrd.texture_map) {
			i.store->destroy();
		}
		dspy.src_atlas->destroy();
		dspy.src_font->destroy();
	}).get();
	post_progress_val(0.52f);

	// _shared
	cout << console::color::YELLOW << "- SHARED -";

	shrd.mouse_work.unhook_event();
	shrd.screen = stage_enum::HOME;
	shrd.get_file_font()->close();
	shrd.get_file_atlas()->close();
	shrd.file_font.reset_this();
	shrd.file_atlas.reset_this();
	shrd.async_queue.join();
	shrd.task_queue.clear(); // drop

	post_progress_val(0.70f);

	for(auto& i : shrd.casted_boys) {
		i.second.safe([](std::vector<sprite_pair>& vec) {
			vec.clear();
		});
	}

	post_progress_val(0.99f);

	// _display (self)
	cout << console::color::YELLOW << "- DISPLAY (self) -";
	dspy.disp.destroy();

	if (warnstr.length()) {
		al_show_native_message_box(nullptr, "Error message", "The app encountered errors or warnings", warnstr.c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
	}
}

CoreWorker::CoreWorker()
	: shrd(dspy.disp)
{
}

bool CoreWorker::work_and_yield()
{
	cout << "On load.";
	if (!full_load()) return false;
	cout << "On wait.";
	while (!shrd.kill_all) std::this_thread::sleep_for(std::chrono::milliseconds(500));
	cout << "On close.";
	full_close();
	cout << "Bye.";
	return true;
}
