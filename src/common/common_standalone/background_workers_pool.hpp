#pragma once

#include "check.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Implementation of the simple job queue with pool of background threads.
// Template parameters are type of the job object and type of the result object.
template <typename TJob, typename TResult>
class BackgroundWorkersPool
{
protected:
  BackgroundWorkersPool(int pool_size) : abort_processing_(false)
  {
    // Create all threads right away and let them wait until first job is pushed.
    for (int i = 0; i < pool_size; i++)
    {
      threads_.emplace_back(&BackgroundWorkersPool::BackgroundProcessingLoop, this, i);
    }
  }

  virtual ~BackgroundWorkersPool()
  {
    // We need to be aborted before destructor.
    CHECK(threads_.size() == 0, "Must have 0 threads in BackgroundWorkersPool destructor, %d left.", threads_.size());
  }

  // Stops all processing in background threads and clears all internal data. Object should not be used after this call.
  // Must be called prior destructor.
  void AbortAll()
  {
    // Signal all threads to stop processing.
    std::unique_lock<std::mutex> jobs_lock(jobs_mutex_);
    abort_processing_ = true;
    jobs_lock.unlock();
    jobs_notifier_.notify_all();
    // Wait all threads to finish.
    for (size_t i = 0; i < threads_.size(); i++)
    {
      threads_[i].join();
    }
    // Clear all the owned objects.
    threads_.clear();
    jobs_queue_ = {};
    results_queue_ = {};
  }

  // Returns one result.
  TResult PopResult()
  {
    // Synchronize access since background thread may be pushing result to the queue.
    std::unique_lock<std::mutex> results_lock(results_mutex_);
    if (results_queue_.empty())
    {
      // No results available, wait background thread to push some result.
      results_notifier_.wait(results_lock, [this] { return !results_queue_.empty(); });
    }

    // We must have result here, take it.
    CHECK(!results_queue_.empty(), "Results queue empty.");
    TResult result = results_queue_.front();
    results_queue_.pop();
    results_lock.unlock();

    return result;
  }

  // Provides access to one result without popping it.
  TResult PeekResult()
  {
    std::unique_lock<std::mutex> results_lock(results_mutex_);
    if (results_queue_.empty())
    {
      // No results available, wait background thread to push some result.
      results_notifier_.wait(results_lock, [this] { return !results_queue_.empty(); });
    }

    TResult result = results_queue_.front();
    results_lock.unlock();

    return result;
  }

  // Adds new job to the queue.
  void PushJobWithCopy(TJob job)
  {
    PushJobWithMove(std::move(job));
  }

  // Moves new job to the queue.
  void PushJobWithMove(TJob&& job)
  {
    // Synchronize access since background threads may be taking job from the queue.
    std::unique_lock<std::mutex> jobs_lock(jobs_mutex_);
    jobs_queue_.emplace(std::move(job));
    jobs_lock.unlock();
    // Notify background threads that new job is available.
    jobs_notifier_.notify_all();
  }

private:
  // main background threads processing loop.
  void BackgroundProcessingLoop(int thread_id)
  {
    for (;;)
    {
      std::unique_lock<std::mutex> jobs_lock(jobs_mutex_);
      if (jobs_queue_.empty() || !abort_processing_)
      {
        // Wait other thread to add job. Wait releases mutex while waiting and locks once wait is over.
        jobs_notifier_.wait(jobs_lock, [this] { return !jobs_queue_.empty() || abort_processing_; });
      }
      // Here we must have job to do, take it.
      if (abort_processing_)
      {
        break;
      }

      TJob job = std::move(jobs_queue_.front());
      jobs_queue_.pop();
      // Done taking the job, release lock.
      jobs_lock.unlock();

      // Perform processing.
      TResult result = BackgroundProcesingMethod(job, thread_id);

      // Push result to the queue, synchronize since other threads may be pushing as well.
      std::unique_lock<std::mutex> results_lock(results_mutex_);
      results_queue_.push(std::move(result));
      results_lock.unlock();
      // Notify that we have new result available.
      results_notifier_.notify_all();
    }
  }

  // Background processing method, should be implemented by derived classes.
  virtual TResult BackgroundProcesingMethod(TJob& job, int thread_id) = 0;

private:
  std::vector<std::thread> threads_;
  bool abort_processing_;

  std::mutex jobs_mutex_;
  std::condition_variable jobs_notifier_;
  std::queue<TJob> jobs_queue_;

  std::mutex results_mutex_;
  std::condition_variable results_notifier_;
  std::queue<TResult> results_queue_;
};
