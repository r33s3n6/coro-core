#ifndef UTILS_SHARED_PTR_H
#define UTILS_SHARED_PTR_H

#include <atomic/lock.h>
#include <type_traits>





struct ref_count_t {
    uint32 ref_count;
    uint32 weak_count;
    spinlock lock{"shared_ptr.lock"};
    ref_count_t(int init) : ref_count(init), weak_count(init) {}
    void inc_ref() {
        auto guard = make_lock_guard(lock);
        ref_count++;
        weak_count++;
    }
    std::pair<bool,bool> dec_ref() {
        auto guard = make_lock_guard(lock);
        return {--ref_count,--weak_count};
    }
    uint32 get_ref(){
        auto guard = make_lock_guard(lock);
        return ref_count;
    }
    void inc_weak() {
        auto guard = make_lock_guard(lock);
        weak_count++;
    }
    uint32 dec_weak() {
        auto guard = make_lock_guard(lock);
        return --weak_count;
    }

    // used by weak_ptr
    bool try_inc_ref(){
        auto guard = make_lock_guard(lock);
        if(ref_count == 0) return false;
        ref_count++;
        weak_count++;
        return true;
    }

    // used by try_detach_weak
    // first bool: if detach is successful
    // second bool: if ref_count is to be destroyed
    std::pair<bool,bool> try_dec_ref(){
        auto guard = make_lock_guard(lock);
        if(ref_count > 1) return {false, false};
        ref_count--;
        weak_count--;
        return {true, weak_count==0};
    }

};

template <typename T>
class weak_ptr;

template <typename T, typename = void>
struct has_on_destroy_fn
    : std::false_type
{};


template <typename T>
struct has_on_destroy_fn<T,
    std::void_t<decltype(std::declval<T>().on_destroy(std::declval<weak_ptr<T>>()))>>
    : std::true_type
{};

template <typename T>
class shared_ptr {
   private: 
    T* ptr = nullptr;
    ref_count_t* ref_count = nullptr;

   public:

    constexpr explicit shared_ptr(T* ptr) { reset(ptr); }
    constexpr shared_ptr(std::nullptr_t p=nullptr) { reset(p); }
    
    ~shared_ptr() {
        __dec_ref();
    }

    explicit operator bool() const { return ptr != nullptr; }
    T* operator->() { return ptr; }
    const T* operator->() const { return ptr; }
    T& operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }
    bool operator==(const shared_ptr& other) { return ptr == other.ptr; }
    bool operator!=(const shared_ptr& other) { return ptr != other.ptr; }
    bool operator!() { return !ptr; }
    T* get() { return ptr; }
    const T* get() const { return ptr; }


    template <typename other_type>
    friend class shared_ptr;

    template <typename other_type>
    friend class weak_ptr;
    
    
    constexpr shared_ptr(const shared_ptr& other) noexcept {
        __reset(other.ptr, other.ref_count);
    }

    constexpr shared_ptr(shared_ptr&& other) noexcept {
        std::swap(ptr, other.ptr);
        std::swap(ref_count, other.ref_count);
    }

    template <typename other_type,
              typename = typename std::enable_if_t<
                  std::is_convertible_v<other_type*, T*>>>
    constexpr shared_ptr(const shared_ptr<other_type>& other) noexcept {

        __reset(other.ptr, other.ref_count);
    }

    template <typename other_type,
              typename = typename std::enable_if_t<
                  std::is_convertible_v<other_type*, T*>>>
    operator shared_ptr<other_type>() noexcept {

        return {(other_type*)ptr, ref_count};
    }


    constexpr shared_ptr& operator=(const shared_ptr& other) noexcept {

        __reset(other.ptr, other.ref_count);
        return *this;
    }

    constexpr shared_ptr& operator=(shared_ptr&& other) noexcept {

        std::swap(ptr, other.ptr);
        std::swap(ref_count, other.ref_count);
        return *this;
    }




    template <typename other_type,
              typename = typename std::enable_if_t<
                  std::is_convertible_v<other_type*, T*>>>
    constexpr shared_ptr<T>& operator=(const shared_ptr<other_type>& other) noexcept {

        __reset(other.ptr, other.ref_count);
        return *this;
    }

    weak_ptr<T> get_weak() const {
        return weak_ptr<T>(ptr, ref_count);
    }


    void reset(T* new_ptr = nullptr) {
        __dec_ref();

        ptr = new_ptr;
        if (ptr) {
            ref_count = new ref_count_t(1);
        } else {
            ref_count = nullptr;
        }

        // __inc_ref();
    }

    bool try_detach_weak() {
        if (ptr && ref_count) {
            auto [detach, destroy] = ref_count->try_dec_ref();
            if (detach) {
                if (destroy) {
                    delete ref_count;
                }
                ref_count = new ref_count_t(1);
                return true;
            }
        }
        return false;
    }

    uint32 use_count(){
        if(ref_count) return ref_count->get_ref();
        return 0;
    }



    private:
    void __dec_ref() {
        if (ptr && ref_count) {
            
            if constexpr (has_on_destroy_fn<T>::value) {
                if (try_detach_weak()) { // detach old ptr
                    ptr->on_destroy(get_weak());
                }
            } 

            auto [ref,weak] = ref_count->dec_ref();
            if (!ref) {
                delete ptr;
            }
            if (!weak) {
                delete ref_count;
            }


        }
    }
    void __inc_ref() {
        if (ptr && ref_count) {
            ref_count->inc_ref();
        }
    }

    void __reset(T* new_ptr, ref_count_t* new_ref_count) {
        __dec_ref();

        ptr = new_ptr;
        ref_count = new_ref_count;

        __inc_ref();
    }


    void __reset_weak(T* new_ptr, ref_count_t* new_ref_count) {
        __dec_ref();

        ptr = new_ptr;
        ref_count = new_ref_count;

        if(!ref_count->try_inc_ref()){
            ptr = nullptr;
            ref_count = nullptr;
        }
    }
    // used by weak_ptr
    constexpr shared_ptr(T* new_ptr, ref_count_t* new_ref_count) noexcept {
        __reset_weak(new_ptr, new_ref_count);
    }
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args... args) {
    return shared_ptr<T>(new T(args...));
}

