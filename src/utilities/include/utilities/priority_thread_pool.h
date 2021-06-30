/***************************************************************************
 *  priority_thread_pool.h - A thread pool with a priority queue
 *
 *  Created:   Wed 10 Mar 22:57:22 CET 2021
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

#ifndef SRC_UTILITIES_INCLUDE_UTILITIES_PRIORITY_THREAD_POOL_H
#define SRC_UTILITIES_INCLUDE_UTILITIES_PRIORITY_THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace utilities {

/** Compare a pair by only comparing the first element of the pair; the second element is ignored.
 * This is a helpful comparator if the second element is incomparable.
 * @tparam T1 The type of the first element in the pair
 * @tparam T2 The type of the cond element in the pair
 */
template <typename T1, typename T2>
struct CompareFirstOfPair
{
	/** Compare the two pairs.
	 * @param pair1 The first pair
	 * @param pair2 The second pair
	 * @return True if the first element of the first pair is smaller than the first element of the
	 * second pair.
	 */
	bool
	operator()(const std::pair<T1, T2> &pair1, const std::pair<T1, T2> &pair2) const
	{
		return pair1.first < pair2.first;
	}
};

template <class Priority, class T>
class QueueAccess;

/** A multi-threaded priority queue with a fixed number of workers.
 * @tparam Priority The priority type
 * @tparam T The job type, must be a Callable
 */
template <class Priority = int, class T = std::function<void()>>
class ThreadPool
{
	friend class QueueAccess<Priority, T>;

public:
	/** Flag whether the pool shall be started on initialization. */
	enum class StartOnInit {
		NO,
		YES,
	};
	/** Construct a thread pool.
	 * @param start Whether the pool shall be started on initialization
	 * @param num_threads The number of threads in the pool
	 */
	ThreadPool(StartOnInit start       = StartOnInit::YES,
	           std::size_t num_threads = std::thread::hardware_concurrency());
	/** Stop and destruct the pool. This will stop all workers. */
	virtual ~ThreadPool();
	/** Add a job to the pool.
	 * @param job A pair (priority, job), where job is a Callable.
	 */
	void add_job(std::pair<Priority, T> &&job);
	/** Add a job to to the pool.
	 * @param job The job to run, must be a Callable
	 * @param priority The priority of the job, the job with the highest priority is run first
	 */
	void add_job(T &&job, const Priority &priority = Priority{});
	/** Start the workers in the pool. */
	void start();
	/** Stop the workers. They will finish their current job, but not necessarily process all jobs in
	 * the queue. */
	void cancel();
	/** Do not allow new jobs to the queue. */
	void close_queue();
	/** Wait until all tasks have completed. */
	void wait();
	/** Close the queue and let the workers finish all jobs. */
	void finish();

private:
	std::size_t              size;
	bool                     started{false};
	std::vector<std::thread> workers;
	std::priority_queue<std::pair<Priority, T>,
	                    std::vector<std::pair<Priority, T>>,
	                    CompareFirstOfPair<Priority, T>>
	                        queue;
	std::atomic_bool        stopping{false};
	std::atomic_bool        queue_open{true};
	std::mutex              queue_mutex;
	std::condition_variable queue_cond;
	std::vector<bool>       worker_idle;
	std::condition_variable worker_idle_cond;
	std::mutex              worker_idle_mutex;
};

/** Get direct access to the job of a thread pool.
 * The ThreadPool must not be running, i.e., you should use either the ThreadPool's workers, or
 * start the ThreadPool with StartOnInit::NO and access the queue manually. Direct queue access is
 * mainly helpful for testing and for single-threaded, synchronous queue processing.
 * @tparam Priority The priority type
 * @tparam T The job type, must be a Callable
 */
template <class Priority, class T>
class QueueAccess
{
public:
	/** Get access to a ThreadPool. The lifetime of this QueueAcess must not exceed the pool's
	 * lifetime.
	 * @param pool The pool to access
	 */
	QueueAccess(ThreadPool<Priority, T> *pool);
	/** Get the first element of the pool.
	 * @return The first element of the pool's queue.
	 */
	const std::pair<Priority, T> &top() const;
	/** Remove the first element of the pool's queue. */
	void pop();
	/** Check if the pool's queue is empty. */
	bool empty() const;

	/** Get the size of the queue. */
	std::size_t get_size() const;

private:
	ThreadPool<Priority, T> *pool;
};

} // namespace utilities

#include "priority_thread_pool.hpp"

#endif /* ifndef SRC_UTILITIES_INCLUDE_UTILITIES_PRIORITY_THREAD_POOL_H */
