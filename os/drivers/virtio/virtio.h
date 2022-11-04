#ifndef DRIVERS_VIRTIO_H
#define DRIVERS_VIRTIO_H

#include <ccore/types.h>
//
// virtio device definitions.
// for both the mmio interface, and virtio descriptors.
// only tested with qemu.
// this is the "legacy" virtio interface.
//
// the virtio spec:
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// virtio mmio control registers, mapped starting at 0x10001000.
// from qemu virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE		    0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION		        0x004 // version; 1 is legacy
#define VIRTIO_MMIO_DEVICE_ID		    0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID		    0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES	    0x010
#define VIRTIO_MMIO_DRIVER_FEATURES	    0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	    0x028 // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL		    0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX	    0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM		    0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN		    0x03c // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN		    0x040 // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY		    0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY	    0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK	    0x064 // write-only
#define VIRTIO_MMIO_STATUS		        0x070 // read/write

struct virtio_blk_config {
    uint64 capacity;
    uint32 size_max;
    uint32 seg_max;
    struct virtio_blk_geometry {
        uint16 cylinders;
        uint8 heads;
        uint8 sectors;
    } geometry;
    uint32 blk_size;
    struct virtio_blk_topology {
        // # of logical blocks per physical block (log2)
        uint8 physical_block_exp;
        // offset of first aligned logical block
        uint8 alignment_offset;
        // suggested minimum I/O size in blocks
        uint16 min_io_size;
        // optimal (suggested maximum) I/O size in blocks
        uint32 opt_io_size;
    } topology;
    uint8 writeback;
    uint8 __unused0[3];
    uint32 max_discard_sectors;
    uint32 max_discard_seg;
    uint32 discard_sector_alignment;
    uint32 max_write_zeroes_sectors;
    uint32 max_write_zeroes_seg;
    uint8 write_zeroes_may_unmap;
    uint8 __unused1[3];
};


struct virtio_regs_t {
	uint32 magic_value;             // 0x000: read-only, 0x74726976
	uint32 version;                 // 0x004: read-only, version; 1 is legacy
	uint32 device_id;               // 0x008: read-only, device type; 1 is net, 2 is disk
	uint32 vendor_id;               // 0x00c: read-only, 0x554d4551
	uint32 device_features;         // 0x010: read-only
	uint32 device_features_sel;     // 0x014: write-only
	uint32 __unused0[2];
	uint32 driver_features;         // 0x020: read/write
	uint32 driver_features_sel;     // 0x024: write-only
    uint32 guest_page_size;         // 0x028: write-only
    uint32 __unused1[1];
	uint32 queue_sel;               // 0x030: write-only    
	uint32 queue_num_max;           // 0x034: read-only
	uint32 queue_num;               // 0x038: write-only
    uint32 queue_align;             // 0x03c: write-only
	uint32 queue_pfn;               // 0x040: read/write, physical page number for queue
	uint32 __unused2[3];
	uint32 queue_notify;            // 0x050: write-only
	uint32 __unused3[3];
	uint32 interrupt_status;        // 0x060: read-only
	uint32 interrupt_ack;           // 0x064: write-only
	uint32 __unused4[2];
	uint32 status;                  // 0x070: read/write
    uint32 __unused5[35];
    virtio_blk_config config;       // 0x100: read-only
};

//#define offset_of(type, member) ((size_t) &((type *)0)->member)
//constexpr int offset1 = offset_of(virtio_regs_t, queue_num_max);
// constexpr int offset2 = offset_of(virtio_regs_t, status);

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8
#define VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET	64


// device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_FLUSH           9	/* Cache flush command support */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

// a single descriptor, from the spec.
struct virtq_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};
#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

// the (entire) avail ring, from the spec.
struct virtq_avail {
    uint16 flags; // always zero
    uint16 idx;   // driver will write ring[idx] next
    uint16 ring[NUM]; // descriptor numbers of chain heads
    uint16 unused;
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
struct virtq_used_elem {
    uint32 id;   // index of start of completed descriptor chain
    uint32 len;
};

struct virtq_used {
    uint16 flags; // always zero
    uint16 idx;   // device increments when it adds a ring[] entry
    struct virtq_used_elem ring[NUM];
};

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk
#define VIRTIO_BLK_T_FLUSH 4 // flush disk cache

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
    uint32 type; // VIRTIO_BLK_T_IN or ..._OUT
    uint32 reserved;
    uint64 sector;
};

#endif