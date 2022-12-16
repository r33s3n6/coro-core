#include "test_runner.h"

#include <test/coroutine/bdev_rw.hpp>




void run_tests(void*) {

    // pt_malloc test

    test::coroutine::test_bdev_rw test1(ramdisk_id, 1024, 190);
    test1.run();
    test1.print();


}