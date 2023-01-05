#include "test_runner.h"

#include <test/coroutine/bdev_rw.hpp>
#include <test/coroutine/alloc.hpp>
#include <test/coroutine/sleep.hpp>
#include <test/coroutine/sleep_task.hpp>

#include <test/process/sleep_task.hpp>
#include <test/process/sleep.hpp>

#include <test/nfs/shell.hpp>

void run_tests(void*) {

    // pt_malloc test

    // test::coroutine::test_bdev_rw test1(virtio_disk_id, 1024, 100);
    // test1.run();
    // test1.print();

    // test::coroutine::test_alloc test1(1);
    // test1.run();
// 
    // debugf("test1 done\n");
// 
    // test::coroutine::test_alloc test2(11);
    // test2.run();
// 
    // debugf("test2 done\n");

    // test::process::test_sleep test3(1);
    // test3.run();

    // test::coroutine::test_sleep test4(10);
    // test4.run();

    test::coroutine::test_sleep_task test5(1000, 100000);
    test5.run();
    test5.print();

    // test::process::test_sleep_task test5(1000, 100000);
    // test5.run();
    // test5.print();
    debugf("done");

    // auto bdev = device::get<block_device>(virtio_disk_id);
    // test::nfs::test_shell test6(bdev, 8192);
    // test6.run();
}