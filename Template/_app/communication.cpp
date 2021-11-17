#include "communication.h"

void Communication::_async()
{
	if (!client->has_socket()) {
		if (!client->setup(socket_config().set_port(ESP_HOST_PORT).set_family(socket_config::e_family::IPV4).set_ip_address(ip_cpy))) // esp32_ip_address_fixed
		{
			_set_stat(Storage::conn_status::NON_CONNECTED);
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			return;
		}
		else {
			_set_stat(Storage::conn_status::IDLE);
		}
	}
	else {
#ifdef CONNECTION_VERBOSE
		cout << console::color::DARK_PURPLE << "ESP32 SEND";
#endif
		// ############################################
		// # SEND
		// ############################################
		esp_package pkg, spkg;

		if (al_get_time() - ask_weight_time > 0.0) {
			ask_weight_time = al_get_time() + communication_ask_time;
			esp_package _tmp;
			build_pc_request_weight(&_tmp);
			packages_to_send.push_back(std::move(_tmp));
			cout << console::color::AQUA << "[CLIENT] Auto requesting weight.";
		}

		if (packages_to_send.size()) {
			packages_to_send.safe([&](std::vector<esp_package>& vec) {spkg = vec.front(); vec.erase(vec.begin()); });
		}
		else {
			COMMUNICATION_ASSERT_AUTO(build_both_empty(&spkg) == 1, "Can't prepare empty package.");
		}
		COMMUNICATION_ASSERT_AUTO(client->send((char*)&spkg, sizeof(spkg)), "Can't prepare empty package.");

#ifdef CONNECTION_VERBOSE
		cout << console::color::DARK_PURPLE << "ESP32 RECV";
#endif
		// ############################################
		// # RECV
		// ############################################
		COMMUNICATION_ASSERT_AUTO(client->recv(pkg), "Failed recv client->");

#ifdef CONNECTION_VERBOSE
		cout << console::color::DARK_PURPLE << "ESP32 HANDLE";
#endif

		switch (pkg.type) {
		case PKG_ESP_DETECTED_SOMETHING:
		{
			cout << console::color::AQUA << "[CLIENT] ESP32 said it detected something! Expecting/receiving a file...";

			const bool gud = _make_new_file_auto();

			if (!gud) {
				cout << console::color::RED << "[CLIENT] Can't open new file. ABORT!";
				client->close_socket();
				_set_stat(Storage::conn_status::NON_CONNECTED);
				return;
			}
			else {
				_set_stat(Storage::conn_status::DOWNLOADING_IMAGE);
			}
		}
		break;
		case PKG_ESP_JPG_SEND:
		{
			if (file_temp.empty()) {
				cout << console::color::RED << "[CLIENT] The package order wasn't right! Trying to fix anyway...";

				const bool gud = _make_new_file_auto();

				if (!gud) {
					cout << console::color::RED << "[CLIENT] Can't open new file. ABORT!";
					client->close_socket();
					_set_stat(Storage::conn_status::NON_CONNECTED);
					return;
				}
				else {
					_set_stat(Storage::conn_status::DOWNLOADING_IMAGE);
				}
			}

			if (file_temp->write(pkg.data.raw, pkg.len) != pkg.len) {
				cout << console::color::RED << "[CLIENT] Temporary file has stopped working?! ABORTING!";
				client->close_socket();
				_set_stat(Storage::conn_status::NON_CONNECTED);
				return;
			}

			if (pkg.left == 0) {
				cout << console::color::AQUA << "[CLIENT] End of file! Analysing...";
				file_temp->flush();
				_set_stat(Storage::conn_status::PROCESSING_IMAGE);

				cout << console::color::AQUA << "[CLIENT] File has " << file_temp->size() << " bytes.";

				file_temp->seek(0, file::seek_mode_e::BEGIN);

#ifdef CHECK_HASH
				{
					std::vector<char> buff;
					char minibuf[256];
					size_t herr = 0;
					while (herr = file_temp->read(minibuf, 256)) buff.insert(buff.end(), minibuf, minibuf + herr);
					cout << console::color::AQUA << "Check HASH: " << sha256(buff) << ", size: " << buff.size();
					file_temp->seek(0, file::seek_mode_e::BEGIN);
				}
#endif
				cout << console::color::AQUA << "[CLIENT] Pushing file to function (or void if not set lol)";
				int res = _push_file();

				if (res == -1) {
					cout << console::color::RED << "[CLIENT] FILE FUNCTION MALFUNCTION! ABORT!";
					client->close_socket();
					_set_stat(Storage::conn_status::NON_CONNECTED);
					return;
				}
					_set_stat(Storage::conn_status::COMMANDING);
				cout << console::color::AQUA << "[CLIENT] Good. Next task should send result.";

				build_pc_good_or_bad(&spkg, static_cast<char>(res == 1));
				packages_to_send.push_back(std::move(spkg));
			}

			if (pkg.left > 0) _set_stat(Storage::conn_status::DOWNLOADING_IMAGE);
		}
		break;
		case PKG_ESP_WEIGHT_INFO:
		{
			weight_good = pkg.data.esp.weight_info.good_side_kg;
			weight_bad = pkg.data.esp.weight_info.bad_side_kg;
			cout << console::color::AQUA << "[CLIENT] ESP32 got weight info [good=" << weight_good << ";bad=" << weight_bad << "] (kg)";
			_set_stat(Storage::conn_status::IDLE);
		}
		break;
		default:
			_set_stat(Storage::conn_status::IDLE);
#ifdef CONNECTION_VERBOSE
			cout << console::color::DARK_PURPLE << "ESP32 NO DATA";
#endif
			break;
		}
	}
}

void Communication::_set_stat(Storage::conn_status s)
{
	stor.wifi_status = s;
}

int Communication::_push_file()
{
	int ret_v = -1;
	file_handler.csafe([this,&ret_v](const std::function<int(hybrid_memory<file>&&)>& f) {if (f && file_temp.valid() && file_temp->is_open()) ret_v = f(std::move(file_temp)); file_temp.reset_this(); });
	return ret_v;
}

bool Communication::_make_new_file_auto()
{
	file_temp = make_hybrid_derived<file, tempfile>();
	return ((tempfile*)file_temp.get())->open("ass_temp_handling_texture_XXXXXX.jpg");
}

Communication::Communication(Storage& s)
	: stor(s)
{
}

Communication::~Communication()
{
	stop();
}

void Communication::start_sync(const std::string& ip, const std::function<bool(void)> keep_going)
{
	if (!keep_going) throw std::runtime_error("Invalid function");
	stop();
	ip_cpy = ip;
	client = std::make_unique<TCP_client>();

	while (keep_going())
	{
		_async();
	}

	//proc.task_async([ip,this] { _async(); },
	//	thread::speed::INTERVAL, 1.0 / 100, 
	//	[](const std::exception& e) {cout << console::color::RED << "Communication exception: " << e.what(); });
}

void Communication::stop()
{
	//proc.join();
	client.reset();
	packages_to_send.clear();
	file_temp.reset_this();
	stor.wifi_status = Storage::conn_status::NON_CONNECTED;
	weight_bad = weight_good = 0.0f;
	ask_weight_time = 0.0;
}

void Communication::set_on_file(std::function<int(hybrid_memory<file>&&)> f)
{
	file_handler = f;
}

float Communication::get_weight_good() const
{
	return weight_good;
}

float Communication::get_weight_bad() const
{
	return weight_bad;
}

void Communication::request_weight()
{
	ask_weight_time = al_get_time() + 0.2;
}
