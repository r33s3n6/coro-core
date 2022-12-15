#include "test_runner.h"

#include <test/coroutine/bdev_rw.hpp>



void run_tests(void*) {
    test::coroutine::test_bdev_rw test1(virtio_disk_id, 1024, 100);
    test1.run();
    test1.print();
}