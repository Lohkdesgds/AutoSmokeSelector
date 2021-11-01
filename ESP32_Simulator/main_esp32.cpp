#include <Lunaris/all.h>

#include "../_shared/package_definition.h"

#include <conio.h>

using namespace Lunaris;

#define MYASST(X, MSGGG) if (!(X)) {Lunaris::cout << Lunaris::console::color::RED << MSGGG; Lunaris::cout << Lunaris::console::color::RED << "AUTOMATIC ABORT BEGUN"; client.close_socket(); continue; }

struct file_transf {
	std::vector<std::string> files;
	std::mutex muu;
};

int main()
{
	cout << console::color::GREEN << "Starting ESP32 communication protocol tester...";

	TCP_client client;
	file_transf __shrd;
	std::string __curr_working; // if empty and __shrd.files.size(), cut from __shrd.files and put here. When end, clear().
	file __curr_file;	// holds __curr_working

	bool gud = client.setup(socket_config().set_port(ESP_HOST_PORT).set_family(socket_config::e_family::IPV4));

	if (!gud) {
		cout << console::color::RED << "Bad news for you: can't connect to localhost.";
		return 0;
	}
	else cout << console::color::GREEN << "Connected!";


	thread read_usr([&] {
		std::string filecatch;

		cout << console::color::LIGHT_PURPLE << "Console is ready to receive file drop now.";

		do {
			if (filecatch.size() > 0) cout << console::color::LIGHT_PURPLE << "\nPlease drop the file here, do not type manually.";
			filecatch.clear();

			while (!_kbhit()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
			while (_kbhit()) {
				int val = _getch();
				filecatch += val;
				putchar(val);
			}
		} while (filecatch.size() < 2);

		cout << console::color::LIGHT_PURPLE << "\nThinking...";

		{
			std::lock_guard<std::mutex> luu(__shrd.muu);
			file test;

			if (filecatch.size() && filecatch.front() == '\"') filecatch.erase(filecatch.begin());
			if (filecatch.size() && filecatch.back() == '\"') filecatch.pop_back();

			if (test.open(filecatch, file::open_mode_e::READ_TRY)) {
				test.close();
				__shrd.files.push_back(filecatch);
				cout << console::color::LIGHT_PURPLE << "So the current file now is '" << filecatch << "'";
			}
			else {
				cout << console::color::LIGHT_PURPLE << "Hmmm I failed to add '" << filecatch << "'. Path is weird?";
			}
		}
	}, thread::speed::HIGH_PERFORMANCE);

	while (client.has_socket()) {
		// ############################################
		// # RECV
		// ############################################

		esp_package pkg, spkg;
		MYASST(client.recv(pkg), "RECV FAILED! Abort!");

		// ############################################
		// # SEND
		// ############################################

		switch (pkg.type) {
		case PKG_PC_OPTION_FINAL:
		{
			cout << console::color::GREEN << "[FAKE_ESP32] PKG_PC_OPTION_FINAL: " << (pkg.data.pc.option_final.good == PLANT_GOOD ? "GOOD" : "BAD");
			MYASST(build_both_empty(&spkg) > 0, "Can't build empty package!");
			MYASST(client.send((char*)&spkg, sizeof(spkg)), "Can't send empty package properly");
		}
		break;
		case PKG_PC_REQUEST_WEIGHT:
		{
			cout << console::color::GREEN << "[FAKE_ESP32] PKG_PC_REQUEST_WEIGHT called. Returning numbers...";

			MYASST(build_esp_weight_info(&spkg, (random() % 100000) * 0.001f, (random() % 100000) * 0.001f) >= 0, "Failed to build build_esp_weight_info");

			cout << console::color::GREEN << "[FAKE_ESP32] PKG_PC_REQUEST_WEIGHT are: {" << spkg.data.esp.weight_info.bad_side_kg << " kg bad; " << spkg.data.esp.weight_info.good_side_kg << " kg good}";

			MYASST(client.send((char*)&spkg, sizeof(spkg)), "Can't send empty package properly");
		}
		break;
		default:
		{
			std::lock_guard<std::mutex> safy(__shrd.muu);

			if (__curr_working.length()) 
			{
				char buf[PACKAGE_SIZE];
				unsigned long rd = static_cast<unsigned long>(__curr_file.read(buf, PACKAGE_SIZE));
				if (rd > 0) {
					if (build_esp_each_jpg_data(&spkg, (char**)&buf, &rd) > 0)
					{
						MYASST(client.send((char*)&spkg, sizeof(spkg)), "Can't send empty package properly");
					}
					else {
						__curr_working.clear();
						__curr_file.close();
						MYASST(build_both_empty(&spkg) > 0, "Can't build empty package!");
						MYASST(client.send((char*)&spkg, sizeof(spkg)), "Can't send empty package properly");
					}
				}
				else {
					__curr_working.clear();
					__curr_file.close();
					MYASST(build_both_empty(&spkg) > 0, "Can't build empty package!");
					MYASST(client.send((char*)&spkg, sizeof(spkg)), "Can't send empty package properly");
				}
			}
			else if (__shrd.files.size()) {
				__curr_working = __shrd.files.front();
				__shrd.files.erase(__shrd.files.begin());
				bool gud = __curr_file.open(__curr_working, file::open_mode_e::READ_TRY);
				if (!gud) {
					cout << console::color::RED << "Failed to open previously selected file '" << __curr_working << "'. Skipped.";
					__curr_working.clear();
				}
				else {
					cout << console::color::RED << "About to send '" << __curr_working << "'...";
				}
				MYASST(build_esp_has_something(&spkg) > 0, "Can't build has something package!");
				MYASST(client.send((char*)&spkg, sizeof(spkg)), "Can't send has something package properly");
			}

		}
		break;
		}

	}

	read_usr.force_kill();
	cout << console::color::YELLOW << "Disconnected.";

	{
		std::string _buf;
		std::getline(std::cin, _buf);
	}

	return 0;
}