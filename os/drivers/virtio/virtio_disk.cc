#include "virtio_disk.h"

#include <utils/assert.h>
#include <utils/log.h>

// #define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

template <typename T>
inline volatile T& io(T& v) {
    return (*(volatile T*)(&v));
}

virtio_disk::virtio_disk(uint8* disk_pages) : block_device("virtio_disk"), disk_pages(disk_pages) {
    //rw_buffer = (rw_buffer_t*)kernel_io_allocator.alloc_page();
    //debugf("disk_pages: %p, rw_buffer: %p",disk_pages, (void*)rw_buffer);
}
virtio_disk::~virtio_disk() {
    //kernel_io_allocator.free_page((void*)rw_buffer);
}

int virtio_disk::open(void* base_address) {
    kernel_assert(base_address != nullptr,
                  "virtio_disk::open: base_address is null");
    kernel_assert(this->regs == nullptr, "virtio_disk::open: already opened");
    this->regs = (virtio_regs_t*)base_address;

    register_device({VIRTIO_DISK_MAJOR, VIRTIO_DISK_MINOR});

    uint32 status = 0;

    uint64 features;
    uint32 max;

    if (regs->magic_value != 0x74726976 || regs->version != 1 ||
        regs->device_id != 2 || regs->vendor_id != 0x554d4551) {
        warnf("virtio_disk::open: virtio disk not found: magic_value: %x, version: %x, device_id: %x, vendor_id: %x",
              regs->magic_value, regs->version, regs->device_id,
              regs->vendor_id);
        goto err;
    }

    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    regs->status = status;

    status |= VIRTIO_CONFIG_S_DRIVER;
    regs->status = status;

    // negotiate features
    features = regs->device_features;
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_FLUSH);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    regs->device_features = features;

    // tell device that feature negotiation is complete.
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    regs->status = status;


    // tell device we're completely ready.
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    regs->status = status;


    regs->guest_page_size = PGSIZE;


    // initialize queue 0.
    regs->queue_sel = 0;
    max = regs->queue_num_max;
    if (max == 0) {
        warnf("virtio_disk: no virtqueues available");
        goto err;
    }

    if (max < NUM) {
        warnf("virtio_disk: virtqueue size %d is too small", max);
        goto err;
    }

    kernel_assert((uint64)(disk_pages) % PGSIZE == 0,
                  "virtio_disk::open: disk_pages is not page aligned");

    regs->queue_num = NUM;
    memset(disk_pages, 0, 2*PGSIZE);
    regs->queue_pfn = ((uint64)disk_pages) >> PGSHIFT;

    if (regs->status & VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET) {
        warnf("virtio_disk: device failed");
        goto err;
    }

    // desc = pages -- num * virtq_desc
    // avail = pages + 0x40 -- 2 * uint16, then num * uint16
    // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem

    desc = (struct virtq_desc*)disk_pages;
    avail = (struct virtq_avail*)(disk_pages + NUM * sizeof(struct virtq_desc));
    used = (struct virtq_used*)(disk_pages + PGSIZE);

    // all NUM descriptors start out unused.
    for (int i = 0; i < NUM; i++)
        free[i] = 1;

    // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
    return 0;

err:
    warnf("virtio_disk: initialize failed");
    deregister_device();
    this->regs = nullptr;
    return -1;
}

void virtio_disk::close() {
    regs = nullptr;
    deregister_device();
}

task<int> virtio_disk::read(uint64 block_no, uint64 count, void *buf) {
    std::optional<int> result = co_await disk_command(VIRTIO_BLK_T_IN,block_no, count, buf);
    // we do not handle error here
    co_return result.value();
}
task<int> virtio_disk::write(uint64 block_no, uint64 count, const void *buf) {
    std::optional<int> result = co_await disk_command(VIRTIO_BLK_T_OUT, block_no, count, (void*)buf);
    // we do not handle error here
    co_return result.value();
}

task<int> virtio_disk::flush() {
    std::optional<int> result = co_await disk_command(VIRTIO_BLK_T_FLUSH, 0, 0, (void*)0);
    // we do not handle error here
    co_return result.value();
}

uint64 virtio_disk::capacity() const {
    return regs->config.capacity * 512 / block_device::BLOCK_SIZE;
}

// find a free descriptor, mark it non-free, return its index.
int virtio_disk::alloc_desc() {
    for (int i = 0; i < NUM; i++) {
        if (free[i]) {
            free[i] = 0;
            return i;
        }
    }
    return -1;
}

// mark a descriptor as free.
void virtio_disk::free_desc(int i) {
    if (i >= NUM)
        panic("free_desc 1");
    if (free[i])
        panic("free_desc 2");
    desc[i].addr = 0;
    desc[i].len = 0;
    desc[i].flags = 0;
    desc[i].next = 0;
    free[i] = 1;
    request_queue.wake_up_one();
}

