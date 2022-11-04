#ifndef UTILS_SLEEPABLE_H
#define UTILS_SLEEPABLE_H

class sleepable {
    public:
    virtual void sleep() = 0;
    virtual void wake_up() = 0;
};

#endif