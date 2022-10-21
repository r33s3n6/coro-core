#ifndef UTILS_WAIT_QUEUE_H
#define UTILS_WAIT_QUEUE_H

#include <utils/list.h>

class sleepable {
    public:
    virtual void sleep() = 0;
    virtual void wake_up() = 0;
};

class wait_queue {
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
};



#endif