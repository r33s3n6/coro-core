#include <utils/panic.h>

namespace std{

    void __throw_bad_array_new_length() {
        panic("exception from standard library!\n");
    }




  // Helper for exception objects in <except>
  void
  __throw_bad_exception(void) {panic("exception from standard library!\n");}

  // Helper for exception objects in <new>
  void
  __throw_bad_alloc(void) {panic("exception from standard library!\n");}

  // Helper for exception objects in <typeinfo>
  void
  __throw_bad_cast(void) {panic("exception from standard library!\n");}

  void
  __throw_bad_typeid(void) {panic("exception from standard library!\n");}

  // Helpers for exception objects in <stdexcept>
  void
  __throw_logic_error(const char*) {panic("exception from standard library!\n");}

  void
  __throw_domain_error(const char*) {panic("exception from standard library!\n");}

  void
  __throw_invalid_argument(const char*) {panic("exception from standard library!\n");}

  void
  __throw_length_error(const char*) {panic("exception from standard library!\n");}

  void
  __throw_out_of_range(const char*) {panic("exception from standard library!\n");}

  void
  __throw_out_of_range_fmt(const char*, ...) {panic("exception from standard library!\n");}

  void
  __throw_runtime_error(const char*) {panic("exception from standard library!\n");}

  void
  __throw_range_error(const char*) {panic("exception from standard library!\n");}

  void
  __throw_overflow_error(const char*) {panic("exception from standard library!\n");}

  void
  __throw_underflow_error(const char*) {panic("exception from standard library!\n");}

  // Helpers for exception objects in <ios>
  void
  __throw_ios_failure(const char*) {panic("exception from standard library!\n");}

  void
  __throw_ios_failure(const char*, int) {panic("exception from standard library!\n");}

  // Helpers for exception objects in <system_error>
  void
  __throw_system_error(int) {panic("exception from standard library!\n");}

  // Helpers for exception objects in <future>
  void
  __throw_future_error(int) {panic("exception from standard library!\n");}

  // Helpers for exception objects in <functional>
  void
  __throw_bad_function_call() {panic("exception from standard library!\n");}

  void 
  __throw_bad_optional_access() {panic("exception from standard library!\n");}

    struct nothrow_t {
        explicit nothrow_t() = default;
    };
    nothrow_t nothrow;
}

