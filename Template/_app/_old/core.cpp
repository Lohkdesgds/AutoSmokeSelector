#include "core.h"

CoreWorker::_shared::_shared(std::function<ALLEGRO_TRANSFORM(void)> f)
	: mouse_work(f), mouse_pair(make_hybrid_derived<sprite, block>()), wifi_blk(make_hybrid_derived<sprite, block>())
{
}

memfile* CoreWorker::_shared::get_file_font()
{
	return (memfile*)file_font.get();
}

memfile* CoreWorker::_shared::get_file_atlas()
{
	return (memfile*)file_atlas.get();
}

CoreWorker::_display::_display()
	: dispev(disp)
{
}

void CoreWorker::_esp32_communication::close_all()
{
	status = package_status::NON_CONNECTED;

	procc.signal_stop();
	if (con) con->current.close_socket();
	procc.join();
	if (con) con.reset();

	package_combine_file.reset_this();
	packages_to_send.clear();

	con.reset();
}

std::string CoreWorker::get_default_config_path()
{
	return get_standard_path() + "config.ini";
}

void CoreWorker::ensure_config_defaults()
{
	shrd.conf.ensure("processing", "saturation_compensation", 0.45, config::config_section_mode::SAVE);// how much sat compensation
	shrd.conf.ensure("processing", "brightness_compensation", 0.10, config::config_section_mode::SAVE);// how much compensation to bright (up to +10%)
	shrd.conf.ensure("processing", "overflow_boost", 1.05, config::config_section_mode::SAVE);// 1.0f = no overflow, less = less brightness and color
	shrd.conf.ensure("processing", "area_center_x", 0.3f, config::config_section_mode::SAVE);// width of the center to consider
	shrd.conf.ensure("processing", "area_center_y", 0.3f, config::config_section_mode::SAVE);// height of the center to consider
	shrd.conf.ensure("processing", "center_weight", 9, config::config_section_mode::SAVE);// average weight of the center. More = more times considered
	shrd.conf.ensure("reference", "good_plant", { (231.0f / 255.0f),(135.0f / 255.0f), (65.0f / 255.0f) }, config::config_section_mode::SAVE);
	shrd.conf.ensure("reference", "bad_plant", { (19.0f / 255.0f),(140.0f / 255.0f), (74.0f / 255.0f) }, config::config_section_mode::SAVE);
	shrd.conf.ensure("filemanagement", "export_path", std::string("&"), config::config_section_mode::SAVE);
	shrd.conf.ensure("filemanagement", "save_all", false, config::config_section_mode::SAVE);
	shrd.conf.ensure("display", "size_length", 800, config::config_section_mode::SAVE);
	{
		if (shrd.conf.get_as<int>("display", "size_length") < 150) shrd.conf.set("display", "size_length", 150);
	}
}

bool CoreWorker::get_is_loading()
{
	//return espp.con ? !espp.con->current.has_socket() : true;
	return shrd.on_throw_thr.id && !shrd.on_throw_thr.has_ended();
}

void CoreWorker::auto_handle_process_image(const bool asyncr)
{
	if (shrd.latest_esp32_texture_orig.empty()) return;

	const auto ff = [&] {
		shrd.background_color = process_image(*shrd.latest_esp32_texture_orig, shrd.conf);
		shrd.bad_perc = how_far(shrd.bad_plant, shrd.background_color);
		shrd.good_perc = how_far(shrd.good_plant, shrd.background_color);
	};

	if (asyncr) async_task(ff);
	else ff();
}

void CoreWorker::async_task(std::function<void(void)> f)
{
	if (shrd.on_throw_thr.id && !shrd.on_throw_thr.has_ended()) shrd.on_throw_thr.ended.get();
	shrd.on_throw_thr = throw_thread(f);
}

void CoreWorker::auto_update_wifi_icon(const textures_enum mod)
{
	int corr = static_cast<int>(mod);
	if (corr < static_cast<int>(textures_enum::__FIRST_WIFI) || corr > static_cast<int>(textures_enum::__LAST_WIFI)) return;
	corr -= static_cast<int>(textures_enum::__FIRST_WIFI);
	((block*)shrd.wifi_blk.get_notick())->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(corr));
}

