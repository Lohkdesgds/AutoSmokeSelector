#pragma once

#include <Lunaris/all.h>
#include "data_handling.h"

using namespace Lunaris;

struct Storage {
	enum class camera_mode { PROGRESS_BAR_NO_RESOURCE, DEFAULT_CAMERA };
	enum class conn_status { NON_CONNECTED, IDLE, DOWNLOADING_IMAGE, PROCESSING_IMAGE, COMMANDING };
private:
	const std::string static_path;
	camera_mode cam_now = camera_mode::PROGRESS_BAR_NO_RESOURCE;
	camera_mode cam_ack = camera_mode::PROGRESS_BAR_NO_RESOURCE;
public:
	config conf;
	hybrid_memory<file> file_font; // MEMFILE!
	hybrid_memory<file> file_atlas; // MEMFILE!
	color bad_color, good_color;
	float bad_perc = 0.0f, good_perc = 0.0f;
	std::atomic_bool kill_all = false;
	float generic_progressbar = 0.0f;
	color background_color = color(32, 32, 32);
	hybrid_memory<file> latest_esp32_download_file;
	hybrid_memory<texture> latest_esp32_texture_orig; // memory bitmap in memory already baby
	conn_status wifi_status = conn_status::NON_CONNECTED;

	// for set_camera waiting for drawing thread to ack, this acks change (and gets the current val)
	camera_mode camera_ack();
	// has confirm wait?
	void set_camera(camera_mode, const bool);
	// when on PROGRESS_BAR_NO_RESOURCE mode, this [0.0,1.0] = [0%, 100%] on screen
	void set_graphic_perc(const float);

	void replace_file_auto(hybrid_memory<file>&&);

	bool reload_conf(const std::string&);
	bool export_conf(const std::string&);

	// config path
	Storage(const std::string&);
};