// free a chain of descriptors.
void virtio_disk::free_chain(int i) {
    while (1) {
        int flag = desc[i].flags;
        int nxt = desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
int virtio_disk::alloc_descs(int* idx) {
    for (int i = 0; i < 3; i++) {
        idx[i] = alloc_desc();
        if (idx[i] < 0) {
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}



// this is not optimal for read operation only need 2 descriptors
task<int> virtio_disk::disk_command(uint64 command, uint64 block_no, uint64 count, void *buf) {
    #ifdef VIRTIO_DISK_DEBUG

    if (command == VIRTIO_BLK_T_OUT) {
        debug_core("disk_command: write [%l:+%l) from %p, buf[0]=%p", block_no, count, buf, (void*)*(uint64*)buf);
    }

    #endif

    kernel_assert( count == 1, "virtio_disk::disk_command: count must be 1");
    uint64 sector = block_no * (block_device::BLOCK_SIZE / 512);
    // uint64 nsector = count * (block_device::BLOCK_SIZE / 512);

    if (block_no + count > capacity()) {
        warnf("virtio_disk_rw: block_no %l + count %l > capacity %l", block_no, count, capacity());
        // panic("virtio_disk_rw");
        co_return task_fail;
    }

    lock.lock();

    // the spec's Section 5.2 says that legacy block operations use
    // three descriptors: one for type/reserved/sector, one for the
    // data, one for a 1-byte status result.
    // allocate the three descriptors.
    int idx[3];

    while (1) {
        if (alloc_descs(idx) == 0) {
            break;
        }
        // debug_core("virtio_disk_rw: no descriptors available");
        co_await request_queue.done(lock);
    }


    // format the three descriptors.
    // qemu's virtio-blk.c reads them.
    virtio_blk_req* buf0 = &ops[idx[0]];


    buf0->type = command;  // write the disk

    buf0->reserved = 0;
    buf0->sector = sector;

    desc[idx[0]].addr = (uint64)buf0;
    desc[idx[0]].len = sizeof(struct virtio_blk_req);
    desc[idx[0]].flags = VRING_DESC_F_NEXT;
    desc[idx[0]].next = idx[1];

    desc[idx[1]].addr = (uint64)buf;
    desc[idx[1]].len = count * block_device::BLOCK_SIZE;
    uint16 flags;
    if (command == VIRTIO_BLK_T_OUT || command == VIRTIO_BLK_T_FLUSH) {
        flags = 0;  // device reads b->data
    }
    else {
        flags = VRING_DESC_F_WRITE;  // device writes b->data
    }
    flags |= VRING_DESC_F_NEXT;
    desc[idx[1]].flags = flags;
    desc[idx[1]].next = idx[2];

    info[idx[0]].status = 0xfb;  // device writes 0 on success
    info[idx[0]].done = false;
    desc[idx[2]].addr = (uint64)&info[idx[0]].status;
    desc[idx[2]].len = 1;
    desc[idx[2]].flags = VRING_DESC_F_WRITE;  // device writes the status
    desc[idx[2]].next = 0;

    __sync_synchronize();

    // tell the device the first index in our chain of descriptors.
    avail->ring[avail->idx % NUM] = idx[0];

    // tell the device another avail ring entry is available.
    avail->idx = avail->idx + 1;  // not % NUM ...

    __sync_synchronize();

    while ( avail->idx - used->idx > NUM) {
        __sync_synchronize();
    }

    regs->queue_notify = 0;  // value is queue number


    // Wait for virtio_disk_intr() to say request has finished.
    while (!info[idx[0]].done) {
        // debug_core("virtio_disk_rw: waiting for request to finish");
        if (regs->status & VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET) {
            warnf("virtio_disk_rw: device needs reset");
            co_return -1;
        }
        co_await info[idx[0]].wait_queue.done(lock);

    }


    free_chain(idx[0]);

    lock.unlock();

    #ifdef VIRTIO_DISK_DEBUG
    if (command == VIRTIO_BLK_T_IN) {
        debug_core("disk_command: read [%l:+%l) to %p, buf[0]=%p", block_no, count, buf, (void*)*(uint64*)buf);
    }
    #endif
    
    co_return 0;
}

void virtio_disk::virtio_disk_intr() {
    //debug_core("virtio_disk_intr");


    lock.lock();

    // the device won't raise another interrupt until we tell it
    // we've seen this interrupt, which the following line does.
    // this may race with the device writing new entries to
    // the "used" ring, in which case we may process the new
    // completion entries in this interrupt, and have nothing to do
    // in the next interrupt, which is harmless.
    regs->interrupt_ack = regs->interrupt_status & 0x3;

    __sync_synchronize();

    // the device increments disk.used->idx when it
    // adds an entry to the used ring.

    while (used_idx != used->idx) {
        __sync_synchronize();
        int id = used->ring[used_idx % NUM].id;

        while (info[id].status != 0) ;

        used_idx += 1;

        info[id].done = true;
        info[id].wait_queue.wake_up();

        // debug_core("virtio_disk_intr: id %d, used_idx: %d", id, used_idx);
    }

    __sync_synchronize();

    lock.unlock();

    
}
