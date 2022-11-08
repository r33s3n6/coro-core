#ifndef UTILS_WAIT_QUEUE_H
#define UTILS_WAIT_QUEUE_H

#include <utils/list.h>
#include "sleepable.h"

#include <coroutine.h>



class wait_queue {
    public:
    struct wait_queue_done {
        wait_queue* wq;
        task_base caller;
        spinlock& lock;
        wait_queue_done(wait_queue* wq, spinlock& lock) : wq(wq), lock(lock) {}
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
            
            __sync_synchronize();

            // switch back to scheduler
            return std::noop_coroutine();
        }
        void await_resume() {
            
            if (!caller.get_promise()->no_yield) {
                __sync_synchronize();
            }

            lock.lock();


        }
    };
    private:
    list<sleepable*> sleepers;

    public:
    void sleep(sleepable* sleeper) {
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

    wait_queue_done done(spinlock& lock) {
        return {this, lock};
    }
};


class single_wait_queue {
    public:
    struct wait_queue_done {
        single_wait_queue* wq;
        task_base caller;
        spinlock& lock;
        wait_queue_done(single_wait_queue* wq, spinlock& lock) : wq(wq), lock(lock) {}
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
            
            __sync_synchronize();

            // switch back to scheduler
            return std::noop_coroutine();
        }
        void await_resume() {
            
            if (!caller.get_promise()->no_yield) {
                __sync_synchronize();
            }

            lock.lock();
        }
    };
    private:
    sleepable* sleeper;

    public:
    void sleep(sleepable* sleeper) {
        this->sleeper = sleeper;
        sleeper->sleep();
    }

    void wake_up() {
        if (sleeper) {
            sleeper->wake_up();
            sleeper = nullptr;
        }
    }

    wait_queue_done done(spinlock& lock) {
        return {this, lock};
    }
};




#endif