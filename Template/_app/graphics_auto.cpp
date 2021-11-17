#include "graphics_auto.h"

void GraphicInterface::draw()
{
	const auto _mod = stor.camera_ack();

	if (update_display.exchange(false)) {
		transform transf;
		switch (_mod) {
		case Storage::camera_mode::DEFAULT_CAMERA:
			transf.build_classic_fixed_proportion_auto(1.0f);
			transf.translate_inverse(0.0f, current_offy);
			transf.apply();
			break;
		case Storage::camera_mode::PROGRESS_BAR_NO_RESOURCE:
			transf.identity();
			transf.apply();
			break;
		}
	}

	switch (_mod) {
	case Storage::camera_mode::DEFAULT_CAMERA:
		stor.background_color.clear_to_this();

		casted_boys[screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				i->get_notick()->draw();
			}
		});

		{
			wifi_blk.get_notick()->draw();
			mouse_pair.get_notick()->draw();
		}
		break;
	case Storage::camera_mode::PROGRESS_BAR_NO_RESOURCE:
	{
		const float min_x = disp.get_width() * 0.2;
		const float max_x = disp.get_width() * 0.8;
		const float min_y = disp.get_height() * 0.75;
		const float max_y = disp.get_height() * 0.8;
		const float cp = limit_range_of(stor.generic_progressbar, 0.0f, 1.0f);

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
		break;
	}
}

void GraphicInterface::handle_display_event(const display_event& ev)
{
	switch(ev.get_type()) {
	case ALLEGRO_EVENT_DISPLAY_CLOSE:
		on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });
		break;
	case ALLEGRO_EVENT_DISPLAY_RESIZE:
	{
		int result = ev.as_display().height < ev.as_display().width ? ev.as_display().height : ev.as_display().width;
		if (result < 200) result = 200;
		disp.post_task([result, this] { return al_resize_display(disp.get_raw_display(), result, result); });
		stor.conf.set("display", "size_length", result);
		update_display = true;
	}
	break;
	}
}

