#include <Lunaris/all.h>
#include "../_shared/package_definition.h"
#include "_app/core.h"

using namespace Lunaris;

int main()
{
	CoreWorker core;
	core.work_and_yield();
}

//void host_job();
//
//int main()
//{
//	//CoreWorker work;
//	//if (!work.launch()) return 1;
//	//
//	//work.work();
//	host_job();
//
//	std::this_thread::sleep_for(std::chrono::seconds(1));
//
//	return 0;
//}
//
//void host_job()
//{
//	TCP_host host;
//	bool good = host.setup(socket_config()
//		.set_family(socket_config::e_family::IPV4)
//		.set_port(ESP_HOST_PORT));
//
//	if (!good) {
//		cout << console::color::RED << "Bad news, host won't wake up!";
//		return;
//	}
//
//	while (1) {
//		cout << console::color::GREEN << "Listening...";
//		auto client = host.listen();
//		if (client.has_socket()) {
//			{
//				//socket_config self;
//				cout << console::color::GREEN << "Good connection so far!";// From: " << client.convert_from(self, {});
//			}
//
//			while (client.has_socket()) {
//				const auto vec = client.recv(6);
//				if (vec.size() != 6) {
//					cout << console::color::RED << "Bad news, wrong package size.";
//					client.close_socket();
//					continue;
//				}
//				cout << "Got: " << vec.data();
//				client.send("Hello!", 6);
//				std::this_thread::sleep_for(std::chrono::milliseconds(10));
//			}
//
//			cout << console::color::GREEN << "Disconnected properly.";
//		}
//		else {
//			cout << console::color::YELLOW << "Bad connection.";
//		}
//	}
//}

