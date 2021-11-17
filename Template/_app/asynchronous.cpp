#include "asynchronous.h"

AsyncSinglePoll::~AsyncSinglePoll()
{
	if (async.exists() && !async.has_ended())
		async.ended.get();
}

void AsyncSinglePoll::launch(const std::function<void(void)> f)
{
	if (async.exists() && !async.has_ended())
		async.ended.get();

	async = throw_thread(f);
}

bool AsyncSinglePoll::tasking() const
{
	return async.exists() && !async.has_ended();
}
