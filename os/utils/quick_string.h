#ifndef UTILS_QUICK_STRING_H
#define UTILS_QUICK_STRING_H

#include <ccore/types.h>
#include <atomic/spinlock.h>
#include <mm/utils.h>
#include <utils/utility.h>

#include <utility>

class quick_string_ref;

class quick_string : noncopyable {
    const char* _data = nullptr;
    uint32 _size = 0;
    uint32 _hash = -1;

    public:
    quick_string(const char* data, uint32 size) {
        init(data, size);
    }
    quick_string(const char* data) {
        init(data, strlen(data));
    }
    quick_string() : _data(nullptr), _size(0) {}

    quick_string( quick_string&& other) {
        _data = other._data;
        _size = other._size;
        _hash = other._hash;
        other._data = nullptr;
        other._size = 0;
        other._hash = -1;
    }

    quick_string& operator=(quick_string&& other) {
        std::swap(_data, other._data);
        _size = other._size;
        _hash = other._hash;
        return *this;
    }

    void init(const char* data, uint32 size) {
        destroy();
        _size = size;
        _data = strndup(data, size);
        _hash = __hash(_data);
    }

    void destroy() {
        if (_data) {
            delete[] _data;
            _data = nullptr;
        }
    }
 

    ~quick_string() {
        destroy();
    }

    const char* data() const { return _data; }
    uint32 size() const { return _size; }

    uint32 hash() const { return _hash; }

    static uint32 __hash(const char* name) {
        uint32 hash = 0;
        while (*name) {
            hash = hash * 131 + *name;
            name++;
        }
        return hash >> 1;
    }

    private:
    friend class quick_string_ref;
    quick_string(const char* data, uint32 size, uint32 hash) : _size(size), _hash(hash) {
        _data = strndup(data, size);
    }


};

class quick_string_ref {
    const char* _data = nullptr;
    uint32 _size = 0;
    uint32 _hash = -1;
    
    public:
    quick_string_ref(const char* data) {
        _data = data;
        _size = strlen(data);
        _hash = quick_string::__hash(data);
    }

    const char* data() const { return _data; }
    uint32 size() const { return _size; }
    uint32 hash() const { return _hash; }

    quick_string to_string() const {
        return quick_string(_data, _size, _hash);
    }

};

#endif // UTILS_QUICK_STRING_H
