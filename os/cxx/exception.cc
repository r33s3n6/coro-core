namespace std{
    void __throw_length_error(char const*) {
        __builtin_unreachable();
    }
    void __throw_bad_alloc() {
        __builtin_unreachable();
    }
    void __throw_bad_array_new_length() {
        __builtin_unreachable();
    }
    struct nothrow_t {
        explicit nothrow_t() = default;
    };
    nothrow_t nothrow;
}

//TODO
int threadid(){
    return 0;
}

int cpuid()
{
	return 0;
}