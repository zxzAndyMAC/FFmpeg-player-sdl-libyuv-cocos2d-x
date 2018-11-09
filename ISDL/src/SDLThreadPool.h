
#ifndef _SDLTHREAD_H_
#define _SDLTHREAD_H_

#include <stdlib.h>
#include <functional>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>

namespace SDL 
{

	class SDLThreadPool
	{
	public:
		SDLThreadPool(int threads = 4)
			: _stop(false)
		{
			for (int index = 0; index < threads; ++index)
			{
				_workers.emplace_back(std::thread(std::bind(&SDLThreadPool::threadFunc, this)));
			}
		}

		void addTask(const std::function<void()> &task) {
			std::unique_lock<std::mutex> lk(_queueMutex);
			_taskQueue.emplace(task);
			_taskCondition.notify_one();
		}

		~SDLThreadPool()
		{
			{
				std::unique_lock<std::mutex> lk(_queueMutex);
				_stop = true;
				_taskCondition.notify_all();
			}

			for (auto&& worker : _workers) {
				worker.join();
			}
		}

	private:
		void threadFunc()
		{
			while (true) {
				std::function<void()> task = nullptr;
				{
					std::unique_lock<std::mutex> lk(_queueMutex);
					if (_stop)
					{
						break;
					}
					if (!_taskQueue.empty())
					{
						task = std::move(_taskQueue.front());
						_taskQueue.pop();
					}
					else
					{
						_taskCondition.wait(lk);
						continue;
					}
				}

				task();
			}
		}

		std::vector<std::thread>  _workers;
		std::queue< std::function<void()> > _taskQueue;

		std::mutex _queueMutex;
		std::condition_variable _taskCondition;
		bool _stop;
	};
}

#endif