/*

#define EMULATE_RANDOM_ESP32


#ifdef EMULATE_RANDOM_ESP32
struct __random_esp32_data {
	TCP_host host;
	TCP_client host_cl;
	bool has_initialized_once = false;

	struct random_data {
		char* iterator = nullptr;
		unsigned long length = 0;
		bool detected_something = false;
		std::string __easy_mem;
		std::mutex __saf;
	} detectors;

	~__random_esp32_data();
	void __thread_as_random_esp32();
};

#endif

struct __async_main_thread_requests {
	TCP_client client;
	bool request_weight = false;
	int good_bad_send = -1; // PLANT_GOOD == good, PLANT_BADD == bad, -1 == none
	thread async;

	void async_keep_alive();
};

int main()
{
#ifdef EMULATE_RANDOM_ESP32
	cout << console::color::GREEN << "[FAKE_ESP32] Configuration enabled fake ESP32 emulation.";
	__random_esp32_data __thr_rnd_esp32_data;
	thread __emulated_esp32_async([&__thr_rnd_esp32_data] {__thr_rnd_esp32_data.__thread_as_random_esp32(); }, thread::speed::INTERVAL, 1.0 / 1);
#endif
	__async_main_thread_requests __mainstuff;

	__mainstuff.async.task_async([&] { __mainstuff.async_keep_alive(); }, thread::speed::INTERVAL, 1.0 / 1);
	

	std::string userbuf;

	while (1) {
		std::getline(std::cin, userbuf);

		if (userbuf == "exit") break;
		else if (userbuf == "help") {
			cout << "Commands:";
			cout << "exit: exit the app";
			cout << "help: this";
			cout << "weight: request weight data";
			cout << "rndstr: generate random string on the other side";
			cout << "send_bad_plant: return as bad plant";
			cout << "send_good_plant: return as good plant";
		}
		else if (userbuf == "weight") {
			__mainstuff.request_weight = true;
			cout << "Added task to weight request";
		}
		else if (userbuf == "send_bad_plant") {
			__mainstuff.good_bad_send = PLANT_BADD;
			cout << "Added task to bad plant";
		}
		else if (userbuf == "send_good_plant") {
			__mainstuff.good_bad_send = PLANT_GOOD;
			cout << "Added task to good plant";
		}
		else if (userbuf == "rndstr") {
			std::lock_guard<std::mutex> safy(__thr_rnd_esp32_data.detectors.__saf);
			__thr_rnd_esp32_data.detectors.__easy_mem = ((random() % 2) == 0 ? "Once upon a time there was " : "In the past we believed that there were ") + std::to_string(random() % 1000) + ((random() % 2) == 0 ? " monsters around the city." : " people somewhere in the yard.") + " The end.";
			__thr_rnd_esp32_data.detectors.iterator = __thr_rnd_esp32_data.detectors.__easy_mem.data();
			__thr_rnd_esp32_data.detectors.length = static_cast<unsigned long>(__thr_rnd_esp32_data.detectors.__easy_mem.length());
			__thr_rnd_esp32_data.detectors.detected_something = true;
			cout << "Generated random string as jpg";
		}
	}

	__mainstuff.client.close_socket();
	__mainstuff.async.join();
#ifdef EMULATE_RANDOM_ESP32
	__emulated_esp32_async.join();
#endif
	return 0;
}

#ifdef EMULATE_RANDOM_ESP32
__random_esp32_data::~__random_esp32_data()
{
	host_cl.close_socket();
}

void __random_esp32_data::__thread_as_random_esp32()
{
	if (has_initialized_once) {
		if (!host_cl.has_socket()) {
			cout << console::color::YELLOW << "[FAKE_ESP32] ESP32 is now waiting for new connection.";

			while (!(host_cl = host.listen()).has_socket()) {
				cout << console::color::GREEN << "[FAKE_ESP32] One connection wasn't valid, waiting again...";
			}

			cout << console::color::GREEN << "[FAKE_ESP32] Connected!";
			return;
		}

		std::vector<char> raw_to_build;
		raw_to_build.resize(sizeof(esp_package));
		esp_package* to_build = (esp_package*)raw_to_build.data();

		const auto send_empty = [&] {
			if (build_both_empty(to_build) < 0) return false;
			if (!host_cl.send(raw_to_build)) return false;
			return true;
		};
		const auto easy_abort_timed = [&] {
			cout << console::color::RED << "[FAKE_ESP32] CAN'T SEND PACKAGE! ERROR! DESYNCED SOMEHOW! ABORT!";
			host_cl.close_socket();
			std::this_thread::sleep_for(std::chrono::seconds(2));
			return;
		};
		const auto easy_report_quit_empty = [&](const std::string& rep) {
			if (rep.length()) cout << console::color::YELLOW << "[FAKE_ESP32] Exception: " + rep;
			if (!send_empty()) { easy_abort_timed(); return; }
		};

		// ############################################
		// # RECV
		// ############################################

		//cout << console::color::DARK_PURPLE << "[FAKE_ESP32] Recv...";
		auto raw_data = host_cl.recv(sizeof(esp_package));
		//cout << console::color::DARK_PURPLE << "[FAKE_ESP32] Good.";

		if (raw_data.size() != sizeof(esp_package)) {
			cout << console::color::YELLOW << "[FAKE_ESP32] INVALID PACKAGE CAME TO ESP32! Closed? ABORT!";
			host_cl.close_socket();
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
		else {

			// ############################################
			// # SEND (if good, else kill self (above))
			// ############################################

			esp_package* pkg = (esp_package*)raw_data.data();

			switch (pkg->type) {
			case PKG_PC_OPTION_FINAL:
			{
				cout << console::color::GREEN << "[FAKE_ESP32] PKG_PC_OPTION_FINAL: " << (pkg->data.pc.option_final.good == PLANT_GOOD ? "GOOD" : "BAD");
				send_empty();
			}
			break;
			case PKG_PC_REQUEST_WEIGHT:
			{
				cout << console::color::GREEN << "[FAKE_ESP32] PKG_PC_REQUEST_WEIGHT called. Returning numbers...";

				if (build_esp_weight_info(to_build, (random() % 100000) * 0.001f, (random() % 100000) * 0.001f) < 0) { easy_report_quit_empty("Failed to build build_esp_weight_info"); return; }

				cout << console::color::GREEN << "[FAKE_ESP32] PKG_PC_REQUEST_WEIGHT are: {" << to_build->data.esp.weight_info.bad_side_kg << " kg bad; " << to_build->data.esp.weight_info.good_side_kg << " kg good}";

				if (!host_cl.send(raw_to_build)) easy_abort_timed();
			}
			break;
			default:
			{
				std::lock_guard<std::mutex> safy(detectors.__saf);

				if (detectors.detected_something && detectors.iterator != nullptr && detectors.length > 0)
				{
					if (build_esp_has_something(to_build) < 0) { easy_report_quit_empty("Failed to build esp_has_something"); return; }
					detectors.detected_something = false; // pointer is not null, read.
				}
				else if (detectors.iterator != nullptr && detectors.length > 0)
				{
					if (build_esp_each_jpg_data(to_build, &detectors.iterator, &detectors.length) < 0) { easy_report_quit_empty("Failed to build esp_each_jpg_data"); return; }
				}
				else {
					if (build_both_empty(to_build) < 0) { easy_abort_timed(); return; }
				}

				if (!host_cl.send(raw_to_build)) easy_abort_timed();
			}
			break;
			}
		}
	}
	else {
		if (!host.setup(socket_config()
			.set_family(socket_config::e_family::IPV4)
			.set_port(ESP_HOST_PORT)
		)) {
			cout << console::color::RED << "[FAKE_ESP32] FATAL ERROR SIMULATING ESP32 TCP HOST!";
			std::this_thread::sleep_for(std::chrono::seconds(5));
			std::terminate(); // fatal error
		}
		else {
			cout << console::color::GREEN << "[FAKE_ESP32] TCP host working! Waiting user...";

			while (!(host_cl = host.listen()).has_socket()) {
				cout << console::color::GREEN << "[FAKE_ESP32] One connection wasn't valid, waiting again...";
			}

			cout << console::color::GREEN << "[FAKE_ESP32] Connected!";
			has_initialized_once = true;
		}
	}
}
#endif

void __async_main_thread_requests::async_keep_alive()
{
	if (client.has_socket()) {

		std::vector<char> raw_to_build;
		raw_to_build.resize(sizeof(esp_package));
		esp_package* to_build = (esp_package*)raw_to_build.data();

		const auto send_empty = [&] {
			if (build_both_empty(to_build) < 0) return false;
			if (!client.send(raw_to_build)) return false;
			return true;
		};
		const auto easy_abort_timed = [&] {
			cout << console::color::RED << "[CLIENT] CAN'T SEND PACKAGE! ERROR! DESYNCED SOMEHOW! ABORT!";
			client.close_socket();
			std::this_thread::sleep_for(std::chrono::seconds(2));
			return;
		};
		const auto easy_report_quit_empty = [&](const std::string& rep) {
			if (rep.length()) cout << console::color::YELLOW << "[CLIENT] Exception: " + rep;
			if (!send_empty()) { easy_abort_timed(); return; }
		};

		// ############################################
		// # SEND
		// ############################################


		if (request_weight) {
			request_weight = false;
			if (build_pc_request_weight(to_build) < 0) { easy_report_quit_empty("Failed doing build_pc_request_weight");  return; }
			if (!client.send(raw_to_build)) { easy_report_quit_empty("Failed doing build_pc_request_weight");  return; }
			cout << console::color::AQUA << "[CLIENT] Requested weight info";
		}
		else if (good_bad_send != PLANT_NONE) {
			if (build_pc_good_or_bad(to_build, static_cast<char>(good_bad_send)) < 0) { easy_report_quit_empty("Failed doing build_pc_good_or_bad");  return; }
			if (!client.send(raw_to_build)) { easy_report_quit_empty("Failed doing build_pc_good_or_bad");  return; }
			cout << console::color::AQUA << "[CLIENT] Sent plant info";
			good_bad_send = PLANT_NONE;
		}
		else {
			if (!send_empty()) { easy_abort_timed(); return; }
		}

		// ############################################
		// # RECV
		// ############################################

		//cout << console::color::DARK_PURPLE << "[CLIENT] Recv...";
		auto raw_data = client.recv(sizeof(esp_package));
		//cout << console::color::DARK_PURPLE << "[CLIENT] Good.";

		if (raw_data.size() != sizeof(esp_package)) {
			cout << console::color::RED << "[CLIENT] INVALID PACKAGE CAME TO CLIENT! ABORT!";
			client.close_socket();
			std::this_thread::sleep_for(std::chrono::seconds(2));
			return;
		}
		else {
			esp_package* pkg = (esp_package*)raw_data.data();

			switch (pkg->type) {
			case PKG_ESP_DETECTED_SOMETHING:
			{
				cout << console::color::AQUA << "[CLIENT] ESP32 said it detected something!";
			}
			break;
			case PKG_ESP_JPG_SEND:
			{
				std::string mov;
				for (unsigned long a = 0; a < pkg->len; a++) mov += pkg->data.esp.raw_data[a];
				cout << console::color::AQUA << "[CLIENT] ESP32 block of data:";
				cout << console::color::AQUA << "[CLIENT] {" << mov << "}";
				cout << console::color::AQUA << "[CLIENT] Remaining: " << pkg->left;
				cout << console::color::AQUA << "[CLIENT] Size of this: " << pkg->len;
			}
			break;
			case PKG_ESP_WEIGHT_INFO:
			{
				cout << console::color::AQUA << "[CLIENT] ESP32 got weight info:";
				cout << console::color::AQUA << "[CLIENT] Good: " << pkg->data.esp.weight_info.good_side_kg << " kg";
				cout << console::color::AQUA << "[CLIENT] Bad:  " << pkg->data.esp.weight_info.bad_side_kg << " kg";
			}
			break;
			default:
			break;
			}
		}

	}
	else {
		cout << "[CLIENT] Trying to connect to localhost...";

		for (int tries = 0; tries < 10 && !client.setup(socket_config().set_port(ESP_HOST_PORT).set_ip_address(ESP_IP_ADDRESS).set_family(socket_config::e_family::IPV4)); tries++) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			cout << "[CLIENT] Trying again to connect to localhost...";
		}

		if (!client.has_socket()) {
			cout << console::color::RED << "[CLIENT] Failed to connect. ABORT!";
			std::this_thread::sleep_for(std::chrono::seconds(3));
			std::terminate();
		}
	}
}*/