void GraphicInterface::handle_mouse_event(const int tp, const mouse::mouse_event& ev)
{
	if (disp.empty()) return;

	block* mous = (block*)mouse_pair.get_notick();
	if (!mous) return;

	switch (tp) {
	case ALLEGRO_EVENT_MOUSE_AXES:
		mous->set<float>(enum_sprite_float_e::POS_X, ev.real_posx);
		mous->set<float>(enum_sprite_float_e::POS_Y, ev.real_posy);

		if (ev.scroll_event_id(1)) {
			if (ev.scroll_event_id(1) > 0) current_offy -= 0.25f;
			else current_offy += 0.25f;
			update_display = true;
		}

		casted_boys[screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				if (!i->does_work_on(screen)) continue;
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
		casted_boys[screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				if (!i->does_work_on(screen)) continue;
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
		casted_boys[screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				if (!i->does_work_on(screen)) continue;
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

	if (current_offy < screen_set[screen].min_y) {
		current_offy = screen_set[screen].min_y;
		update_display = true;
	}
	else if (current_offy > screen_set[screen].max_y) {
		current_offy = screen_set[screen].max_y;
		update_display = true;
	}
}

void GraphicInterface::auto_handle_process_image(const bool asyncr)
{
	if (stor.latest_esp32_texture_orig.empty()) return;

	const auto ff = [&] {
		stor.background_color = process_image(*stor.latest_esp32_texture_orig, stor.conf);
		stor.bad_perc = how_far(stor.bad_color, stor.background_color);
		stor.good_perc = how_far(stor.good_color, stor.background_color);
	};

	if (asyncr) async.launch(ff);
	else ff();
}

GraphicInterface::GraphicInterface(Storage& s, Communication& c)
	: stor(s), comm(c), dispev(disp), mouse_work(disp), mouse_pair(make_hybrid_derived<sprite, block>()), wifi_blk(make_hybrid_derived<sprite, block>())
{
	c.set_on_file([this](hybrid_memory<file>&& ff) -> int {
		auto txtur = make_hybrid<texture>();
		bool gud = txtur->load(ff);

		if (!gud) {
			cout << console::color::RED << "[IMGPROC] Could not load the file as texture! ABORT!";
			return -1;
		}

		if (stor.conf.get_as<bool>("filemanagement", "save_all")) {
			cout << console::color::AQUA << "[CLIENT] Saving file...";
			const std::string path = stor.conf.get("filemanagement", "export_path");

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
					async.launch([path] {
						al_show_native_message_box(nullptr, u8"Falha ao salvar imagem!", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de escrever no caminho para salvar a imagem:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
					});
				}
				else {
					cout << console::color::AQUA << "[CLIENT] Saved image! (/" << (lul + '-' + std::string(decc) + ".jpg") << ")";
				}
			}
			else {
				stor.conf.set("filemanagement", "save_all", false);
				async.launch([path] {
					al_show_native_message_box(nullptr, u8"O caminho se tornou inválido?!", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho para salvar a imagem:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
				});
			}
		}

		cout << console::color::AQUA << "[CLIENT] Working on image...";

		latest_esp32_texture.replace_shared(std::move(txtur->duplicate())); // gonna be translated to VIDEO_BITMAP
		stor.latest_esp32_texture_orig.replace_shared(std::move(txtur.reset_shared())); // this is MEMORY_BITMAP
		stor.latest_esp32_download_file.replace_shared(std::move(ff.reset_shared()));

		auto_handle_process_image(false); // has to wait
		return stor.good_perc >= stor.bad_perc ? 1 : 0;
	});
}

bool GraphicInterface::start(std::function<void(void)> qui)
{
	on_quit_f = qui;

	if (!disp.create(
		display_config()
		.set_display_mode(
			display_options()
			.set_width(stor.conf.get_as<int>("display", "size_length"))
			.set_height(stor.conf.get_as<int>("display", "size_length"))
		)
		.set_extra_flags(ALLEGRO_RESIZABLE | ALLEGRO_OPENGL/* | ALLEGRO_NOFRAME*/)
		.set_window_title(get_app_name())
		.set_fullscreen(false)
		.set_use_basic_internal_event_system(true)
		.set_framerate_limit(max_fps)
		.set_vsync(true)
		.set_wait_for_display_draw(true)
	)) {
		cout << console::color::DARK_RED << "Failed creating display.";
		on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });
		return false;
	}

	disp.hook_draw_function([this](auto&) { draw(); });
	disp.hook_exception_handler([](const std::exception& e) { cout << console::color::DARK_RED << "Exception @ drawing thread: " << e.what(); });

	stor.set_graphic_perc(0.0f);
	stor.set_camera(Storage::camera_mode::PROGRESS_BAR_NO_RESOURCE, true);
	update_display = true;
	stor.set_graphic_perc(0.01f);

	disp.set_icon_from_icon_resource(IDI_ICON1);
	stor.set_graphic_perc(0.05f);

	dispev.hook_event_handler([this](const display_event& ev) {handle_display_event(ev); });
	mouse_work.hook_event([&](const int tp, const mouse::mouse_event& me) {handle_mouse_event(tp, me); });
	stor.set_graphic_perc(0.10f);

	cout << console::color::DARK_GRAY << "Hiding mouse...";
	disp.post_task([&] { return al_hide_mouse_cursor(disp.get_raw_display()); });
	cout << console::color::DARK_GRAY << "Loading font resource...";

	stor.file_font = make_hybrid_derived<file, memfile>();
	*((memfile*)stor.file_font.get()) = get_executable_resource_as_memfile(IDR_FONT1, resource_type_e::FONT);
	if (((memfile*)stor.file_font.get())->size() == 0) {
		cout << console::color::DARK_RED << "Failed loading font resource.";
		on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });
		return false;
	}
	stor.set_graphic_perc(0.20f);

	cout << console::color::DARK_GRAY << "Loading atlas resource...";

	stor.file_atlas = make_hybrid_derived<file, memfile>();
	*((memfile*)stor.file_atlas.get()) = get_executable_resource_as_memfile(IDB_PNG1, (WinString)L"PNG");
	if (((memfile*)stor.file_atlas.get())->size() == 0) {
		cout << console::color::DARK_RED << "Failed loading atlas resource.";
		on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });
		return false;
	}
	stor.set_graphic_perc(0.40f);

	cout << console::color::DARK_GRAY << "Linking font resource...";

	src_font = make_hybrid<font>();
	if (!src_font->load(font_config().set_file(stor.file_font).set_is_ttf(true).set_resolution(150))) {
		cout << console::color::DARK_RED << "Failed linking font resource.";
		on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });
		return false;
	}
	stor.set_graphic_perc(0.45f);

	cout << console::color::DARK_GRAY << "Linking atlas resource...";

	src_atlas = make_hybrid<texture>();
	if (!src_atlas->load(texture_config().set_file(stor.file_atlas).set_flags(ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR))) {
		cout << console::color::DARK_RED << "Failed linking atlas resource.";
		on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });
		return false;
	}
	stor.set_graphic_perc(0.50f);

	cout << console::color::DARK_GRAY << "Creating sub-bitmaps...";

	texture_map[textures_enum::MOUSE_LOADING]		= make_hybrid<texture>(src_atlas->create_sub(0, 0, 200, 200));

	texture_map[textures_enum::MOUSE]				= make_hybrid<texture>(src_atlas->create_sub(0, 200, 200, 200));
	texture_map[textures_enum::BUTTON_UP]			= make_hybrid<texture>(src_atlas->create_sub(350, 0, 600, 90));
	texture_map[textures_enum::BUTTON_DOWN]			= make_hybrid<texture>(src_atlas->create_sub(350, 90, 600, 90));

	texture_map[textures_enum::WIFI_SEARCHING]		= make_hybrid<texture>(src_atlas->create_sub(350, 180, 100, 100));
	texture_map[textures_enum::WIFI_IDLE]			= make_hybrid<texture>(src_atlas->create_sub(450, 180, 100, 100));
	texture_map[textures_enum::WIFI_TRANSFER]		= make_hybrid<texture>(src_atlas->create_sub(550, 180, 100, 100));
	texture_map[textures_enum::WIFI_THINKING]		= make_hybrid<texture>(src_atlas->create_sub(650, 180, 100, 100));
	texture_map[textures_enum::WIFI_COMMAND]		= make_hybrid<texture>(src_atlas->create_sub(750, 180, 100, 100));

	latest_esp32_texture = make_hybrid<texture>(); // just so it gets replaced later

	stor.set_graphic_perc(0.75f);
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
		

		const auto make_button = [&boys = casted_boys, &font = src_font]
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

		const auto make_common_button = [&boys = casted_boys, &font = src_font, &txture_map = texture_map, &make_button, &curr_set](const float width, const float height, const float px, const float py, const color boxclr, const std::string& txtt,
			const std::function<void(sprite*)> ftick = [](auto){}, const std::function<void(sprite*, const sprite_pair::cond&)> fclick = [](auto,auto){}, const std::function<void(sprite*)> rstf = [](auto){}, const float animspeed = 4.0f, const float font_siz = 0.12f,
			const std::function<void(sprite*)> ftxttick = [](auto) {}) {
			make_button(curr_set, { txture_map[textures_enum::BUTTON_UP], txture_map[textures_enum::BUTTON_DOWN] }, 0.2f, width, height, px, py, boxclr, color(200, 200, 200), font_siz, txtt, animspeed, ftick, [fclick](sprite* s, const sprite_pair::cond& v) { ((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (v.is_mouse_on_it && v.is_mouse_pressed) ? 1 : 0); fclick(s, v); }, rstf, ftxttick);
		};
		const auto make_color_box = [&boys = casted_boys, &make_button, &curr_set](const float width = 1.0f, const float height = 1.0f, const float px = 0.0f, const float py = 0.0f, const float smoothness = 0.0f, const color boxclr = color(0.1f, 0.1f, 0.1f, 0.6f),
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
		const auto make_text = [&boys = casted_boys, &font = src_font, &curr_set](const float siz, const float sx, const float sy, const float px, const float py, const float smoothness, const color txtclr, const std::string& txtstr, const std::initializer_list<text_shadow> tshad = {},
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
			blk = (block*)mouse_pair.get_notick();
			blk->texture_insert(texture_map[textures_enum::MOUSE]);
			blk->texture_insert(texture_map[textures_enum::MOUSE_LOADING]);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 0.08f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, 0.0f);
			//blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 4.0f);
			blk->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			mouse_pair.set_tick([&](sprite* raw) {
				block* bl = (block*)raw;
				bl->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, async.tasking() ? 1 : 0);
				bl->set<float>(enum_sprite_float_e::ROTATION, async.tasking() ? static_cast<float>(al_get_time() * 3.0) : 0.0f);
			});

						
			((block*)wifi_blk.get_notick())->texture_insert(texture_map[textures_enum::WIFI_SEARCHING]);
			((block*)wifi_blk.get_notick())->texture_insert(texture_map[textures_enum::WIFI_IDLE]);
			((block*)wifi_blk.get_notick())->texture_insert(texture_map[textures_enum::WIFI_TRANSFER]);
			((block*)wifi_blk.get_notick())->texture_insert(texture_map[textures_enum::WIFI_THINKING]);
			((block*)wifi_blk.get_notick())->texture_insert(texture_map[textures_enum::WIFI_COMMAND]);
			((block*)wifi_blk.get_notick())->set<float>(enum_sprite_float_e::SCALE_G, 0.17f);
			((block*)wifi_blk.get_notick())->set<float>(enum_sprite_float_e::POS_X, 0.89f);
			((block*)wifi_blk.get_notick())->set<float>(enum_sprite_float_e::POS_Y, -0.89f);
			((block*)wifi_blk.get_notick())->set<float>(enum_sprite_float_e::DRAW_MOVEMENT_RESPONSIVENESS, 4.0f);
			((block*)wifi_blk.get_notick())->set<bool>(enum_sprite_boolean_e::DRAW_TRANSFORM_COORDS_KEEP_SCALE, true);
			((block*)wifi_blk.get_notick())->set<bool>(enum_block_bool_e::DRAW_SET_FRAME_VALUE_READONLY, true);
			wifi_blk.set_tick([this](sprite* s) {

				int corr = static_cast<int>(((block*)s)->get<size_t>(enum_block_sizet_e::RO_DRAW_FRAME));
				corr += static_cast<int>(textures_enum::__FIRST_WIFI);
				if (corr > static_cast<int>(textures_enum::__LAST_WIFI)) corr = -1;

				switch (stor.wifi_status) {

				case Storage::conn_status::NON_CONNECTED:
					((block*)wifi_blk.get_notick())->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(textures_enum::WIFI_SEARCHING) - static_cast<size_t>(textures_enum::__FIRST_WIFI));
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.35 * cos(1.5 * al_get_time() + 0.8 * sin(al_get_time() * 1.2554 + 0.3 * ((random() % 1000) / 1000.0)))));
					break;
				case Storage::conn_status::IDLE:
					((block*)wifi_blk.get_notick())->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(textures_enum::WIFI_IDLE) - static_cast<size_t>(textures_enum::__FIRST_WIFI));
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.25 * cos(0.7 * al_get_time())));
					break;
				case Storage::conn_status::DOWNLOADING_IMAGE:
					((block*)wifi_blk.get_notick())->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(textures_enum::WIFI_TRANSFER) - static_cast<size_t>(textures_enum::__FIRST_WIFI));
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(0.25 * cos(4.5 * al_get_time())));
					break;
				case Storage::conn_status::PROCESSING_IMAGE:
					((block*)wifi_blk.get_notick())->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(textures_enum::WIFI_THINKING) - static_cast<size_t>(textures_enum::__FIRST_WIFI));
					s->set<float>(enum_sprite_float_e::ROTATION, static_cast<float>(al_get_time() * 3.0));
					break;
				case Storage::conn_status::COMMANDING:
					((block*)wifi_blk.get_notick())->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, static_cast<size_t>(textures_enum::WIFI_COMMAND) - static_cast<size_t>(textures_enum::__FIRST_WIFI));
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
			screen_set[curr_set].min_y = 0.0f;
			screen_set[curr_set].max_y = 0.0f;

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
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { screen = stage_enum::CONFIG; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); });

			make_common_button(2.97f, 1.0f, 0.37f, 0.1f, color(25, 180, 25), "Monitorar",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { screen = stage_enum::PREVIEW; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, 1.5f); });

			make_common_button(6.66667f, 1.0f, 0.0f, 0.4f, color(23, 180, 180), "Arquivo...",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { screen = stage_enum::HOME_OPEN; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.5f); });

			make_common_button(6.66667f, 1.0f, 0.0f, 0.7f, color(150, 45, 45), "Sair",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); }); } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 2.5f); });

		}

		cout << console::color::DARK_GRAY << "Building HOME_OPEN sprites...";
		curr_set = stage_enum::HOME_OPEN;
		{
			screen_set[curr_set].min_y = 0.0f;
			screen_set[curr_set].max_y = 0.0f;

			casted_boys[stage_enum::HOME].csafe([&](const std::vector<hybrid_memory<sprite_pair>>& vec) {
				for (const auto& it : vec) {
					hybrid_memory<sprite_pair> cpy = it;
					casted_boys[curr_set].push_back(std::move(cpy));
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
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && !down.is_mouse_on_it) { screen = stage_enum::HOME; }},
				[&,block_y,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, block_y + y_off_comp); });
			
			make_common_button(7.50f, 1.0f, pairs[0][0], pairs[0][1], color(36, 170, 170), "Abrir imagem",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) {
				async.launch([&] {
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

					latest_esp32_texture.replace_shared(std::move(txtur->duplicate())); // gonna be translated to VIDEO_BITMAP
					stor.latest_esp32_texture_orig.replace_shared(std::move(txtur.reset_shared())); // this is MEMORY_BITMAP
					stor.latest_esp32_download_file.replace_shared(std::move(fpp.reset_shared()));

					auto_handle_process_image(false);

					cout << console::color::AQUA << "[FILECHOOSER] Done.";

					screen = stage_enum::CONFIG;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[0][1] + y_off_comp); }, 1.0f, 0.10f);
			
			make_common_button(7.50f, 1.0f, pairs[1][0], pairs[1][1], color(140, 115, 36), "Caminho para salvar imagens",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) {
				async.launch([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Onde devo salvar as fotos, se habilitado?", "*.*", ALLEGRO_FILECHOOSER_FOLDER), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0); // path/with/no/end

					stor.conf.set("filemanagement", "export_path", path);
					stor.conf.flush();

					al_show_native_message_box(nullptr, u8"Sucesso!", u8"Salvo!", (u8"As próximas imagens devem ser salvas neste caminho:\n" + path + u8"\n\nOBSERVAÇÃO: Você PRECISA habilitar o salvamento das fotos na aba de preview ou de configuração para as fotos serem salvas!").c_str(), nullptr, ALLEGRO_MESSAGEBOX_WARN);
					screen = stage_enum::HOME;

					cout << console::color::AQUA << "[FILECHOOSER] This is the path to save images now: " << path;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[1][1] + y_off_comp); }, 1.0f, 0.10f);

			make_common_button(3.25f, 1.0f, pairs[2][0], pairs[2][1], color(170, 75, 115), "Abrir config",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { 
				async.launch([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Qual arquivo de configuração deseja importar?", "*.ini", ALLEGRO_FILECHOOSER_FILE_MUST_EXIST), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0);

					stor.conf.flush(); // guaranteed last data saved

					if (!stor.reload_conf(path)) {
						al_show_native_message_box(nullptr, u8"Falha ao tentar ler", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
						return;
					}
					else {
						al_show_native_message_box(nullptr, u8"Sucesso!", u8"Perfeito!", (u8"O arquivo abaixo foi importado com sucesso!\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_WARN);
						screen = stage_enum::HOME;
					}

					cout << console::color::AQUA << "[FILECHOOSER] Imported config: " << path;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[2][1] + y_off_comp); }, 1.0f, 0.10f);

			make_common_button(3.25f, 1.0f, pairs[3][0], pairs[3][1], color(100, 80, 170), "Exportar config",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) {
				async.launch([&] {
					const auto dialog = std::unique_ptr<ALLEGRO_FILECHOOSER, void(*)(ALLEGRO_FILECHOOSER*)>(al_create_native_file_dialog(nullptr, u8"Onde deseja exportar uma cópia?", "*.ini", ALLEGRO_FILECHOOSER_SAVE), al_destroy_native_file_dialog);
					const bool got_something = al_show_native_file_dialog(nullptr, dialog.get()) && al_get_native_file_dialog_count(dialog.get()) > 0;

					if (!got_something) return;

					std::string path = al_get_native_file_dialog_path(dialog.get(), 0);

					if (!stor.export_conf(path)) {
						al_show_native_message_box(nullptr, u8"Falha ao tentar escrever", u8"Houve uma falha!", (u8"Infelizmente não fui capaz de abrir o caminho:\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_ERROR);
						return;
					}
					else {
						al_show_native_message_box(nullptr, u8"Sucesso!", u8"Perfeito!", (u8"Exportei corretamente para o caminho abaixo!\n" + path).c_str(), nullptr, ALLEGRO_MESSAGEBOX_WARN);
						screen = stage_enum::HOME;
					}

					cout << console::color::AQUA << "[FILECHOOSER] Exported config: " << path;
				});
				} },
				[&,pairs,y_off_comp](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, pairs[3][1] + y_off_comp); }, 1.0f, 0.10f);

		}

		cout << console::color::DARK_GRAY << "Building CONFIG sprites...";
		curr_set = stage_enum::CONFIG;
		{
			screen_set[curr_set].min_y = 0.0f;
			screen_set[curr_set].max_y = 0.0f;
			

			const float offauto = 0.40f;
			const float offautox = 0.605f;
			// ================ AUTOSAVE BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f); },
				curr_set
			));
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[&, offautox](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {

						stor.conf.set("filemanagement", "save_all", (down.cpy.real_posx > offautox));

						s->set<float>(enum_sprite_float_e::POS_X, offautox + (stor.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
						s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
							0.4f + 0.6f * (!stor.conf.get_as<bool>("filemanagement", "save_all")),
							0.4f + 0.6f * stor.conf.get_as<bool>("filemanagement", "save_all"),
							0.4f));
					}
					else {
						if (stor.conf.get_as<bool>("filemanagement", "save_all") && stor.conf.get("filemanagement", "export_path").length() < 3) {
							stor.conf.set("filemanagement", "save_all", false);
							async.launch([&, s] {
								int res = al_show_native_message_box(nullptr, u8"Erro!", u8"Você ainda não tem um caminho para salvar as imagens!", u8"Quer que eu te leve ao lugar onde você pode configurar isso?\nVocê precisará clicar no botão para definir o caminho das imagens.", nullptr, ALLEGRO_MESSAGEBOX_YES_NO);
								s->set<float>(enum_sprite_float_e::POS_X, offautox + (stor.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
								s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
									0.4f + 0.6f * (!stor.conf.get_as<bool>("filemanagement", "save_all")),
									0.4f + 0.6f * stor.conf.get_as<bool>("filemanagement", "save_all"),
									0.4f));
								if (res == 1) {
									screen = stage_enum::HOME_OPEN;
								}
							});
						}
					}
				},
				[&,offauto, offautox](sprite* s) {
					s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f);
					s->set<float>(enum_sprite_float_e::POS_X, offautox + (stor.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.4f + 0.6f * (!stor.conf.get_as<bool>("filemanagement", "save_all")),
						0.4f + 0.6f * stor.conf.get_as<bool>("filemanagement", "save_all"),
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
			blk->texture_insert(latest_esp32_texture);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 1.5f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.7f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.17f);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			//blk->set<float>(enum_sprite_float_e::OUT_OF_SIGHT_POS, 9999.9f);
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.5f, 0.5f, 0.5f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(std::move(each)));

			make_common_button(3.5f, 1.0f, -0.7f, -0.85f, color(200, 25, 25), "Voltar",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { screen = stage_enum::HOME; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); });

			make_common_button(4.0f, 0.65f, -0.5f, 0.59f, config_to_color(stor.conf, "reference", "bad_plant"), "Definir cor como \"ruim\"",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					s->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(stor.conf, "reference", "bad_plant"));
					if (down.is_unclick && down.is_mouse_on_it) {
						color_to_config(stor.conf, "reference", "bad_plant", stor.background_color);
						stor.bad_color = stor.background_color;
						stor.bad_perc = how_far(stor.bad_color, stor.background_color);
						stor.good_perc = how_far(stor.good_color, stor.background_color);
					}
				},
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); }, 2.0f, 0.08f);

			make_common_button(4.0f, 0.65f, 0.5f, 0.59f, config_to_color(stor.conf, "reference", "good_plant"), "Definir cor como \"boa\"",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) {
					s->set<color>(enum_sprite_color_e::DRAW_TINT, config_to_color(stor.conf, "reference", "good_plant"));
					if (down.is_unclick && down.is_mouse_on_it) {
						color_to_config(stor.conf, "reference", "good_plant", stor.background_color);
						stor.good_color = stor.background_color;
						stor.bad_perc = how_far(stor.bad_color, stor.background_color);
						stor.good_perc = how_far(stor.good_color, stor.background_color);
					}
				},
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, 1.5f); }, 2.0f, 0.08f);
			
			make_common_button(6.66667f, 1.0f, 0.0f, 0.84f, color(23, 180, 180), u8"Correção e ajuste global",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { screen = stage_enum::CONFIG_FIX; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.5f); }, 1.75f);

			make_text(0.075f, 1.0f, 1.0f, 0.0f, -0.67f, 8.0f, color(200, 200, 200), u8"Pensando...", { text_shadow{ 0.003f,0.02f,color(0,0,0) } },
				[&](sprite* s) {text* t = (text*)s; t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::to_string(stor.good_perc * 100.0f) + "% bom / " + std::to_string(stor.bad_perc * 100.0f) + "% ruim\nConsiderado bom? " + (stor.good_perc >= stor.bad_perc ? "SIM" : u8"NÃO")); },
				[](auto, auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, s->get<float>(enum_sprite_float_e::POS_X)); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, s->get<float>(enum_sprite_float_e::POS_Y)); });


			casted_boys[curr_set].safe([&](std::vector<hybrid_memory<sprite_pair>>& vec) {
				for (auto& ech : vec) ech->get_notick()->set<bool>(enum_sprite_boolean_e::DRAW_TRANSFORM_COORDS_KEEP_SCALE, true);
			});
		}

		cout << console::color::DARK_GRAY << "Building CONFIG_FIX sprites...";
		curr_set = stage_enum::CONFIG_FIX;
		{
			const float off_y = 0.74f;
			const float anim_y = 1.5f;

			screen_set[curr_set].min_y = 0.0f;
			screen_set[curr_set].max_y = off_y;
			

			casted_boys[stage_enum::CONFIG].csafe([&](const std::vector<hybrid_memory<sprite_pair>>& vec) {
				for (const auto& it : vec) {
					hybrid_memory<sprite_pair> cpy = it;
					casted_boys[curr_set].push_back(std::move(cpy));
				}
			});

			float offauto = 0.255f + off_y;

			make_color_box(0.89f, 0.63f, 0.0f, offauto, 1.0f, color(0.1f, 0.1f, 0.1f, 0.7f),
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { 
					if (down.is_unclick && !down.is_mouse_on_it) { screen = stage_enum::CONFIG; }
					if (down.is_unclick && down.is_mouse_on_it) {
						if (latest_esp32_texture.valid() && !latest_esp32_texture->empty()) {
							auto_handle_process_image();
						}
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y - 0.75f); });


			offauto -= 0.455f;

			// ================ SATURATION BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (stor.conf.get_as<float>("processing", "saturation_compensation") * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.4f,
						0.4f + 0.6f * stor.conf.get_as<float>("processing", "saturation_compensation"),
						0.4f));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						stor.conf.set("processing", "saturation_compensation", (down.cpy.real_posx + 0.7f) / 1.4f);
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(src_font);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						u8"Saturação: +" + std::to_string((int)((stor.conf.get_as<float>("processing", "saturation_compensation")) * 100.0f)) + "% boost"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ BRIGHTNESS BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + ((stor.conf.get_as<float>("processing", "brightness_compensation") * 2.0f) * 1.4f)); // up to 0.5
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "brightness_compensation") * 2.0f),
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "brightness_compensation") * 2.0f),
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "brightness_compensation") * 2.0f)));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						stor.conf.set("processing", "brightness_compensation", 0.5f * (down.cpy.real_posx + 0.7f) / 1.4f);
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(src_font);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Brilho: +" + std::to_string((int)((stor.conf.get_as<float>("processing", "brightness_compensation")) * 100.0f)) + "% boost"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ OVERFLOW BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + ((stor.conf.get_as<float>("processing", "overflow_boost") - 1.0f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.4f - 0.4f * (stor.conf.get_as<float>("processing", "overflow_boost") - 1.0f),
						0.7f + 0.3f * (stor.conf.get_as<float>("processing", "overflow_boost") - 1.0f),
						0.4f - 0.4f * (stor.conf.get_as<float>("processing", "overflow_boost") - 1.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						stor.conf.set("processing", "overflow_boost", 1.0f + (down.cpy.real_posx + 0.7f) / 1.4f);
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(src_font);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Ganho: +" + std::to_string((int)((stor.conf.get_as<float>("processing", "overflow_boost") - 1.0f) * 100.0f)) + "%"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ CENTER WEIGHT BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (((stor.conf.get_as<float>("processing", "center_weight") - 1.0f) / 29.0f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * ((stor.conf.get_as<float>("processing", "center_weight") - 1.0f) / 29.0f),
						0.6f,
						0.6f + 0.4f * ((stor.conf.get_as<float>("processing", "center_weight") - 1.0f) / 29.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						stor.conf.set("processing", "center_weight", static_cast<int>(1.1f + 29.0f * (down.cpy.real_posx + 0.7f) / 1.4f));
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(src_font);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Peso central: x" + std::to_string((int)(stor.conf.get_as<float>("processing", "center_weight")))
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.19f;
			// ================ X AXIS BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (((stor.conf.get_as<float>("processing", "area_center_x") - 0.1f) / 0.9f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "area_center_x") - 1.0f),
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "area_center_x") - 1.0f),
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "area_center_x") - 1.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						stor.conf.set("processing", "area_center_x", 0.1f + 0.9f * (down.cpy.real_posx + 0.7f) / 1.4f);
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(src_font);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Largura do centro: " + std::to_string((int)(stor.conf.get_as<float>("processing", "area_center_x") * 100.0f)) + "%"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			offauto += 0.18f;
			// ================ Y AXIS BAR
			// First bar config
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[](auto) {},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			// First bar config slider
			make_blk();
			blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
			blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					s->set<float>(enum_sprite_float_e::POS_X, -0.7f + (((stor.conf.get_as<float>("processing", "area_center_y") - 0.1f) / 0.9f) * 1.4f));
					s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "area_center_y") - 1.0f),
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "area_center_y") - 1.0f),
						0.6f + 0.4f * (stor.conf.get_as<float>("processing", "area_center_y") - 1.0f)
					));
				},
				[&](sprite* s, const sprite_pair::cond& down) {
					((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
					if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {
						stor.conf.set("processing", "area_center_y", 0.1f + 0.9f * (down.cpy.real_posx + 0.7f) / 1.4f);
					}
				},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));

			make_txt();
			txt->font_set(src_font);
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
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
				std::move(each),
				[&](sprite* s) {
					((text*)s)->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(
						"Altura do centro: " + std::to_string((int)(stor.conf.get_as<float>("processing", "area_center_y") * 100.0f)) + "%"
					));
				},
				[](auto, auto) {},
				[offauto, anim_y](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto + anim_y); }
			));
		}

		cout << console::color::DARK_GRAY << "Building PREVIEW sprites...";
		curr_set = stage_enum::PREVIEW;
		{
			screen_set[curr_set].min_y = 0.0f;
			screen_set[curr_set].max_y = 0.0f;

			make_color_box(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, color(0.1f, 0.1f, 0.1f, 0.6f));
			
			const float offauto = 0.44f;
			const float offautox = 0.605f;
			// ================ AUTOSAVE BAR
			// First bar config
			{
				make_blk();
				blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
				casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
					std::move(each),
					[](auto) {},
					[](auto, auto) {},
					[offauto](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f); },
					curr_set
				));
				make_blk();
				blk->texture_insert(texture_map[textures_enum::BUTTON_UP]);
				blk->texture_insert(texture_map[textures_enum::BUTTON_DOWN]);
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
				casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(
					std::move(each),
					[](auto) {},
					[&, offautox](sprite* s, const sprite_pair::cond& down) {
						((block*)s)->set<size_t>(enum_block_sizet_e::RO_DRAW_FRAME, (down.is_mouse_on_it && down.is_mouse_pressed) ? 1 : 0);
						if (down.is_mouse_on_it && down.is_mouse_pressed && down.cpy.real_posx < 0.7f && down.cpy.real_posx > -0.7f) {

							stor.conf.set("filemanagement", "save_all", (down.cpy.real_posx > offautox));

							s->set<float>(enum_sprite_float_e::POS_X, offautox + (stor.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
							s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
								0.4f + 0.6f * (!stor.conf.get_as<bool>("filemanagement", "save_all")),
								0.4f + 0.6f * stor.conf.get_as<bool>("filemanagement", "save_all"),
								0.4f));
						}
						else {
							if (stor.conf.get_as<bool>("filemanagement", "save_all") && stor.conf.get("filemanagement", "export_path").length() < 3) {
								stor.conf.set("filemanagement", "save_all", false);
								async.launch([&, s] {
									int res = al_show_native_message_box(nullptr, u8"Erro!", u8"Você ainda não tem um caminho para salvar as imagens!", u8"Quer que eu te leve ao lugar onde você pode configurar isso?\nVocê precisará clicar no botão para definir o caminho das imagens.", nullptr, ALLEGRO_MESSAGEBOX_YES_NO);
									s->set<float>(enum_sprite_float_e::POS_X, offautox + (stor.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
									s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
										0.4f + 0.6f * (!stor.conf.get_as<bool>("filemanagement", "save_all")),
										0.4f + 0.6f * stor.conf.get_as<bool>("filemanagement", "save_all"),
										0.4f));
									if (res == 1) {
										screen = stage_enum::HOME_OPEN;
									}
								});
							}
						}
					},
					[&,offauto, offautox](sprite* s) {
						s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, offauto - 0.6f);
						s->set<float>(enum_sprite_float_e::POS_X, offautox + (stor.conf.get_as<bool>("filemanagement", "save_all") ? 0.07f : -0.07f));
						s->set<color>(enum_sprite_color_e::DRAW_TINT, color(
							0.4f + 0.6f * (!stor.conf.get_as<bool>("filemanagement", "save_all")),
							0.4f + 0.6f * stor.conf.get_as<bool>("filemanagement", "save_all"),
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
			blk->texture_insert(latest_esp32_texture);
			blk->set<float>(enum_sprite_float_e::SCALE_G, 1.5f);
			blk->set<float>(enum_sprite_float_e::SCALE_Y, 0.7f);
			blk->set<float>(enum_sprite_float_e::POS_X, 0.0f);
			blk->set<float>(enum_sprite_float_e::POS_Y, -0.13f);
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, blk->get<float>(enum_sprite_float_e::POS_X));
			blk->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, blk->get<float>(enum_sprite_float_e::POS_Y));
			blk->set<color>(enum_sprite_color_e::DRAW_DRAW_BOX, color(0.5f, 0.5f, 0.5f));
			blk->set<bool>(enum_sprite_boolean_e::DRAW_DRAW_BOX, true);
			casted_boys[curr_set].push_back(make_hybrid<sprite_pair>(std::move(each)));

			make_common_button(3.5f, 1.0f, -0.7f, -0.85f, color(200, 25, 25), "Voltar",
				[](auto) {},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { screen = stage_enum::HOME; } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, -1.5f); });

			make_text(0.09f, 1.0f, 1.0f, 0.0f, 0.62f, 8.0f, color(200, 200, 200), u8"Pensando...", { text_shadow{ 0.004f,0.05f,color(0,0,0) } },
				[&](sprite* s) {
					text* t = (text*)s;
					if (stor.good_perc == 0.0f && stor.bad_perc == 0.0f && stor.wifi_status == Storage::conn_status::NON_CONNECTED) {
						std::string a = ".";
						for (size_t p = 0; p < (static_cast<size_t>(1.5 * al_get_time()) % 3); p++) a += '.';
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(u8"\nAguardando conexão" + a));
					}
					else {
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::to_string(stor.good_perc * 100.0f) + "% bom / " + std::to_string(stor.bad_perc * 100.0f) + "% ruim\nConsiderado bom? " + (stor.good_perc >= stor.bad_perc ? "SIM" : u8"NÃO"));
					}
				},
				[](auto, auto) {},
				[](sprite* s) {s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_X, s->get<float>(enum_sprite_float_e::POS_X)); s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 0.85f); });

			

			make_common_button(15.0f, 0.85f, 0.0f, 1.095f, color(25, 150, 65), u8"Não conectado",
				[&](sprite* s) {
					if (stor.wifi_status == Storage::conn_status::NON_CONNECTED) {
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
					if (stor.wifi_status == Storage::conn_status::NON_CONNECTED) {
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
					if (stor.wifi_status == Storage::conn_status::NON_CONNECTED) s->set<float>(enum_sprite_float_e::POS_Y, 1.07f);
					else																	s->set<float>(enum_sprite_float_e::POS_Y, 0.935f);
				},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { comm.request_weight(); } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.07f); }, 15.0f, 0.12f,
				[&](sprite* s) {
					text* t = (text*)s;
					s->set<float>(enum_sprite_float_e::POS_Y, 0.935f - 0.12f * 0.45f);
					if (stor.wifi_status == Storage::conn_status::NON_CONNECTED) {
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("offline"));
						s->set<float>(enum_sprite_float_e::POS_Y, 1.07f - 0.12f * 0.45f);
					}
					else {
						char formattt[40];
						sprintf_s(formattt, "%07.3f kg", comm.get_weight_good());
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(formattt));
					}
				});
			
			make_common_button(3.0f, 0.7f, 0.7f, 1.2f, color(150, 25, 65), "---.--- kg",
				[&](sprite* s) {
					if (stor.wifi_status == Storage::conn_status::NON_CONNECTED) s->set<float>(enum_sprite_float_e::POS_Y, 1.07f);
					else																	s->set<float>(enum_sprite_float_e::POS_Y, 0.935f);
				},
				[&](sprite* s, const sprite_pair::cond& down) { if (down.is_unclick && down.is_mouse_on_it) { comm.request_weight(); } },
				[&](sprite* s) { s->set<float>(enum_sprite_float_e::RO_DRAW_PROJ_POS_Y, 1.07f); }, 15.0f, 0.12f,
				[&](sprite* s) {
					text* t = (text*)s;
					s->set<float>(enum_sprite_float_e::POS_Y, 0.935f - 0.12f * 0.45f);
					if (stor.wifi_status == Storage::conn_status::NON_CONNECTED) {
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string("offline"));
						s->set<float>(enum_sprite_float_e::POS_Y, 1.07f - 0.12f * 0.45f);
					}
					else {
						char formattt[40];
						sprintf_s(formattt, "%07.3f kg", comm.get_weight_bad());
						t->set<safe_data<std::string>>(enum_text_safe_string_e::STRING, std::string(formattt));
					}
				});

		}
	}
	stor.set_graphic_perc(0.90f);

	update_pos.task_async([this] {
		casted_boys[screen].safe([&](std::vector<hybrid_memory<sprite_pair>>& mypairs) {
			for (auto& i : mypairs) {
				i->get_think()->think();
			}
		});
		{
			wifi_blk.get_think()->think();
			mouse_pair.get_think()->think();
		}
	}, thread::speed::INTERVAL, 1.0 / 20);

	stor.set_graphic_perc(0.99f);
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	stor.set_camera(Storage::camera_mode::DEFAULT_CAMERA, true);
	update_display = true;

	return true;
}

void GraphicInterface::stop()
{
	on_quit_f.csafe([&](const std::function<void(void)>& f) { if (f) f(); });

	stor.set_graphic_perc(0.01f);
	stor.set_camera(Storage::camera_mode::PROGRESS_BAR_NO_RESOURCE, true);
	update_display = true;

	update_pos.join();
	disp.post_task([&] {
		for (auto& i : texture_map) {
			if (i.store.valid()) i.store->destroy();
		}
		if (src_atlas.valid()) src_atlas->destroy();
		if (src_font.valid()) src_font->destroy();
		if (latest_esp32_texture.valid()) latest_esp32_texture->destroy();
		return true;
	}).get();

	mouse_work.unhook_event();
	screen = stage_enum::HOME;
	stor.file_font.reset_this();
	stor.file_atlas.reset_this();
	if (stor.latest_esp32_texture_orig.valid()) stor.latest_esp32_texture_orig->destroy();

	for (auto& i : casted_boys) {
		i.second.safe([](std::vector<hybrid_memory<sprite_pair>>& vec) {
			vec.clear();
		});
	}

	stor.set_graphic_perc(0.99f);

	disp.destroy();
}
