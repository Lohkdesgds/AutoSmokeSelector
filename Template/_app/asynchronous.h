#pragma once

#include <Lunaris/all.h>

using namespace Lunaris;

class AsyncSinglePoll : public NonCopyable {
	async_thread_info async;
public:
	~AsyncSinglePoll();
	void launch(const std::function<void(void)>);

	bool tasking() const;
};