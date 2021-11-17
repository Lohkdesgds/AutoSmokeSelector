#pragma once

#include <Lunaris/all.h>
#include "storage.h"
#include "../../_shared/package_definition.h"

using namespace Lunaris;

#define COMMUNICATION_ASSERT_AUTO(X, MSGGG) [&]{ if (!(X)) {Lunaris::cout << Lunaris::console::color::RED << MSGGG; Lunaris::cout << Lunaris::console::color::RED << "[AUTOMATIC ABORT CONNECTION]"; if (client) client->close_socket(); return false; } return true;}()
#define CHECK_HASH

constexpr double communication_ask_time = 60.0; // once a sec

class Communication : public NonCopyable, public NonMovable {
	std::string ip_cpy = "127.0.0.1";
	Storage& stor;
	float weight_good = 0.0f;
	float weight_bad = 0.0f;
	safe_vector<esp_package> packages_to_send;
	hybrid_memory<file> file_temp;

	double ask_weight_time = 0.0;

	std::unique_ptr<TCP_client> client;

	safe_data<std::function<int(hybrid_memory<file>&&)>> file_handler;

	void _async();
	void _set_stat(Storage::conn_status);
	// 0 == false, 1 == true, -1 == failed
	int _push_file();
	bool _make_new_file_auto();
public:
	Communication(Storage&);
	~Communication();

	void start_sync(const std::string&, const std::function<bool(void)>);
	void stop();

	// move file, returns true if good
	void set_on_file(std::function<int(hybrid_memory<file>&&)>);

	float get_weight_good() const;
	float get_weight_bad() const;

	void request_weight();
};