#pragma once
#include <deque>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <mutex>
#include <utility>
namespace life {
// Work runs only on the coordinator under its world lock. Replies run only on
// the UI thread, after that lock has been released. No UI wait on simulation.
class UiCommands {
public:
  using Reply=std::function<void()>;
  using Work=std::function<Reply()>;
private:
  std::mutex mutex_;std::condition_variable wake_;
  std::deque<Work> pending_;
  std::deque<Reply> replies_;
public:
  void submit(Work work){std::lock_guard lock(mutex_);pending_.push_back(std::move(work));wake_.notify_one();}
  void execute(){std::deque<Work> work;{std::lock_guard lock(mutex_);work.swap(pending_);}for(auto& action:work){auto reply=action();if(reply){std::lock_guard lock(mutex_);replies_.push_back(std::move(reply));}}}
  void waitUntil(std::chrono::steady_clock::time_point deadline){std::unique_lock lock(mutex_);wake_.wait_until(lock,deadline,[&]{return !pending_.empty();});}
  bool pending(){std::lock_guard lock(mutex_);return !pending_.empty();}
  void complete(){std::deque<Reply> replies;{std::lock_guard lock(mutex_);replies.swap(replies_);}for(auto& reply:replies)reply();}
};
}