template <typename T>
class weak_ptr {
   private: 
    T* ptr = nullptr;
    ref_count_t* ref_count = nullptr;

   public:
    constexpr weak_ptr() noexcept = default;

    constexpr weak_ptr(std::nullptr_t) noexcept {}

    constexpr weak_ptr(shared_ptr<T>& other) {
        __reset(other.ptr, other.ref_count);
    }

    constexpr weak_ptr(T* ptr, ref_count_t* ref_count) {
        __reset(ptr, ref_count);
    }
    // constexpr weak_ptr(T* ptr = nullptr) { reset(ptr); }
    
    ~weak_ptr() {
        __dec_ref();
    }

    explicit operator bool() const { return ptr != nullptr; }
    bool operator==(const weak_ptr& other) { return ptr == other.ptr; }
    bool operator!=(const weak_ptr& other) { return ptr != other.ptr; }
    bool operator!() { return !ptr; }


    template <typename other_type>
    friend class weak_ptr;
    
    
    constexpr weak_ptr(const weak_ptr& other) noexcept {
        __reset(other.ptr, other.ref_count);
    }

    template <typename other_type,
              typename = typename std::enable_if_t<
                  std::is_convertible_v<other_type*, T*>>>
    constexpr weak_ptr(const weak_ptr<other_type>& other) noexcept {

        __reset(other.ptr, other.ref_count);
    }


    constexpr weak_ptr& operator=(const weak_ptr& other) noexcept {

        __reset(other.ptr, other.ref_count);
        return *this;
    }


    template <typename other_type,
              typename = typename std::enable_if_t<
                  std::is_convertible_v<other_type*, T*>>>
    constexpr weak_ptr<T>& operator=(const weak_ptr<other_type>& other) noexcept {

        __reset(other.ptr, other.ref_count);
        return *this;
    }

    bool expired() const {
        return !ref_count || ref_count->get_ref() == 0;
    }

    shared_ptr<T> lock() {
        if (ref_count) {
            return {ptr, ref_count};
        }
        return {};
    }

    private:
    void __dec_ref() {
        if (ref_count) {
            auto weak = ref_count->dec_weak();
            if (!weak) {
                delete ref_count;
            }
        }
    }
    void __inc_ref() {
        if (ref_count) {
            ref_count->inc_weak();
        }
    }

    void __reset(T* new_ptr, ref_count_t* new_ref_count) {
        __dec_ref();

        ptr = new_ptr;
        ref_count = new_ref_count;

        __inc_ref();
    }
};

#endif