void CoreWorker::hook_display_load()
{
	shrd.progressbar_mode = true;
	_post_update_display_one_o_one();
	al_init_primitives_addon();
	dspy.disp.set_fps_limit(60);
	dspy.disp.set_economy_fps(20);
	dspy.display_default_draw = false;
	while (dspy.display_reflected_draw != dspy.display_default_draw) std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void CoreWorker::hook_display_default()
{
	shrd.progressbar_mode = false;
	_post_update_display();
	dspy.disp.set_fps_limit(max_fps_allowed);
	dspy.disp.set_economy_fps(20);
	dspy.display_default_draw = true;
	while (dspy.display_reflected_draw != dspy.display_default_draw) std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

bool CoreWorker::start_esp32_threads()
{
	espp.close_all();
	espp.con = std::make_unique<_esp32_communication::host_stuff>();

	espp.procc.task_async([&] {
		if (!espp.con->current.has_socket()) {
			if (!espp.con->current.setup(socket_config().set_port(ESP_HOST_PORT).set_family(socket_config::e_family::IPV4).set_ip_address(esp32_ip_address_fixed))) // esp32_ip_address_fixed
			{
				espp.status = _esp32_communication::package_status::NON_CONNECTED;
				auto_update_wifi_icon(textures_enum::WIFI_SEARCHING);
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				return;
			}
			else {
				auto_update_wifi_icon(textures_enum::WIFI_IDLE);
			}
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

			if (al_get_time() - espp.ask_weight_when_time > 0.0) {
				espp.ask_weight_when_time = al_get_time() + ask_weight_in_time;
				esp_package _tmp;
				build_pc_request_weight(&_tmp);
				espp.packages_to_send.push_back(std::move(_tmp));
				cout << console::color::AQUA << "[CLIENT] Auto requesting weight.";
			}

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
					cout << console::color::YELLOW << "[CLIENT] End of file!";
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

					//dspy.disp.post_task([&] {
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

					if (shrd.conf.get_as<bool>("filemanagement", "save_all")) {
						cout << console::color::AQUA << "[CLIENT] Saving file...";
						const std::string path = shrd.conf.get("filemanagement", "export_path");

						if (path.length() > 3) {
							std::tm tm;
							std::time_t tim = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
							_gmtime64_s(&tm, &tim);

							char lul[64];
							strftime(lul, 64, "%F-%H-%M-%S", &tm);
							char decc[8];
							sprintf_s(decc, "%04lld", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 1000);
							std::string fullpath = path + "/" + lul + '-' + decc + ".jpg";

							//cout << "Would save at '" << fullpath << "'";
							if (!al_save_bitmap(fullpath.c_str(), txtur->get_raw_bitmap())) {
								cout << console::color::RED << "[CLIENT] Bad news saving image.";
								async_task([path] {
									al_show_native_message_box(nullptr, u8"Falha ao salvar imagem!", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de escrever no caminho para salvar a imagem:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
								});
							}
							else {
								cout << console::color::AQUA << "[CLIENT] Saved image! (/" << (lul + '-' + std::string(decc) + ".jpg") << ")";
							}
						}
						else {
							shrd.conf.set("filemanagement", "save_all", false);
							async_task([path] {
								al_show_native_message_box(nullptr, u8"O caminho se tornou inválido?!", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho para salvar a imagem:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
							});
						}
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
				espp.current_good = pkg.data.esp.weight_info.good_side_kg;
				espp.current_bad = pkg.data.esp.weight_info.bad_side_kg;
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

	cout << console::color::DARK_GRAY << "Loading configuration file...";

	make_path(get_standard_path());

	if (!shrd.conf.load(get_default_config_path())) {
		cout << console::color::YELLOW << "Creating new config file...";
		shrd.conf.save_path(get_default_config_path());
	}

	ensure_config_defaults();

	if (!shrd.conf.flush()) {
		cout << console::color::DARK_RED << "Failed saving config at least once!";
		full_close("Could not create/load configuration file properly!");
		return false;
	}
	shrd.conf.auto_save(true);

	post_progress_val(0.00f);
	cout << console::color::DARK_GRAY << "Creating display...";

	if (!dspy.disp.create(
		display_config()
		.set_display_mode(
			display_options()
			.set_width(shrd.conf.get_as<int>("display", "size_length"))
			.set_height(shrd.conf.get_as<int>("display", "size_length"))
		)
		.set_extra_flags(ALLEGRO_RESIZABLE | ALLEGRO_OPENGL/* | ALLEGRO_NOFRAME*/)
		.set_window_title(get_app_name())
		.set_fullscreen(false)
		.set_use_basic_internal_event_system(true)
	)) {
		cout << console::color::DARK_RED << "Failed creating display.";
		full_close("Can't create display!");
		return false;
	}

	dspy.disp.hook_draw_function([this](auto&) { draw(); });
	dspy.disp.hook_exception_handler([](const std::exception& e) { cout << console::color::DARK_RED << "Exception @ drawing thread: " << e.what(); });

	cout << console::color::DARK_GRAY << "Hooking progress bar...";

	hook_display_load();
	post_progress_val(0.01f);

	cout << console::color::DARK_GRAY << "Setting up window icon...";

	dspy.disp.set_icon_from_icon_resource(IDI_ICON1);
	post_progress_val(0.05f);

	shrd.good_plant = config_to_color(shrd.conf, "reference", "good_plant");
	shrd.bad_plant = config_to_color(shrd.conf, "reference", "bad_plant");

	cout << console::color::DARK_GRAY << "Preparing event handlers...";

	dspy.dispev.hook_event_handler([&](const display_event& ev) {handle_display_event(ev.get_event()); });
	shrd.mouse_work.hook_event([&](const int tp, const mouse::mouse_event& me) {handle_mouse_event(tp, me); });
	post_progress_val(0.10f);

	cout << console::color::DARK_GRAY << "Hiding mouse...";

	dspy.disp.post_task([&] { return al_hide_mouse_cursor(dspy.disp.get_raw_display()); });
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
		auto curr_set = stage_enum::HOME;

		const auto make_txt = [&] {
			each = make_hybrid_derived<sprite, text>();
			txt = (text*)(each.get());
		};
		const auto make_blk = [&] {
			each = make_hybrid_derived<sprite, block>();
			blk = (block*)(each.get());
		};
		

		const auto make_button = [&boys = shrd.casted_boys, &font = dspy.src_font]
			(const stage_enum stag, const std::initializer_list<hybrid_memory<texture>> textures, const float boxscalg, const float boxscalx, const float boxscaly,
			const float boxposx, const float boxposy, const color boxcolor, const color textcolor, const float txtscale, const std::string& textstr, const float smoothness,
			const std::function<void(sprite*)> ftick = [](auto){}, const std::function<void(sprite*, const sprite_pair::cond&)> fclick = [](auto,auto){}, const std::function<void(sprite*)> rstf = [](auto){},
			const std::function<void(sprite*)> ftxttick = [](auto) {})
		{ {
			hybrid_memory<sprite> each;
			text* txt = nullptr;
			block* blk = nullptr;

			each = make_hybrid_derived<sprite, block>();
			blk = (block*)(each.get());

			for(const auto& it : textures) blk->texture_insert(it);
			blk->set<float>(enum_sprite_float_e::SCALE_G, boxscalg);
			blk->set<float>(enum_sprite_float_e::SCALE_X, boxscalx);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, boxscaly);
			blk->set<float>(enum_sprite_float_e::POS_X, boxposx);
			blk->set<float>(enum_sprite_float_e::POS_Y, boxposy);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, boxposx);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, boxposy);
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, smoothness <= 1.0f ? 1.0f : (1.0f / smoothness));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, boxcolor);
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			boys[stag].push_back(make_hybrid<sprite_pair>(std::move(each), ftick, fclick, rstf, stag));

			each = make_hybrid_derived<sprite, text>();
			txt = (text*)(each.get());

			txt->font_set(font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, txtscale);
			txt->set<float>(enum_sprite_float_e::SCALE_X, 0.95f);
			txt->set<float>(enum_sprite_float_e::POS_X, boxposx);
			txt->set<float>(enum_sprite_float_e::POS_Y, boxposy - txtscale * 0.45f);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, boxposx);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, boxposy - txtscale * 0.45f);
			txt->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, smoothness <= 1.0f ? 1.0f : (1.0f / smoothness));
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, textstr);
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			boys[stag].push_back(make_hybrid<sprite_pair>(std::move(each), ftxttick, [](auto, auto) {}, rstf, stag));
		} };

		const auto make_common_button = [&boys = shrd.casted_boys, &font = dspy.src_font, &txture_map = shrd.texture_map, &make_button, &curr_set](const float width, const float height, const float px, const float py, const color boxclr, const std::string& txtt,
			const std::function<void(sprite*)> ftick = [](auto){}, const std::function<void(sprite*, const sprite_pair::cond&)> fclick = [](auto,auto){}, const std::function<void(sprite*)> rstf = [](auto){}, const float animspeed = 4.0f, const float font_siz = 0.12f,
			const std::function<void(sprite*)> ftxttick = [](auto) {}) {
			make_button(curr_set, { txture_map[textures_enum::BUTTON_UP], txture_map[textures_enum::BUTTON_DOWN] }, 0.2f, width, height, px, py, boxclr, color(200, 200, 200), font_siz, txtt, animspeed, ftick, [fclick](sprite* s, const sprite_pair::cond& v) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (v.is_mouse_on_it && v.is_mouse_pressed) ? 1 : 0); fclick(s, v); }, rstf, ftxttick);
		};
		const auto make_color_box = [&boys = shrd.casted_boys, &make_button, &curr_set](const float width = 1.0f, const float height = 1.0f, const float px = 0.0f, const float py = 0.0f, const float smoothness = 0.0f, const color boxclr = color(0.1f, 0.1f, 0.1f, 0.6f),
			const std::function<void(sprite*)> ftick = [](auto) {}, const std::function<void(sprite*, const sprite_pair::cond&)> fclick = [](auto, auto) {}, const std::function<void(sprite*)> rstf = [](auto) {})
		{ {
			hybrid_memory<sprite> each;
			block* blk = nullptr;

			each = make_hybrid_derived<sprite, block>();
			blk = (block*)(each.get());

			blk->set<float>(enum_sprite_float_e::SCALE_G, 2.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, width);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, height);
			blk->set<float>(enum_sprite_float_e::POS_X, px);
			blk->set<float>(enum_sprite_float_e::POS_Y, py);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, px);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, py);
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, smoothness <= 1.0f ? 1.0f : (1.0f / smoothness));
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, boxclr);
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			boys[curr_set].push_back(make_hybrid<sprite_pair>(std::move(each), ftick, fclick, rstf, curr_set));
		} };
		const auto make_text = [&boys = shrd.casted_boys, &font = dspy.src_font, &curr_set](const float siz, const float sx, const float sy, const float px, const float py, const float smoothness, const color txtclr, const std::string& txtstr, const std::initializer_list<text_shadow> tshad = {},
			const std::function<void(sprite*)> ftick = [](auto) {}, const std::function<void(sprite*, const sprite_pair::cond&)> fclick = [](auto, auto) {}, const std::function<void(sprite*)> rstf = [](auto) {})
		{ {
			auto each = make_hybrid_derived<sprite, text>();
			auto txt = (text*)(each.get());

			txt->font_set(font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, siz);
			txt->set<float>(enum_sprite_float_e::SCALE_X, sx);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, sy);
			txt->set<float>(enum_sprite_float_e::POS_X, px);
			txt->set<float>(enum_sprite_float_e::POS_Y, py);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, px);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, py);
			txt->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, smoothness <= 1.0f ? 1.0f : (1.0f / smoothness));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, txtclr);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, txtstr);
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			for (const auto& it : tshad) txt->shadow_insert(it);
			boys[curr_set].push_back(make_hybrid<sprite_pair>(std::move(each), ftick, fclick, rstf, curr_set));
		} };


		//const auto sindx = [](const sprite* s, const size_t v) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, v); };





		cout << console::color::DARK_GRAY << "Building global sprites...";
		{
			blk = (block*)shrd.mouse_pair.get_notick();
			blk->texture_insert(shrd.texture_map[textures_enum::MOUSE]);
			blk->texture_insert(shrd.texture_map[textures_enum::MOUSE_LOADING]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.0f);
			//blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 4.0f);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.mouse_pair.set_tick([&](sprite* raw) {
				block* bl = (block*)raw;
				bl->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, get_is_loading() ? 1 : 0);
				bl->set<float>(enum_sprite_float_e::ROTATION, get_is_loading() ? static_cast<float>(/*std::fmod(*/al_get_time() * 3.0/*, ALLEGRO_PI * 2.0)*/) : 0.0f);
			});

						
			((block*)shrd.wifi_blk.get_notick())->texture_insert(shrd.texture_map[textures_enum::WIFI_SEARCHING]);
			((block*)shrd.wifi_blk.get_notick())->texture_insert(shrd.texture_map[textures_enum::WIFI_IDLE]);
			((block*)shrd.wifi_blk.get_notick())->texture_insert(shrd.texture_map[textures_enum::WIFI_TRANSFER]);
			((block*)shrd.wifi_blk.get_notick())->texture_insert(shrd.texture_map[textures_enum::WIFI_THINKING]);
			((block*)shrd.wifi_blk.get_notick())->texture_insert(shrd.texture_map[textures_enum::WIFI_COMMAND]);
			((block*)shrd.wifi_blk.get_notick())->set<float>(enum_sprite_float_e::SCALE_G, 0.17f);
			((block*)shrd.wifi_blk.get_notick())->set<float>(enum_sprite_float_e::POS_X, 0.89f);
			((block*)shrd.wifi_blk.get_notick())->set<float>(enum_sprite_float_e::POS_Y, -0.89f);
			((block*)shrd.wifi_blk.get_notick())->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 4.0f);
			((block*)shrd.wifi_blk.get_notick())->set<bool>(enum_sprite_boolean_e::DRAW_TRANSFORM_COORDS_KEEP_SCALE, true);
			((block*)shrd.wifi_blk.get_notick())->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.wifi_blk.set_tick([](sprite* s) {

				int corr = static_cast<int>(((block*)s)->get<size_t>(enum_block_sizet_e::RO_DRAW_FRAME));
				corr += static_cast<int>(textures_enum::__FIRST_WIFI);
				if (corr > static_cast<int>(textures_enum::__LAST_WIFI)) corr = -1;

				switch (corr) {
				case static_cast<int>(textures_enum::WIFI_SEARCHING):
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.35 * cos(1.5 * al_get_time() + 0.8 * sin(al_get_time() * 1.2554 + 0.3 * ((random() % 1000) / 1000.0)))));
					break;
				case static_cast<int>(textures_enum::WIFI_IDLE):
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.25 * cos(0.7 * al_get_time())));
					break;
				case static_cast<int>(textures_enum::WIFI_TRANSFER):
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.25 * cos(4.5 * al_get_time())));
					break;
				case static_cast<int>(textures_enum::WIFI_THINKING):
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(al_get_time() * 3.0));
					break;
				case static_cast<int>(textures_enum::WIFI_COMMAND):
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.25 * cos(1.5 * al_get_time())));
					break;
				default:
					s->set<float>(enum_sprite_float_e::ROTATION, 0.0f);
					break;
				}
			});
		}

		cout << console::color::DARK_GRAY << "Building HOME sprites...";
		curr_set = stage_enum::HOME;
		{
			shrd.screen_set[curr_set].min_y = 0.0f;
			shrd.screen_set[curr_set].max_y = 0.0f;

			make_color_box(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, color(0.1f, 0.1f, 0.1f, 0.6f));

			make_text(0.23f, 1.0f, 1.0f, 0.0f,  -0.67f, 5.0f, color(235, 235, 235), "Auto Smoke Selector", { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
				[](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_Y, -0.67f + static_cast<float>(0.022 * cos(al_get_time() + 0.4)));
				},
				[](auto,auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, -0.9f); });

			make_text(0.23f, 1.0f, 1.0f, 0.0f,  -0.49f, 6.0f, color(200, 200, 200), "~~~~~~~~~~~~~~~~~~", { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
				[](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_Y, -0.49f + static_cast<float>(0.022 * cos(al_get_time() + 0.2)));
					((text*)s)->set<color>(enum_sprite_color_e::DRAW_TINT, 
						color(
							0.7f + static_cast<float>(limit_range_of(static_cast<float>(50.0 * cos(1.7 * al_get_time() + 0.0) - 49.0), 0.0f, 0.1f)),
							0.7f + static_cast<float>(limit_range_of(static_cast<float>(50.0 * cos(3.2 * al_get_time() + 0.4) - 49.0), 0.0f, 0.1f)),
							0.7f + static_cast<float>(limit_range_of(static_cast<float>(50.0 * cos(4.1 * al_get_time() + 1.6) - 49.0), 0.0f, 0.1f))
						));
				},
				[](auto, auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, -0.72f); });

			make_text(0.05f, 1.7f, 1.0f, 0.0f, -0.42f, 8.0f, color(200, 200, 200), u8"by LAG | Lunaris B" + std::to_string(LUNARIS_BUILD_NUMBER), { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
				[](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_Y, -0.42f + static_cast<float>(0.022 * cos(al_get_time())));
					((text*)s)->set<color>(enum_sprite_color_e::DRAW_TINT, color(0.7f + static_cast<float>(0.15f * cos(al_get_time() * 0.833 + 0.5)), 0.7f + static_cast<float>(0.15f * sin(al_get_time() * 0.537 + 0.37)), 0.7f + static_cast<float>(0.15f * cos(al_get_time() * 0.48 + 1.25))));
				},
				[](auto, auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, -0.66f); });

			make_common_button(2.97f, 1.0f, -0.37f, 0.1f, color(25, 180, 25), "Config",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::CONFIG; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); });

			make_common_button(2.97f, 1.0f, 0.37f, 0.1f, color(25, 180, 25), "Monitorar",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::PREVIEW; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, 1.5f); });

			make_common_button(6.66667f, 1.0f, 0.0f, 0.4f, color(23, 180, 180), "Arquivo...",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::HOME_OPEN; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.5f); });

			make_common_button(6.66667f, 1.0f, 0.0f, 0.7f, color(150, 45, 45), "Sair",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.kill_all = true; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 2.5f); });

		}

		cout << console::color::DARK_GRAY << "Building HOME_OPEN sprites...";
		curr_set = stage_enum::HOME_OPEN;
		{
			shrd.screen_set[curr_set].min_y = 0.0f;
			shrd.screen_set[curr_set].max_y = 0.0f;

			shrd.casted_boys[stage_enum::HOME].csafe([&](const std::vector<hybrid_memory<sprite_pair>>& vec) {
				for (const auto& it : vec) {
					hybrid_memory<sprite_pair> cpy = it;
					shrd.casted_boys[curr_set].push_back(std::move(cpy));
				}
			});

			const float block_y = 0.425f;
			const float y_off_comp = 1.5f;

			const float pairs[4][2] = {
				{ 0.0f,    0.16f},
				{ 0.0f,    0.42f},
				{-0.425f,  0.68f},
				{ 0.425f,  0.68f},
			};
			

			make_color_box(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, color(0.1f, 0.1f, 0.1f, 0.75f),
				[](auto) {},
				[](auto, auto) {},
				[](auto) {});			

			make_color_box(0.85f, 0.42f, 0.0f, block_y, 1.0f, color(0.4f, 0.4f, 0.4f, 0.6f),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && !down.is_mouse_on_it) { shrd.screen = stage_enum::HOME; }},
				[&,block_y,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, block_y + y_off_comp); });
			
			make_common_button(7.50f, 1.0f, pairs[0][0], pairs[0][1], color(36, 170, 170), "Abrir imagem",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) {
				async_task([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Abra uma imagem para testar!", "*.jpg;*.png;*.bmp;*.jpeg", ALLEGRO_FILECHOOSER_FILE_MUST_EXIST | ALLEGRO_FILECHOOSER_PICTURES), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0);

					cout << console::color::AQUA << "[FILECHOOSER] Trying to open image: " << path;

					auto fpp = make_hybrid<file>();
					auto txtur = make_hybrid<texture>();

					if (!fpp->open(path.c_str(), file::open_mode_e::READ_TRY)) {
						al_show_native_message_box(nullptr, u8"Falha ao tentar ler", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
						return;
					}

					bool gud = txtur->load(fpp);

					if (!gud) {
						al_show_native_message_box(nullptr, u8"Falha ao tentar ler", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de carregar este arquivo como imagem:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
						return;
					}

					cout << console::color::AQUA << "[FILECHOOSER] Processing image...";

					shrd.latest_esp32_texture.replace_shared(std::move(txtur->duplicate())); // gonna be translated to VIDEO_BITMAP
					shrd.latest_esp32_texture_orig.replace_shared(std::move(txtur.reset_shared())); // this is MEMORY_BITMAP
					shrd.latest_esp32_file.replace_shared(std::move(fpp.reset_shared()));

					auto_handle_process_image(false);

					cout << console::color::AQUA << "[FILECHOOSER] Done.";

					shrd.screen = stage_enum::CONFIG;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[0][1] + y_off_comp); }, 1.0f, 0.10f);
			
			make_common_button(7.50f, 1.0f, pairs[1][0], pairs[1][1], color(140, 115, 36), "Caminho para salvar imagens",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) {
				async_task([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Onde devo salvar as fotos, se habilitado?", "*.*", ALLEGRO_FILECHOOSER_FOLDER), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0); // path/with/no/end

					shrd.conf.set("filemanagement", "export_path", path);
					shrd.conf.flush();

					al_show_native_message_box(nullptr, u8"Sucesso!", u8"Salvo!", (u8"As próximas imagens devem ser salvas neste caminho:\n" + path + u8"\n\nOBSERVAÇÃO: Você PRECISA habilitar o salvamento das fotos na aba de preview ou de configuração para as fotos serem salvas!").c_str(), nullptr, ALLEGRO_MESSAGEBOX_WARN);
					shrd.screen = stage_enum::HOME;

					cout << console::color::AQUA << "[FILECHOOSER] This is the path to save images now: " << path;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[1][1] + y_off_comp); }, 1.0f, 0.10f);

			make_common_button(3.25f, 1.0f, pairs[2][0], pairs[2][1], color(170, 75, 115), "Abrir config",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { 
				async_task([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Qual arquivo de configuração deseja importar?", "*.ini", ALLEGRO_FILECHOOSER_FILE_MUST_EXIST), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0);

					shrd.conf.flush(); // guaranteed last data saved

					if (!shrd.conf.load(path)) {
						shrd.conf.load(get_default_config_path()); // if bad reload good data
						al_show_native_message_box(nullptr, u8"Falha ao tentar ler", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
						return;
					}
					else {
						ensure_config_defaults();
						shrd.conf.save_path(get_default_config_path());
						shrd.conf.flush();
						al_show_native_message_box(nullptr, u8"Sucesso!", u8"Perfeito!", (u8"O arquivo abaixo foi importado com sucesso!\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_WARN);
						shrd.screen = stage_enum::HOME;
					}

					cout << console::color::AQUA << "[FILECHOOSER] Imported config: " << path;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[2][1] + y_off_comp); }, 1.0f, 0.10f);

			make_common_button(3.25f, 1.0f, pairs[3][0], pairs[3][1], color(100, 80, 170), "Exportar config",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) {
				async_task([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Onde deseja exportar uma cópia?", "*.ini", ALLEGRO_FILECHOOSER_SAVE), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0);

					shrd.conf.save_path(path);
					bool savegood = shrd.conf.flush();
					shrd.conf.save_path(get_default_config_path());

					if (!savegood) {
						al_show_native_message_box(nullptr, u8"Falha ao tentar escrever", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
						return;
					}
					else {
						al_show_native_message_box(nullptr, u8"Sucesso!", u8"Perfeito!", (u8"Exportei corretamente para o caminho abaixo!\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_WARN);
						shrd.screen = stage_enum::HOME;
					}

					cout << console::color::AQUA << "[FILECHOOSER] Exported config: " << path;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[3][1] + y_off_comp); }, 1.0f, 0.10f);

		}

		cout << console::color::DARK_GRAY << "Building CONFIG sprites...";
		curr_set = stage_enum::CONFIG;
		{
			shrd.screen_set[curr_set].min_y = 0.0f;
			shrd.screen_set[curr_set].max_y = 0.0f;
			

			const float offauto = 0.40f;
			const float offautox = 0.605f;
			// ================ AUTOSAVE BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.325f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.36f);
			blk->set<float>(enum_sprite_float_e::POS_X, offautox);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.3f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f); },
				curr_set
			));
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 0.745f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.37f);
			blk->set<float>(enum_sprite_float_e::POS_X, offautox);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.3f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[&, offautox](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {

						shrd.conf.set("filemanagement", "save_all", (down.cpy.real_posx > offautox));

						s->set<float>(enum_sprite_float_e::POS_X, offautox + (shrd.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
						s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
							0.4f + 0.6f * (!shrd.conf.get_as<bool>("filemanagement", "save_all")),
							0.4f + 0.6f * shrd.conf.get_as<bool>("filemanagement", "save_all"),
							0.4f));
					}
					else {
						if (shrd.conf.get_as<bool>("filemanagement", "save_all") && shrd.conf.get("filemanagement", "export_path").length() < 3) {
							shrd.conf.set("filemanagement", "save_all", false);
							async_task([&, s] {
								int res = al_show_native_message_box(nullptr, u8"Erro!", u8"Você ainda não tem um caminho para salvar as imagens!", u8"Quer que eu te leve ao lugar onde você pode configurar isso?\nVocê precisará clicar no botão para definir o caminho das imagens.", nullptr, ALLEGRO_MESSAGEBOX_YES_NO);
								s->set<float>(enum_sprite_float_e::POS_X, offautox + (shrd.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
								s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
									0.4f + 0.6f * (!shrd.conf.get_as<bool>("filemanagement", "save_all")),
									0.4f + 0.6f * shrd.conf.get_as<bool>("filemanagement", "save_all"),
									0.4f));
								if (res == 1) {
									shrd.screen = stage_enum::HOME_OPEN;
								}
							});
						}
					}
				},
				[&,offauto, offautox](sprite* s) {
					s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f);
					s->set<float>(enum_sprite_float_e::POS_X, offautox + (shrd.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.4f + 0.6f * (!shrd.conf.get_as<bool>("filemanagement", "save_all")),
						0.4f + 0.6f * shrd.conf.get_as<bool>("filemanagement", "save_all"),
						0.4f));
				},
				curr_set
			));
			make_text(0.075f, 1.0f, 1.0f, offautox - 0.17f, offauto - 0.028f, 3.0f, color(200, 200, 200), u8"Salvar cópias em disco?", { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
				[](auto) {},
				[](auto, auto) {},
				[offauto](sprite* s) {((text*)s)->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_RIGHT); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, s->get<float>(enum_sprite_float_e::POS_X)); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.301f); });


			// Image rendering
			make_blk();
			blk->texture_insert(shrd.latest_esp32_texture);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 1.5f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.7f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.17f);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			//blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.5f, 0.5f, 0.5f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(std::move(each)));

			make_common_button(3.5f, 1.0f, -0.7f, -0.85f, color(200, 25, 25), "Voltar",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::HOME; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); });

			make_common_button(4.0f, 0.65f, -0.5f, 0.59f, config_to_color(shrd.conf, "reference", "bad_plant"), "Definir cor como \"ruim\"",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					s->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(shrd.conf, "reference", "bad_plant"));
					if (down.is_unclick && down.is_mouse_on_it) {
						color_to_config(shrd.conf, "reference", "bad_plant", shrd.background_color);
						shrd.bad_plant = shrd.background_color;
						shrd.bad_perc = how_far(shrd.bad_plant, shrd.background_color);
						shrd.good_perc = how_far(shrd.good_plant, shrd.background_color);
					}
				},
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); }, 2.0f, 0.08f);

			make_common_button(4.0f, 0.65f, 0.5f, 0.59f, config_to_color(shrd.conf, "reference", "good_plant"), "Definir cor como \"boa\"",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					s->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(shrd.conf, "reference", "good_plant"));
					if (down.is_unclick && down.is_mouse_on_it) {
						color_to_config(shrd.conf, "reference", "good_plant", shrd.background_color);
						shrd.good_plant = shrd.background_color;
						shrd.bad_perc = how_far(shrd.bad_plant, shrd.background_color);
						shrd.good_perc = how_far(shrd.good_plant, shrd.background_color);
					}
				},
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, 1.5f); }, 2.0f, 0.08f);
			
			make_common_button(6.66667f, 1.0f, 0.0f, 0.84f, color(23, 180, 180), u8"Correção e ajuste global",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::CONFIG_FIX; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.5f); }, 1.75f);

			make_text(0.075f, 1.0f, 1.0f, 0.0f, -0.67f, 8.0f, color(200, 200, 200), u8"Pensando...", { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
				[&](sprite* s) {text* t = (text*)s; t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::to_string(shrd.good_perc * 100.0f) + "% bom / " + std::to_string(shrd.bad_perc * 100.0f) + "% ruim\nConsiderado bom? " + (shrd.good_perc >= shrd.bad_perc ? "SIM" : u8"NÃO")); },
				[](auto, auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, s->get<float>(enum_sprite_float_e::POS_X)); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, s->get<float>(enum_sprite_float_e::POS_Y)); });


			shrd.casted_boys[curr_set].safe([&](std::vector<hybrid_memory<sprite_pair>>& vec) {
				for (auto& ech : vec) ech->get_notick()->set<bool>(enum_sprite_boolean_e::DRAW_TRANSFORM_COORDS_KEEP_SCALE, true);
			});
		}

		cout << console::color::DARK_GRAY << "Building CONFIG_FIX sprites...";
		curr_set = stage_enum::CONFIG_FIX;
		{
			const float off_y = 0.74f;
			const float anim_y = 1.5f;

			shrd.screen_set[curr_set].min_y = 0.0f;
			shrd.screen_set[curr_set].max_y = off_y;
			

			shrd.casted_boys[stage_enum::CONFIG].csafe([&](const std::vector<hybrid_memory<sprite_pair>>& vec) {
				for (const auto& it : vec) {
					hybrid_memory<sprite_pair> cpy = it;
					shrd.casted_boys[curr_set].push_back(std::move(cpy));
				}
			});

			float offauto = 0.255f + off_y;

			make_color_box(0.89f, 0.63f, 0.0f, offauto, 1.0f, color(0.1f, 0.1f, 0.1f, 0.7f),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { 
					if (down.is_unclick && !down.is_mouse_on_it) { shrd.screen = stage_enum::CONFIG; }
					if (down.is_unclick && down.is_mouse_on_it) {
						if (shrd.latest_esp32_texture.valid() && !shrd.latest_esp32_texture->empty()) {
							auto_handle_process_image();
						}
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y - 0.75f); });


			offauto -= 0.455f;

			// ================ SATURATION BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			//blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
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
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.12f);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, txt->get<float>(enum_sprite_float_e::POS_X));
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, txt->get<float>(enum_sprite_float_e::POS_Y));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(u8"Saturação: +00% boost"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						u8"Saturação: +" + std::to_string((int)((shrd.conf.get_as<float>("processing", "saturation_compensation")) * 100.0f)) + "% boost"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ BRIGHTNESS BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
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
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.12f);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, txt->get<float>(enum_sprite_float_e::POS_X));
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, txt->get<float>(enum_sprite_float_e::POS_Y));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Brilho: +00% boost"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Brilho: +" + std::to_string((int)((shrd.conf.get_as<float>("processing", "brightness_compensation")) * 100.0f)) + "% boost"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ OVERFLOW BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
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
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.12f);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, txt->get<float>(enum_sprite_float_e::POS_X));
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, txt->get<float>(enum_sprite_float_e::POS_Y));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Ganho: +00%"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Ganho: +" + std::to_string((int)((shrd.conf.get_as<float>("processing", "overflow_boost") - 1.0f) * 100.0f)) + "%"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ CENTER WEIGHT BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
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
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.12f);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, txt->get<float>(enum_sprite_float_e::POS_X));
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, txt->get<float>(enum_sprite_float_e::POS_Y));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Peso central: x1"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Peso central: x" + std::to_string((int)(shrd.conf.get_as<float>("processing", "center_weight")))
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ X AXIS BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
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
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.12f);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, txt->get<float>(enum_sprite_float_e::POS_X));
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, txt->get<float>(enum_sprite_float_e::POS_Y));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Largura do centro: 00%"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Largura do centro: " + std::to_string((int)(shrd.conf.get_as<float>("processing", "area_center_x") * 100.0f)) + "%"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.18f;
			// ================ Y AXIS BAR
			// First bar config
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 7.3f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.5f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
			blk->set<float>(enum_sprite_float_e::SCALE_X, 1.0f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.52f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
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
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(dspy.src_font);
			txt->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			txt->set<float>(enum_sprite_float_e::SCALE_Y, 1.0f);
			txt->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			txt->set<float>(enum_sprite_float_e::POS_Y, offauto - 0.12f);
			txt->set<float>(enum_text_float_e::DRAW_UPDATES_PER_SEC, text_updates_per_sec);
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, txt->get<float>(enum_sprite_float_e::POS_X));
			txt->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, txt->get<float>(enum_sprite_float_e::POS_Y));
			txt->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
			txt->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("Altura do centro: 00%"));
			txt->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_CENTER);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Altura do centro: " + std::to_string((int)(shrd.conf.get_as<float>("processing", "area_center_y") * 100.0f)) + "%"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));
		}

		cout << console::color::DARK_GRAY << "Building PREVIEW sprites...";
		curr_set = stage_enum::PREVIEW;
		{
			shrd.screen_set[curr_set].min_y = 0.0f;
			shrd.screen_set[curr_set].max_y = 0.0f;

			make_color_box(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, color(0.1f, 0.1f, 0.1f, 0.6f));
			
			const float offauto = 0.44f;
			const float offautox = 0.605f;
			// ================ AUTOSAVE BAR
			// First bar config
			{
				make_blk();
				blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
				blk->set<float>(enum_sprite_float_e::SCALE_G, 0.22f);
				blk->set<float>(enum_sprite_float_e::SCALE_X, 1.325f);
				blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.36f);
				blk->set<float>(enum_sprite_float_e::POS_X, offautox);
				blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
				blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
				blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
				blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.3f);
				blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(125, 125, 125));
				blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
				blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
				shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
					std::move(each),
					[](auto) {},
					[](auto, auto) {},
					[offauto](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f); },
					curr_set
				));
				make_blk();
				blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_UP]);
				blk->texture_insert(shrd.texture_map[textures_enum::BUTTON_DOWN]);
				blk->set<float>(enum_sprite_float_e::SCALE_G, 0.2f);
				blk->set<float>(enum_sprite_float_e::SCALE_X, 0.745f);
				blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.37f);
				blk->set<float>(enum_sprite_float_e::POS_X, offautox);
				blk->set<float>(enum_sprite_float_e::POS_Y, offauto);
				blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
				blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
				blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.3f);
				blk->set<color>(enum_sprite_color_e::DRAW_TINT, color(200, 200, 200));
				blk->set<bool>(enum_sprite_boolean_e::DRAW_USE_COLOR, true);
				blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
				shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
					std::move(each),
					[](auto) {},
					[&, offautox](sprite* s, const sprite_pair::cond& down) {
						((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
						if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {

							shrd.conf.set("filemanagement", "save_all", (down.cpy.real_posx > offautox));

							s->set<float>(enum_sprite_float_e::POS_X, offautox + (shrd.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
							s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
								0.4f + 0.6f * (!shrd.conf.get_as<bool>("filemanagement", "save_all")),
								0.4f + 0.6f * shrd.conf.get_as<bool>("filemanagement", "save_all"),
								0.4f));
						}
						else {
							if (shrd.conf.get_as<bool>("filemanagement", "save_all") && shrd.conf.get("filemanagement", "export_path").length() < 3) {
								shrd.conf.set("filemanagement", "save_all", false);
								async_task([&, s] {
									int res = al_show_native_message_box(nullptr, u8"Erro!", u8"Você ainda não tem um caminho para salvar as imagens!", u8"Quer que eu te leve ao lugar onde você pode configurar isso?\nVocê precisará clicar no botão para definir o caminho das imagens.", nullptr, ALLEGRO_MESSAGEBOX_YES_NO);
									s->set<float>(enum_sprite_float_e::POS_X, offautox + (shrd.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
									s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
										0.4f + 0.6f * (!shrd.conf.get_as<bool>("filemanagement", "save_all")),
										0.4f + 0.6f * shrd.conf.get_as<bool>("filemanagement", "save_all"),
										0.4f));
									if (res == 1) {
										shrd.screen = stage_enum::HOME_OPEN;
									}
								});
							}
						}
					},
					[&,offauto, offautox](sprite* s) {
						s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f);
						s->set<float>(enum_sprite_float_e::POS_X, offautox + (shrd.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
						s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
							0.4f + 0.6f * (!shrd.conf.get_as<bool>("filemanagement", "save_all")),
							0.4f + 0.6f * shrd.conf.get_as<bool>("filemanagement", "save_all"),
							0.4f));
					},
					curr_set
				));
				make_text(0.075f, 1.0f, 1.0f, offautox - 0.17f, offauto - 0.028f, 3.0f, color(200, 200, 200), u8"Salvar cópias em disco?", { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
					[](auto) {},
					[](auto, auto) {},
					[offauto](sprite* s) {((text*)s)->set<int>(enum_text_integer_e::DRAW_ALIGNMENT, ALLEGRO_ALIGN_RIGHT); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, s->get<float>(enum_sprite_float_e::POS_X)); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.301f); });
			}

			// Image rendering
			make_blk();
			blk->texture_insert(shrd.latest_esp32_texture);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 1.5f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.7f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.13f);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.5f, 0.5f, 0.5f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			shrd.casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(std::move(each)));

			make_common_button(3.5f, 1.0f, -0.7f, -0.85f, color(200, 25, 25), "Voltar",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { shrd.screen = stage_enum::HOME; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); });

			make_text(0.09f, 1.0f, 1.0f, 0.0f, 0.62f, 8.0f, color(200, 200, 200), u8"Pensando...", { text_shadow{ 0.004f,0.05f,color(0,0,0) } },
				[&](sprite* s) {
					text* t = (text*)s;
					if (shrd.good_perc == 0.0f && shrd.bad_perc == 0.0f && espp.status == _esp32_communication::package_status::NON_CONNECTED) {
						std::string a = ".";
						for (size_t p = 0; p < (static_cast<size_t>(1.5 * al_get_time()) % 3); p++) a += '.';
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(u8"\nAguardando conexão" + a));
					}
					else {
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::to_string(shrd.good_perc * 100.0f) + "% bom / " + std::to_string(shrd.bad_perc * 100.0f) + "% ruim\nConsiderado bom? " + (shrd.good_perc >= shrd.bad_perc ? "SIM" : u8"NÃO"));
					}
				},
				[](auto, auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, s->get<float>(enum_sprite_float_e::POS_X)); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 0.85f); });

			

			make_common_button(15.0f, 0.85f, 0.0f, 1.095f, color(25, 150, 65), u8"Não conectado",
				[&](sprite* s) {
					if (espp.status == _esp32_communication::package_status::NON_CONNECTED) {
						s->set<color>(enum_sprite_color_e::DRAW_TINT, color(80, 15, 15));
						s->set<float>(enum_sprite_float_e::POS_Y, 1.095f);
						s->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.125f);
					}
					else {
						s->set<color>(enum_sprite_color_e::DRAW_TINT, color(35, 100, 150));
						s->set<float>(enum_sprite_float_e::POS_Y, 0.975f);
						s->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.03f);
					}
				},
				[&](auto,auto){},
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.095f); }, 30.0f, 0.09f,
				[&](sprite* s) {
					text* t = (text*)s;
					if (espp.status == _esp32_communication::package_status::NON_CONNECTED) {
						s->set<float>(enum_sprite_float_e::POS_Y, 1.03f);
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(u8"Não conectado"));
						s->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.125f);
					}
					else {
						s->set<float>(enum_sprite_float_e::POS_Y, 0.91f);
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(u8"<- \"bom\" --   -- \"ruim\" ->"));
						s->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 0.03f);
					}
				});


			make_common_button(3.0f, 0.7f, -0.7f, 1.2f, color(25, 150, 65), "---.--- kg",
				[&](sprite* s) {
					if (espp.status == _esp32_communication::package_status::NON_CONNECTED) s->set<float>(enum_sprite_float_e::POS_Y, 1.07f);
					else																	s->set<float>(enum_sprite_float_e::POS_Y, 0.935f);
				},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { espp.ask_weight_when_time = al_get_time() + 1.0; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.07f); }, 15.0f, 0.12f,
				[&](sprite* s) {
					text* t = (text*)s;
					s->set<float>(enum_sprite_float_e::POS_Y, 0.935f - 0.12f * 0.45f);
					if (espp.status == _esp32_communication::package_status::NON_CONNECTED) {
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("offline"));
						s->set<float>(enum_sprite_float_e::POS_Y, 1.07f - 0.12f * 0.45f);
					}
					else if ((al_get_time() + 1.0) > espp.ask_weight_when_time) {
						std::string a = ".";
						for (size_t p = 0; p < (static_cast<size_t>(3.0 * al_get_time()) % 3); p++) a += '.';
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, a);
					}
					else {
						char formattt[40];
						sprintf_s(formattt, "%07.3f kg", espp.current_good);
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(formattt));
					}
				});
			
			make_common_button(3.0f, 0.7f, 0.7f, 1.2f, color(150, 25, 65), "---.--- kg",
				[&](sprite* s) {
					if (espp.status == _esp32_communication::package_status::NON_CONNECTED) s->set<float>(enum_sprite_float_e::POS_Y, 1.07f);
					else																	s->set<float>(enum_sprite_float_e::POS_Y, 0.935f);
				},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { espp.ask_weight_when_time = al_get_time() + 1.0; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.07f); }, 15.0f, 0.12f,
				[&](sprite* s) {
					text* t = (text*)s;
					s->set<float>(enum_sprite_float_e::POS_Y, 0.935f - 0.12f * 0.45f);
					if (espp.status == _esp32_communication::package_status::NON_CONNECTED) {
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("offline"));
						s->set<float>(enum_sprite_float_e::POS_Y, 1.07f - 0.12f * 0.45f);
					}
					else if ((al_get_time() + 1.0) > espp.ask_weight_when_time) {
						std::string a = ".";
						for (size_t p = 0; p < (static_cast<size_t>(3.0 * al_get_time()) % 3); p++) a += '.';
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, a);
					}
					else {
						char formattt[40];
						sprintf_s(formattt, "%07.3f kg", espp.current_bad);
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(formattt));
					}
				});

		}
	}
	post_progress_val(0.90f);
	cout << console::color::DARK_GRAY << "Starting esp32 communication...";

	if (!start_esp32_threads()) {
		cout << console::color::DARK_RED << "Failed to setup host! That's sad.";
		full_close("Can't start host to ESP32 connection!");
		return false;
	}

	cout << console::color::DARK_GRAY << "Creating processing task...";
	dspy.updatthr.task_async([&] {
		shrd.casted_boys[shrd.screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				i->get_think()->think();
			}
			});
		{
			shrd.wifi_blk.get_think()->think();
			shrd.mouse_pair.get_think()->think();
		}
	}, thread::speed::INTERVAL, 1.0/20);

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
	dspy.disp.post_task([&] {
		transform transf;
		transf.build_classic_fixed_proportion_auto(1.0f);
		transf.translate_inverse(0.0f, shrd.current_offy);
		//shrd.wifi_blk.set<float>(enum_sprite_float_e::POS_Y, -0.85f + shrd.current_offy);
		transf.apply();
		return true;
	});
}

