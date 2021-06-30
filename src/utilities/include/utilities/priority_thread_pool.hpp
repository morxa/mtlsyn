/***************************************************************************
 *  priority_thread_pool.hpp - A thread pool with a priority queue
 *
 *  Created:   Wed 10 Mar 23:06:29 CET 2021
 *  Copyright  2021  Till Hofmann <hofmann@kbsg.rwth-aachen.de>
 ****************************************************************************/
/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.md file.
 */

#pragma once

#include "priority_thread_pool.h"

#include <mutex>

namespace utilities {

class QueueClosedException : public std::logic_error
{
	// Use base class constructors.
	using std::logic_error::logic_error;
};

class QueueStartedException : public std::logic_error
{
	// Use base class constructors.
	using std::logic_error::logic_error;
};

template <class Priority, class T>
ThreadPool<Priority, T>::ThreadPool(StartOnInit start_on_init, std::size_t size) : size(size)
{
	if (start_on_init == StartOnInit::YES) {
		start();
	}
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::start()
{
	if (started) {
		throw QueueStartedException("Pool already started");
	}
	worker_idle = std::vector(size, false);
	for (std::size_t i = 0; i < size; ++i) {
		workers.push_back(std::thread{[this, i]() {
			while (!stopping) {
				{
					std::lock_guard idle_guard{worker_idle_mutex};
					worker_idle[i] = false;
				}
				std::unique_lock lock{queue_mutex};
				while (!queue.empty()) {
					auto job = std::get<1>(queue.top());
					queue.pop();
					lock.unlock();
					job();
					lock.lock();
					if (stopping) {
						return;
					}
				}
				{
					std::lock_guard done_guard{worker_idle_mutex};
					worker_idle[i] = true;
					worker_idle_cond.notify_all();
				}
				if (!queue_open) {
					return;
				}
				// Wait for the stop signal or a new job.
				queue_cond.wait(lock, [this] { return stopping || !queue.empty() || !queue_open; });
			}
		}});
	}
	started = true;
}

template <class Priority, class T>
ThreadPool<Priority, T>::~ThreadPool()
{
	cancel();
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::add_job(std::pair<Priority, T> &&job)
{
	if (!queue_open) {
		throw QueueClosedException("Queue is closed!");
	}
	std::lock_guard guard{queue_mutex};
	queue.push(job);
	queue_cond.notify_one();
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::add_job(T &&job, const Priority &priority)
{
	add_job(std::make_pair(priority, job));
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::close_queue()
{
	queue_open = false;
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::wait()
{
	std::unique_lock lock{worker_idle_mutex};
	while (true) {
		if (std::all_of(begin(worker_idle), end(worker_idle), [](const auto &worker_idle) {
			    return worker_idle;
		    })) {
			return;
		}
		worker_idle_cond.wait(lock);
	}
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::finish()
{
	close_queue();
	{
		std::lock_guard guard{queue_mutex};
		queue_cond.notify_all();
	}
	for (auto &worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
}

template <class Priority, class T>
void
ThreadPool<Priority, T>::cancel()
{
	stopping = true;
	finish();
}

template <class Priority, class T>
QueueAccess<Priority, T>::QueueAccess(ThreadPool<Priority, T> *pool) : pool(pool)
{
}

template <class Priority, class T>
const std::pair<Priority, T> &
QueueAccess<Priority, T>::top() const
{
	if (pool->started) {
		throw QueueStartedException("Pool already started");
	}
	return pool->queue.top();
}

template <class Priority, class T>
void
QueueAccess<Priority, T>::pop()
{
	if (pool->started) {
		throw QueueStartedException("Pool already started");
	}
	return pool->queue.pop();
}

template <class Priority, class T>
bool
QueueAccess<Priority, T>::empty() const
{
	if (pool->started) {
		throw QueueStartedException("Pool already started");
	}
	return pool->queue.empty();
}

template <class Priority, class T>
std::size_t
QueueAccess<Priority, T>::get_size() const
{
	if (pool->started) {
		throw QueueStartedException("Pool already started");
	}
	return pool->queue.size();
}

} // namespace utilities
