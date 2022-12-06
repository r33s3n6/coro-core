#include "buf.h"

#include <utils/log.h>
#include <utils/assert.h>

block_buffer_t kernel_block_buffer;


task<void> block_buffer_node::__load() {
    // debugf("block_buffer: load node %d", block_no);
    co_await bdev->read(block_no, 1, data);
    co_return task_ok;
}
task<void> block_buffer_node::__flush() {
    // debugf("block_buffer: flush node %d", block_no);
    co_await bdev->write(block_no, 1, data);
    co_return task_ok;
}

void block_buffer_node::print() {
    debugf("block_buffer_node: block_no: %d, bdev: %p", block_no, bdev);
}


