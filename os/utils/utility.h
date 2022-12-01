#ifndef UTILS_UTILITY_H
#define UTILS_UTILITY_H

class noncopyable {
public:
    noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};

class nonmovable {
public:
    nonmovable() = default;
    nonmovable(nonmovable&&) = delete;
    nonmovable& operator=(nonmovable&&) = delete;
};


#endif