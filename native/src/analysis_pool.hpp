#pragma once
#include "frame_pacer.hpp"
#include <array>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace life {
// Three persistent helpers shared by at most two coordinators. Independent jobs
// interleave in small chunks; callers also help instead of waiting on idle cores.
class AnalysisPool {
  struct Job {
    std::function<void(size_t,size_t)> task;size_t size{};
    std::atomic_int next{0},remaining{64};std::mutex mutex;std::condition_variable done;std::exception_ptr error;
    bool runOne(){const int part=next.fetch_add(1,std::memory_order_relaxed);if(part>=64)return false;
      try{task(size*part/64,size*(part+1)/64);}catch(...){std::lock_guard lock(mutex);if(!error)error=std::current_exception();}
      if(remaining.fetch_sub(1,std::memory_order_acq_rel)==1){std::lock_guard lock(mutex);done.notify_all();}return true;
    }
  };
  std::array<std::shared_ptr<Job>,8> jobs_,active_;
  std::mutex mutex_;std::condition_variable wake_;size_t cursor_{};std::vector<std::jthread> threads_;
  bool runnableLocked()const{for(const auto& job:active_)if(job&&job->next.load(std::memory_order_relaxed)<64)return true;return false;}
  std::shared_ptr<Job> pickLocked(){for(size_t attempt=0;attempt<active_.size();++attempt){const size_t index=(cursor_+attempt)%active_.size();const auto& job=active_[index];if(job&&job->next.load(std::memory_order_relaxed)<64){cursor_=(index+1)%active_.size();return job;}}return {};}
  std::shared_ptr<Job> takeWork(){std::lock_guard lock(mutex_);return pickLocked();}
public:
  AnalysisPool(){for(auto& job:jobs_)job=std::make_shared<Job>();for(int i=0;i<3;++i)threads_.emplace_back([this](std::stop_token stop){FramePacer::deadlineWork();while(!stop.stop_requested()){std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stop.stop_requested()||runnableLocked();});if(stop.stop_requested())break;auto job=pickLocked();lock.unlock();if(job)job->runOne();}});}
  ~AnalysisPool(){{std::lock_guard lock(mutex_);for(auto& thread:threads_)thread.request_stop();}wake_.notify_all();for(auto& thread:threads_)thread.join();}
  void run(size_t count,const std::function<void(size_t,size_t)>& task){
    if(count<256){task(0,count);return;}
    std::shared_ptr<Job> job;size_t slot=0;
    {std::unique_lock lock(mutex_);for(const auto& candidate:jobs_)if(candidate.use_count()==1){job=candidate;break;}
      if(!job){lock.unlock();task(0,count);return;}
      while(slot<active_.size()&&active_[slot])++slot;
      if(slot==active_.size()){lock.unlock();task(0,count);return;}
      job->task=task;job->size=count;job->next=0;job->remaining=64;job->error=nullptr;active_[slot]=job;
    }wake_.notify_all();
    while(job->remaining.load(std::memory_order_acquire)>0){
      if(job->runOne())continue;
      if(auto other=takeWork()){other->runOne();continue;}
      std::unique_lock lock(job->mutex);job->done.wait(lock,[&]{return job->remaining.load(std::memory_order_acquire)==0;});
    }
    std::exception_ptr error;{std::lock_guard lock(job->mutex);error=job->error;}
    {std::lock_guard lock(mutex_);active_[slot].reset();}
    if(error)std::rethrow_exception(error);
  }
};
}

