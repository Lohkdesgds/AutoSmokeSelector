#include "storage.h"

Storage::camera_mode Storage::camera_ack()
{
	return cam_ack = cam_now;
}

void Storage::set_camera(camera_mode v, const bool wai)
{
	cam_now = v;
	while (wai && (cam_now != cam_ack)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Storage::set_graphic_perc(const float f)
{
	if (f < generic_progressbar) smooth_progressbar = 0.0f;
	generic_progressbar = limit_range_of(f, 0.0f, 1.0f);
}

void Storage::replace_file_auto(hybrid_memory<file>&& ff)
{
	latest_esp32_download_file.replace_shared(std::move(ff.reset_shared()));
}

bool Storage::reload_conf(const std::string& cpath)
{
	bool gud = conf.load(cpath);
	conf.save_path(static_path);
	ensure_default_conf(conf);
	conf.flush();

	good_color = config_to_color(conf, "reference", "good_plant");
	bad_color = config_to_color(conf, "reference", "bad_plant");

	return gud;
}

bool Storage::export_conf(const std::string& path)
{
	conf.save_path(path);
	bool gud = conf.flush();
	conf.save_path(static_path);
	return gud;
}

Storage::Storage(const std::string& cpath)
	: static_path(cpath)
{
	make_path(static_path);
	reload_conf(cpath);
}
