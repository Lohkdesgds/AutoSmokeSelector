#include <Lunaris/all.h>
#include "../_shared/package_definition.h"
#include "_app/storage.h"
#include "_app/asynchronous.h"
#include "_app/communication.h"
#include "_app/graphics_auto.h"

using namespace Lunaris;

#ifdef ASS_LOCALIP
const std::string ip = "127.0.0.1";
#else
const std::string ip = "192.168.4.1";
#endif

int main()
{
	set_app_name("AutoSmokeSelector APP");

	Storage store(get_default_config_path());
	Communication commu(store);
	GraphicInterface graphics(store, commu);
	std::atomic_bool keep_going = true;
	
	graphics.start([&] {keep_going = false; });
	commu.start_sync(ip, [&]()->bool {return keep_going; });

	timed_bomb bmb([] { std::terminate(); }, 5.0);
	store.conf.flush();

	commu.stop();
	graphics.stop();

	bmb.defuse();

	return 0;
}