void CoreWorker::_post_update_display_one_o_one()
{
	dspy.disp.post_task([] {
		transform transf;
		transf.identity();
		transf.apply();
		return true;
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
		int result = ev.display.height < ev.display.width ? ev.display.height : ev.display.width;
		if (result < 200) result = 200;
		dspy.disp.post_task([result, this] { return al_resize_display(dspy.disp.get_raw_display(), result, result); });
		shrd.conf.set("display", "size_length", result);
		post_update_display_auto();
	}
	break;
	}
}

void CoreWorker::handle_mouse_event(const int tp, const mouse::mouse_event& ev)
{
	shrd.last_mouse_movement = al_get_time();
	if (shrd.kill_all) return;
	block* mous = (block*)shrd.mouse_pair.get_notick();
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

		shrd.casted_boys[shrd.screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				if (!i->does_work_on(shrd.screen)) continue;
				collisionable thus(*i->get_notick());
				thus.reset();
				sprite_pair::cond cond;

				cond.is_click = false;
				cond.is_unclick = false;
				cond.is_mouse_on_it = thus.quick_one_sprite_overlap(*mous).dir_to != 0;
				cond.is_mouse_pressed = ev.buttons_pressed != 0;
				cond.cpy = ev;

				i->update(cond);
			}
		});
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
		shrd.casted_boys[shrd.screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				if (!i->does_work_on(shrd.screen)) continue;
				collisionable thus(*i->get_notick());
				thus.reset();
				sprite_pair::cond cond;

				cond.is_click = true;
				cond.is_unclick = false;
				cond.is_mouse_on_it = thus.quick_one_sprite_overlap(*mous).dir_to != 0;
				cond.is_mouse_pressed = true;
				cond.cpy = ev;

				i->update(cond);
			}
		});
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
		shrd.casted_boys[shrd.screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				if (!i->does_work_on(shrd.screen)) continue;
				collisionable thus(*i->get_notick());
				thus.reset();
				sprite_pair::cond cond;

				cond.is_click = false;
				cond.is_unclick = true;
				cond.is_mouse_on_it = thus.quick_one_sprite_overlap(*mous).dir_to != 0;
				cond.is_mouse_pressed = false;
				cond.cpy = ev;

				i->update(cond);
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

	//shrd.async_queue.signal_stop();
	if (shrd.on_throw_thr.id && !shrd.on_throw_thr.has_ended()) shrd.on_throw_thr.ended.get();
	dspy.updatthr.signal_stop();

	// _esp32_communication
	cout << console::color::YELLOW << "- ESP32 COMMUNICATION -";

	post_progress_val(0.10f);

	espp.close_all();

	post_progress_val(0.25f);

	// _display (textures & font)
	cout << console::color::YELLOW << "- DISPLAY (textures and font) -";

	dspy.updatthr.join();
	dspy.disp.post_task([&] {
		for (auto& i : shrd.texture_map) {
			if (i.store.valid()) i.store->destroy();
		}
		if (dspy.src_atlas.valid()) dspy.src_atlas->destroy();
		if (dspy.src_font.valid()) dspy.src_font->destroy();
		if (shrd.latest_esp32_texture.valid()) shrd.latest_esp32_texture->destroy();
		return true;
	}).get();

	post_progress_val(0.52f);

	// _shared
	cout << console::color::YELLOW << "- SHARED -";

	shrd.mouse_work.unhook_event();
	shrd.screen = stage_enum::HOME;
	shrd.file_font.reset_this();
	shrd.file_atlas.reset_this();
	if (shrd.latest_esp32_texture_orig.valid()) shrd.latest_esp32_texture_orig->destroy();

	post_progress_val(0.70f);

	for(auto& i : shrd.casted_boys) {
		i.second.safe([](std::vector<hybrid_memory<sprite_pair>>& vec) {
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

void CoreWorker::draw()
{
	const bool curr_modb = dspy.display_default_draw;
	if (dspy.display_default_draw) {
		shrd.background_color.clear_to_this();

		shrd.casted_boys[shrd.screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				i->get_notick()->draw();
			}
			});

		{
			shrd.wifi_blk.get_notick()->draw();
			shrd.mouse_pair.get_notick()->draw();
		}
	}
	else {
		const float min_x = dspy.disp.get_width() * 0.2;
		const float max_x = dspy.disp.get_width() * 0.8;
		const float min_y = dspy.disp.get_height() * 0.75;
		const float max_y = dspy.disp.get_height() * 0.8;
		const float& cp = shrd.generic_progressbar;// (shrd.generic_progressbar < 0.0f ? 0.0 : (shrd.generic_progressbar > 1.0f ? 1.0f : shrd.generic_progressbar));

		const float x_off = min_x + static_cast<float>(max_x - min_x) * cp;

		const float xoff0 = limit_range_of(x_off + static_cast<float>(2.5 * cos(3.011 * al_get_time() + 0.532)), min_x, max_x);
		const float xoff1 = limit_range_of(x_off + static_cast<float>(2.5 * sin(4.135 * al_get_time() + 2.114)), min_x, max_x);


		ALLEGRO_VERTEX vf[] = {
			ALLEGRO_VERTEX{min_x, min_y, 0, 0, 0, color(0.25f + 0.05f * cp, 0.25f + 0.05f * cp, 0.25f + 0.05f * cp) },
			ALLEGRO_VERTEX{max_x, min_y, 0, 0, 0, color(0.25f + 0.05f * cp, 0.25f + 0.05f * cp, 0.25f + 0.05f * cp) },
			ALLEGRO_VERTEX{max_x, max_y, 0, 0, 0, color(0.15f + 0.05f * cp, 0.15f + 0.05f * cp, 0.15f + 0.05f * cp) },
			ALLEGRO_VERTEX{min_x, max_y, 0, 0, 0, color(0.15f + 0.05f * cp, 0.15f + 0.05f * cp, 0.15f + 0.05f * cp) }
		};
		ALLEGRO_VERTEX v1[] = {
			ALLEGRO_VERTEX{min_x, min_y, 0, 0, 0, color(0.65f - 0.3f * cp, 0.65f - 0.1f * cp, 0.65f - 0.3f * cp) },
			ALLEGRO_VERTEX{xoff0, min_y, 0, 0, 0, color(0.73f            , 0.73f + 0.5f * cp, 0.73f) },
			ALLEGRO_VERTEX{xoff1, max_y, 0, 0, 0, color(0.43f            , 0.43f + 0.5f * cp, 0.43f) },
			ALLEGRO_VERTEX{min_x, max_y, 0, 0, 0, color(0.35f - 0.3f * cp, 0.35f - 0.1f * cp, 0.35f - 0.3f * cp) }
		};

		color(0, 0, 0).clear_to_this();
		int indices1[] = { 0, 1, 2, 3 };
		al_draw_indexed_prim((void*)vf, nullptr, nullptr, indices1, 4, ALLEGRO_PRIM_TRIANGLE_FAN);
		al_draw_indexed_prim((void*)v1, nullptr, nullptr, indices1, 4, ALLEGRO_PRIM_TRIANGLE_FAN);
	}
	dspy.display_reflected_draw = curr_modb;
}

CoreWorker::CoreWorker()
	: shrd(dspy.disp)
{
}

bool CoreWorker::work()
{
	cout << "On load.";
	timed_bomb bmb([] {
		al_show_native_message_box(nullptr, "[A.S.S.] Demorei muito para abrir!", "Esta mensagem aparece num timeout para garantir que você consiga fechar o app caso trave", "Clique em qualquer coisa para fechar.", nullptr, ALLEGRO_MESSAGEBOX_ERROR);
		std::terminate();
	}, 20.0);
	if (!full_load()) return false;
	bmb.defuse();
	return true;
}

void CoreWorker::yield()
{
	cout << "On yield.";
	while (!shrd.kill_all) std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void CoreWorker::close()
{
	cout << "On close.";
	timed_bomb bmb([] {
		al_show_native_message_box(nullptr, "[A.S.S.] Demorei muito para fechar!", "Esta mensagem aparece num timeout para garantir que você consiga fechar o app caso trave", "Clique em qualquer coisa para fechar.", nullptr, ALLEGRO_MESSAGEBOX_ERROR);
		std::terminate();
	}, 20.0);
	full_close();
	bmb.defuse();
	cout << "Bye.";
}