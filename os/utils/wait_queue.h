#ifndef UTILS_WAIT_QUEUE_H
#define UTILS_WAIT_QUEUE_H

#include <utils/list.h>
#include <atomic/spinlock.h>

#include "sleepable.h"

#include <coroutine.h>

#include <queue>

class wait_queue_base {
    public:
    struct wait_queue_done {
        wait_queue_base* wq;
        task_base caller;
        spinlock& lock;
        wait_queue_done(wait_queue_base* wq, spinlock& lock) : wq(wq), lock(lock) {}
        bool await_ready() const { 
            return false; 
        }
        std::coroutine_handle<> await_suspend(task_base h) {
            caller = std::move(h);

            promise_base* p = caller.get_promise();

            if(p->no_yield) {
                lock.unlock();
                return caller.get_handle(); // resume immediately
            }

            wq->sleep(&caller);

            lock.unlock();
            
            //__sync_synchronize();

            // switch back to scheduler
            return std::noop_coroutine();
        }
        void await_resume() {
            
            if (!caller.get_promise()->no_yield) {
               // __sync_synchronize();
            }

            lock.lock();


        }
    };

    virtual void sleep(sleepable* s) = 0;
    // virtual int32 size();
    void wait_done(sleepable* s, spinlock& lock);
    wait_queue_done done(spinlock& lock) {
        return {this, lock};
    }

};

class wait_queue : public wait_queue_base {

    private:
    std::deque<sleepable*> sleepers;

    public:
    void sleep(sleepable* sleeper) override {
        sleepers.push_back(sleeper);
        sleeper->sleep();
    }

    void wake_up_all() {
        for (auto sleeper : sleepers) {
            sleeper->wake_up();
        }
        sleepers.clear();
    }

    void wake_up_one() {
        if (sleepers.size() > 0) {
            sleepers.front()->wake_up();
            sleepers.pop_front();
        }
    }

    int32 size() {
        return sleepers.size();
    }

};


class single_wait_queue : public wait_queue_base {
    public:
    
    private:
    sleepable* sleeper = nullptr;

    public:
    void sleep(sleepable* sleeper) override  {
        this->sleeper = sleeper;
        sleeper->sleep();
    }

    void wake_up() {
        if (sleeper) {
            sleeper->wake_up();
            sleeper = nullptr;
        }
    }

    int32 size() {
        return sleeper ? 1 : 0;
    }


};




#endif