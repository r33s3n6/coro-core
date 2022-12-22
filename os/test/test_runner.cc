#include "test_runner.h"

#include <test/coroutine/bdev_rw.hpp>

#include <test/coroutine/alloc.hpp>


void run_tests(void*) {

    // pt_malloc test

    // test::coroutine::test_bdev_rw test1(ramdisk_id, 1024, 190);
    // test1.run();
    // test1.print();
    test::coroutine::test_alloc test1(1);
    test1.run();

    debugf("test1 done\n");

    test::coroutine::test_alloc test2(11);
    test2.run();

    debugf("test2 done\n");

}