//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
// qemu presents a "legacy" virtio interface.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "virtio.h"
#include <arch/riscv.h>

#include <device/block/buf.h>
#include <device/block/bdev.h>

#include <mm/layout.h>
#include <proc/process.h>

#include <ccore/types.h>
#include <utils/log.h>


class virtio_disk : public block_device {
public:
    virtio_disk(uint8* disk_pages) : block_device("virtio_disk"), disk_pages(disk_pages) {}
    ~virtio_disk() = default;

    uint64 capacity() const override;
    int open(void* base_address) override;
    void close() override;
    task<int> read(uint64 block_no, uint64 count, void *buf) override;
    task<int> write(uint64 block_no, uint64 count, const void *buf) override;
    task<int> flush() override;

private:
    int alloc_desc();
    int alloc_descs(int *idx);
    void free_desc(int i);
    void free_chain(int i);

    // task<int> disk_rw(uint64 block_no, uint64 count, void *buf, bool write);
    task<int> disk_command(uint64 command, uint64 block_no, uint64 count, void *buf);
    task<void> disk_rw_done(int id);

    public:
    void virtio_disk_intr();



private:
    // the virtio driver and device mostly communicate through a set of
    // structures in RAM. pages[] allocates that memory. pages[] is a
    // global (instead of calls to kalloc()) because it must consist of
    // two contiguous pages of page-aligned physical memory.
    uint8* disk_pages;

    // pages[] is divided into three regions (descriptors, avail, and
    // used), as explained in Section 2.6 of the virtio specification
    // for the legacy interface.
    // https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf

    // the first region of pages[] is a set (not a ring) of DMA
    // descriptors, with which the driver tells the device where to read
    // and write individual disk operations. there are NUM descriptors.
    // most commands consist of a "chain" (a linked list) of a couple of
    // these descriptors.
    // points into pages[].
    struct virtq_desc *desc;

    // next is a ring in which the driver writes descriptor numbers
    // that the driver would like the device to process.  it only
    // includes the head descriptor of each chain. the ring has
    // NUM elements.
    // points into pages[].
    struct virtq_avail *avail;

    // finally a ring in which the device writes descriptor numbers that
    // the device has finished processing (just the head of each chain).
    // there are NUM used ring entries.
    // points into pages[].
    struct virtq_used *used;

private:
    volatile virtio_regs_t* regs = nullptr;
    spinlock lock {"virtio_disk.lock"};
    // disk command headers.
    // one-for-one with descriptors, for convenience.
    virtio_blk_req ops[NUM];

    char free[NUM];  // is a descriptor free?
    uint16 used_idx; // we've looked this far in used[2..NUM].
    // track info about in-flight operations,
    // for use when completion interrupt arrives.
    // indexed by first descriptor index of chain.
    struct {
        single_wait_queue wait_queue;
        bool done;
        char status;
    } info[NUM];

    wait_queue request_queue;
};




