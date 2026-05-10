#include "xhci.h"
#include "../memory/memutils.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "io/logging.h"
#include "keyboard.h"
#include "keys.h"
#include "pci.h"
#include "serial.h"
#include "types.h"
#include <stddef.h>

// ── TRB ──────────────────────────────────────────────────────────────────────

typedef struct {
  ulong param;
  uint status;
  uint control;
} __attribute__((packed, aligned(16))) trb_t;

// TRB control bits
#define TRB_C      (1u << 0)  // Cycle
#define TRB_TC     (1u << 1)  // Toggle Cycle (Link TRB)
#define TRB_IOC    (1u << 5)  // Interrupt On Completion
#define TRB_IDT    (1u << 6)  // Immediate Data (Setup TRB)
#define TRB_BSR    (1u << 9)  // Block Set-Address-Request
#define TRB_DIR_IN (1u << 16) // Data Stage direction = IN

#define TRB_TYPE(t) ((uint)(t) << 10)
#define TRB_SLOT(s) ((uint)(s) << 24)
#define TRB_EPID(e) ((uint)(e) << 16)

#define GET_TYPE(c) (((c) >> 10) & 0x3f)
#define GET_CC(s)   (((s) >> 24) & 0xff)
#define GET_SLOT(c) (((c) >> 24) & 0xff)
#define GET_EPID(c) (((c) >> 16) & 0x1f)
#define GET_XLEN(s) ((s) & 0xffffff)

// TRB type codes
#define TT_NORMAL      1
#define TT_SETUP       2
#define TT_DATA        3
#define TT_STATUS      4
#define TT_LINK        6
#define TT_ENABLE_SLOT 9
#define TT_ADDR_DEV    11
#define TT_CONFIG_EP   12
#define TT_NOOP_CMD    23
#define TT_XFER_EVT    32
#define TT_CMD_COMP    33
#define TT_PORT_SC     34

// Completion codes
#define CC_SUCCESS 1
#define CC_SHORT   13

// ── Capability register offsets ───────────────────────────────────────────────
#define CAP_CAPLENGTH  0x00
#define CAP_HCSPARAMS1 0x04
#define CAP_HCSPARAMS2 0x08
#define CAP_HCCPARAMS1 0x10
#define CAP_DBOFF      0x14
#define CAP_RTSOFF     0x18

// ── Operational register offsets ──────────────────────────────────────────────
#define OP_USBCMD 0x00
#define OP_USBSTS 0x04
#define OP_CRCR   0x18
#define OP_DCBAAP 0x30
#define OP_CONFIG 0x38

#define CMD_RS    (1u << 0)
#define CMD_HCRST (1u << 1)
#define STS_HCH   (1u << 0)
#define STS_CNR   (1u << 11)

// ── PORTSC bits ───────────────────────────────────────────────────────────────
#define PS_CCS (1u << 0)
#define PS_PED (1u << 1)
#define PS_PR  (1u << 4)
#define PS_PP  (1u << 9)
#define PS_PRC (1u << 21)
// RW1C bits — must be written 0 to preserve (writing 1 clears them)
#define PS_RW1C     ((1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) | (1u << 21) | (1u << 22) | (1u << 23))
#define PS_SPEED(v) (((v) >> 10) & 0xf)

// ── Runtime interrupter register offsets (byte offset from g_rt) ──────────────
#define RT_IMAN(n)   (0x20u + (uint)(n) * 0x20u + 0x00u)
#define RT_ERSTSZ(n) (0x20u + (uint)(n) * 0x20u + 0x08u)
#define RT_ERSTBA(n) (0x20u + (uint)(n) * 0x20u + 0x10u)
#define RT_ERDP(n)   (0x20u + (uint)(n) * 0x20u + 0x18u)

// ── Ring sizes ────────────────────────────────────────────────────────────────
#define CMD_RING_SZ  64
#define EVT_RING_SZ  256
#define XFER_RING_SZ 32

// ── Max supported slots ───────────────────────────────────────────────────────
#define MAX_SLOTS 8

// ── ERST entry ────────────────────────────────────────────────────────────────
typedef struct {
  ulong seg_base;
  uint seg_size;
  uint _res;
} __attribute__((packed)) erst_t;

// ── MMIO access ───────────────────────────────────────────────────────────────
static volatile ubyte *g_cap;
static volatile ubyte *g_op;
static volatile ubyte *g_rt;
static volatile uint *g_db; // doorbell array (uint32 per slot)
static uint g_ctx_sz;       // 32 or 64 bytes per context entry

#define CAP8(off)      (*(volatile ubyte *)(g_cap + (off)))
#define CAP32(off)     (*(volatile uint *)(g_cap + (off)))
#define OP32(off)      (*(volatile uint *)(g_op + (off)))
#define OP64(off)      (*(volatile ulong *)(g_op + (off)))
#define RT32(off)      (*(volatile uint *)(g_rt + (off)))
#define RT64(off)      (*(volatile ulong *)(g_rt + (off)))
#define PORT32(p, off) (*(volatile uint *)(g_op + 0x400u + (uint)(p) * 0x10u + (off)))

static void mb(void) { __asm__ volatile("mfence" ::: "memory"); }

// ── DMA globals ───────────────────────────────────────────────────────────────
static ulong *g_dcbaa;
static ulong g_dcbaa_phys;

static volatile trb_t *g_cmd_ring;
static ulong g_cmd_ring_phys;
static uint g_cmd_enq;
static uint g_cmd_pcs;

static volatile trb_t *g_evt_ring;
static ulong g_evt_ring_phys;
static uint g_evt_deq;
static uint g_evt_ccs;

static erst_t *g_erst;
static ulong g_erst_phys;

// Shared input context (for Address Device / Configure Endpoint commands)
static volatile ubyte *g_ictx;
static ulong g_ictx_phys;

// ── Input context helpers ─────────────────────────────────────────────────────
// Layout: [ICC][Slot ctx][EP1 ctx]...[EP31 ctx]
static volatile uint *ictx_icc(void) { return (volatile uint *)g_ictx; }
static volatile uint *ictx_slot(void) { return (volatile uint *)(g_ictx + g_ctx_sz); }
static volatile uint *ictx_ep(uint ep_id) { return (volatile uint *)(g_ictx + g_ctx_sz * (ep_id + 1)); }

static void zero_ictx(void) {
  memset8((ubyte *)g_ictx, 0, 33u * g_ctx_sz);
}

// ── Per-slot state ────────────────────────────────────────────────────────────
typedef struct {
  int active;
  int port;
  int speed;

  ulong dev_ctx_phys;

  volatile trb_t *ep0_ring;
  ulong ep0_ring_phys;
  uint ep0_enq;
  uint ep0_pcs;

  // HID keyboard
  volatile trb_t *kbd_ring;
  ulong kbd_ring_phys;
  uint kbd_enq;
  uint kbd_pcs;
  int kbd_dci;
  int kbd_max_pkt;
  ubyte *kbd_buf;
  ulong kbd_buf_phys;
  ubyte prev_report[8];
  bool kbd_ready;

  // USB Mass Storage (Bulk-Only Transport)
  volatile trb_t *bulk_out_ring;
  ulong bulk_out_ring_phys;
  uint bulk_out_enq;
  uint bulk_out_pcs;
  int bulk_out_dci;
  int bulk_out_max_pkt;

  volatile trb_t *bulk_in_ring;
  ulong bulk_in_ring_phys;
  uint bulk_in_enq;
  uint bulk_in_pcs;
  int bulk_in_dci;
  int bulk_in_max_pkt;

  bool msc_ready;
} slot_t;

static slot_t g_slots[MAX_SLOTS + 1]; // 1-indexed
static bool g_xhci_ok;
static int g_msc_slot; // slot ID of the first MSC device found (0 = none)

// ── Ring helpers ──────────────────────────────────────────────────────────────

static void cmd_push(ulong param, uint status, uint control) {
  g_cmd_ring[g_cmd_enq].param = param;
  g_cmd_ring[g_cmd_enq].status = status;
  g_cmd_ring[g_cmd_enq].control = control | (g_cmd_pcs ? TRB_C : 0);
  mb();

  g_cmd_enq++;
  if (g_cmd_enq >= CMD_RING_SZ - 1) {
    g_cmd_ring[CMD_RING_SZ - 1].param = g_cmd_ring_phys;
    g_cmd_ring[CMD_RING_SZ - 1].status = 0;
    g_cmd_ring[CMD_RING_SZ - 1].control = TRB_TYPE(TT_LINK) | TRB_TC | (g_cmd_pcs ? TRB_C : 0);
    mb();
    g_cmd_enq = 0;
    g_cmd_pcs ^= 1;
  }
  g_db[0] = 0; // ring HC doorbell
}

static void ep0_push(int sid, ulong param, uint status, uint control) {
  slot_t *s = &g_slots[sid];
  s->ep0_ring[s->ep0_enq].param = param;
  s->ep0_ring[s->ep0_enq].status = status;
  s->ep0_ring[s->ep0_enq].control = control | (s->ep0_pcs ? TRB_C : 0);
  mb();

  s->ep0_enq++;
  if (s->ep0_enq >= XFER_RING_SZ - 1) {
    s->ep0_ring[XFER_RING_SZ - 1].param = s->ep0_ring_phys;
    s->ep0_ring[XFER_RING_SZ - 1].status = 0;
    s->ep0_ring[XFER_RING_SZ - 1].control = TRB_TYPE(TT_LINK) | TRB_TC | (s->ep0_pcs ? TRB_C : 0);
    mb();
    s->ep0_enq = 0;
    s->ep0_pcs ^= 1;
  }
}

static void kbd_push_one(int sid) {
  slot_t *s = &g_slots[sid];
  s->kbd_ring[s->kbd_enq].param = s->kbd_buf_phys;
  s->kbd_ring[s->kbd_enq].status = (uint)s->kbd_max_pkt;
  s->kbd_ring[s->kbd_enq].control = TRB_TYPE(TT_NORMAL) | TRB_IOC | (s->kbd_pcs ? TRB_C : 0);
  mb();

  s->kbd_enq++;
  if (s->kbd_enq >= XFER_RING_SZ - 1) {
    s->kbd_ring[XFER_RING_SZ - 1].param = s->kbd_ring_phys;
    s->kbd_ring[XFER_RING_SZ - 1].status = 0;
    s->kbd_ring[XFER_RING_SZ - 1].control = TRB_TYPE(TT_LINK) | TRB_TC | (s->kbd_pcs ? TRB_C : 0);
    mb();
    s->kbd_enq = 0;
    s->kbd_pcs ^= 1;
  }

  // Ring doorbell: slot, target = kbd_dci
  g_db[(uint)sid] = (uint)s->kbd_dci;
}

static void bulk_out_push(int sid, ulong phys, uint len) {
  slot_t *s = &g_slots[sid];
  s->bulk_out_ring[s->bulk_out_enq].param = phys;
  s->bulk_out_ring[s->bulk_out_enq].status = len;
  s->bulk_out_ring[s->bulk_out_enq].control =
      TRB_TYPE(TT_NORMAL) | TRB_IOC | (s->bulk_out_pcs ? TRB_C : 0);
  mb();

  s->bulk_out_enq++;
  if (s->bulk_out_enq >= XFER_RING_SZ - 1) {
    s->bulk_out_ring[XFER_RING_SZ - 1].param = s->bulk_out_ring_phys;
    s->bulk_out_ring[XFER_RING_SZ - 1].status = 0;
    s->bulk_out_ring[XFER_RING_SZ - 1].control =
        TRB_TYPE(TT_LINK) | TRB_TC | (s->bulk_out_pcs ? TRB_C : 0);
    mb();
    s->bulk_out_enq = 0;
    s->bulk_out_pcs ^= 1;
  }
  g_db[(uint)sid] = (uint)s->bulk_out_dci;
}

static void bulk_in_push(int sid, ulong phys, uint len) {
  slot_t *s = &g_slots[sid];
  s->bulk_in_ring[s->bulk_in_enq].param = phys;
  s->bulk_in_ring[s->bulk_in_enq].status = len;
  s->bulk_in_ring[s->bulk_in_enq].control =
      TRB_TYPE(TT_NORMAL) | TRB_IOC | (s->bulk_in_pcs ? TRB_C : 0);
  mb();

  s->bulk_in_enq++;
  if (s->bulk_in_enq >= XFER_RING_SZ - 1) {
    s->bulk_in_ring[XFER_RING_SZ - 1].param = s->bulk_in_ring_phys;
    s->bulk_in_ring[XFER_RING_SZ - 1].status = 0;
    s->bulk_in_ring[XFER_RING_SZ - 1].control =
        TRB_TYPE(TT_LINK) | TRB_TC | (s->bulk_in_pcs ? TRB_C : 0);
    mb();
    s->bulk_in_enq = 0;
    s->bulk_in_pcs ^= 1;
  }
  g_db[(uint)sid] = (uint)s->bulk_in_dci;
}

// ── Event ring consumer ───────────────────────────────────────────────────────

typedef struct {
  uint type;
  ulong param;
  uint status;
  uint ctrl;
} evt_t;

static bool evt_dequeue(evt_t *e) {
  volatile trb_t *t = &g_evt_ring[g_evt_deq];
  if ((t->control & 1) != (uint)g_evt_ccs)
    return false;

  e->type = GET_TYPE(t->control);
  e->param = t->param;
  e->status = t->status;
  e->ctrl = t->control;

  g_evt_deq++;
  if (g_evt_deq >= EVT_RING_SZ) {
    g_evt_deq = 0;
    g_evt_ccs ^= 1;
  }

  ulong erdp = g_evt_ring_phys + (ulong)g_evt_deq * sizeof(trb_t);
  erdp |= (1u << 3); // EHB
  RT64(RT_ERDP(0)) = erdp;
  mb();
  return true;
}

// Spin until a CMD_COMP event, return slot ID via out_slot
static int cmd_wait(int *out_slot) {
  evt_t e;
  for (int i = 0; i < 5000000; i++) {
    if (!evt_dequeue(&e))
      continue;
    if (e.type == TT_CMD_COMP) {
      if (out_slot)
        *out_slot = (int)GET_SLOT(e.ctrl);
      return (int)GET_CC(e.status);
    }
  }
  serial_write_line("xHCI: cmd_wait timeout");
  if (out_slot)
    *out_slot = 0;
  return -1;
}

// Spin until a XFER_EVT for the given slot/ep
static int xfer_wait(int sid, int ep_dci) {
  evt_t e;
  for (int i = 0; i < 5000000; i++) {
    if (!evt_dequeue(&e))
      continue;
    if (e.type == TT_XFER_EVT && (int)GET_SLOT(e.ctrl) == sid && (int)GET_EPID(e.ctrl) == ep_dci) {
      int cc = GET_CC(e.status);
      return (cc == CC_SUCCESS || cc == CC_SHORT) ? 0 : -1;
    }
  }
  serial_write_line("xHCI: xfer_wait timeout");
  return -1;
}

// ── BIOS handoff ──────────────────────────────────────────────────────────────

static void bios_handoff(uint xecp_off) {
  volatile uint *p = (volatile uint *)(g_cap + xecp_off);
  klogf(LOG_TRACE, "xHCI: bios_handoff xecp_off=%x first_cap_hdr=%x", xecp_off, *p);

  while (1) {
    uint hdr = *p;
    ubyte id = (ubyte)(hdr & 0xff);
    ubyte nxt = (ubyte)((hdr >> 8) & 0xff);

    if (id == 1) { // USB Legacy Support (USBLEGSUP)
      volatile uint *smi_ctrl = p + 1; // USBLEGCTLSTS
      klogf(LOG_TRACE, "xHCI: USBLEGSUP=%x USBLEGCTLSTS_before=%x", hdr, *smi_ctrl);

      *smi_ctrl = 0; // clear all USB SMI enable bits
      mb();
      klogf(LOG_TRACE, "xHCI: USBLEGCTLSTS_after=%x", *smi_ctrl);

      *p = hdr | (1u << 24); // set OS Owned Semaphore
      mb();
      for (int i = 0; i < 100000000; i++) {
        if (!(*p & (1u << 16)))
          break;
      }
      if (*p & (1u << 16))
        klogf(LOG_TRACE, "xHCI: BIOS handoff timeout USBLEGSUP=%x USBLEGCTLSTS=%x",
              *p, *smi_ctrl);
      else
        klogf(LOG_TRACE, "xHCI: BIOS handoff OK USBLEGSUP=%x USBLEGCTLSTS=%x",
              *p, *smi_ctrl);
      return;
    }
    if (nxt == 0) {
      klogf(LOG_TRACE, "xHCI: no USBLEGSUP in ext-cap chain (last id=%u) - SMI not disabled!", id);
      return;
    }
    p = (volatile uint *)((ulong)p + (ulong)nxt * 4);
  }
}

// ── Controller reset ──────────────────────────────────────────────────────────

static bool ctrl_reset(void) {
  OP32(OP_USBCMD) &= ~CMD_RS;
  for (int i = 0; i < 2000000; i++)
    if (OP32(OP_USBSTS) & STS_HCH)
      break;
  if (!(OP32(OP_USBSTS) & STS_HCH)) {
    serial_write_line("xHCI: HC failed to halt");
    return false;
  }

  OP32(OP_USBCMD) |= CMD_HCRST;
  for (int i = 0; i < 2000000; i++)
    if (!(OP32(OP_USBCMD) & CMD_HCRST))
      break;
  for (int i = 0; i < 2000000; i++)
    if (!(OP32(OP_USBSTS) & STS_CNR))
      break;

  if (OP32(OP_USBCMD) & CMD_HCRST) {
    serial_write_line("xHCI: reset timeout");
    return false;
  }
  return true;
}

// ── Controller init ───────────────────────────────────────────────────────────

static bool ctrl_init(void) {
  uint max_slots = CAP32(CAP_HCSPARAMS1) & 0xff;
  if (max_slots > MAX_SLOTS)
    max_slots = MAX_SLOTS;

  // Allocate scratchpads if needed
  uint sp_lo = (CAP32(CAP_HCSPARAMS2) >> 27) & 0x1f;
  uint sp_hi = (CAP32(CAP_HCSPARAMS2) >> 21) & 0x1f;
  uint sp_count = (sp_hi << 5) | sp_lo;

  // DCBAA
  g_dcbaa_phys = pmm_alloc_pages(1);
  g_dcbaa = (ulong *)PHYS_TO_HHDM(g_dcbaa_phys);
  memset8((ubyte *)g_dcbaa, 0, 4096);

  if (sp_count > 0) {
    ulong sp_array_phys = pmm_alloc_pages(1);
    ulong *sp_array = (ulong *)PHYS_TO_HHDM(sp_array_phys);
    memset8((ubyte *)sp_array, 0, 4096);
    for (uint i = 0; i < sp_count && i < 512; i++)
      sp_array[i] = pmm_alloc_pages(1);
    g_dcbaa[0] = sp_array_phys;
  }

  // Command ring
  g_cmd_ring_phys = pmm_alloc_pages(1);
  g_cmd_ring = (volatile trb_t *)PHYS_TO_HHDM(g_cmd_ring_phys);
  memset8((ubyte *)g_cmd_ring, 0, 4096);
  g_cmd_enq = 0;
  g_cmd_pcs = 1;

  // Event ring segment
  g_evt_ring_phys = pmm_alloc_pages(1);
  g_evt_ring = (volatile trb_t *)PHYS_TO_HHDM(g_evt_ring_phys);
  memset8((ubyte *)g_evt_ring, 0, 4096);
  g_evt_deq = 0;
  g_evt_ccs = 1;

  // ERST (1 entry)
  g_erst_phys = pmm_alloc_pages(1);
  g_erst = (erst_t *)PHYS_TO_HHDM(g_erst_phys);
  memset8((ubyte *)g_erst, 0, 4096);
  g_erst[0].seg_base = g_evt_ring_phys;
  g_erst[0].seg_size = EVT_RING_SZ;

  // Shared input context
  g_ictx_phys = pmm_alloc_pages(1);
  g_ictx = (volatile ubyte *)PHYS_TO_HHDM(g_ictx_phys);
  memset8((ubyte *)g_ictx, 0, 4096);

  mb();

  // Program registers
  OP32(OP_CONFIG) = max_slots;
  OP64(OP_DCBAAP) = g_dcbaa_phys;
  // CRCR: ring base | RCS=1
  OP64(OP_CRCR) = g_cmd_ring_phys | 1;

  // Interrupter 0
  RT32(RT_ERSTSZ(0)) = 1;
  RT64(RT_ERSTBA(0)) = g_erst_phys;
  RT64(RT_ERDP(0)) = g_evt_ring_phys;
  RT32(RT_IMAN(0)) = RT32(RT_IMAN(0)) | 2; // enable interrupter

  mb();

  // Start HC
  OP32(OP_USBCMD) |= CMD_RS;
  for (int i = 0; i < 1000000; i++)
    if (!(OP32(OP_USBSTS) & STS_HCH))
      break;

  if (OP32(OP_USBSTS) & STS_HCH) {
    serial_write_line("xHCI: HC failed to start");
    return false;
  }

  serial_write_line("xHCI: controller running");
  return true;
}

// ── Port reset ────────────────────────────────────────────────────────────────

static bool port_reset(int port) {
  uint ps = PORT32(port, 0);
  uint pls = (ps >> 5) & 0xf;

  klogf(LOG_TRACE, "xHCI: port_reset %u PORTSC=%x PLS=%u PP=%u CCS=%u PED=%u CSC=%u",
        (uint)port, ps, pls,
        (ps >> 9) & 1u, ps & 1u, (ps >> 1) & 1u, (ps >> 17) & 1u);

  // Write test: momentarily clear PP and read back to verify writes reach hardware.
  // If readback still shows PP=1, writes are being discarded (SMM still active).
  {
    uint wt = (ps & ~PS_RW1C) & ~PS_PP;
    PORT32(port, 0) = wt;
    mb();
    uint rb = PORT32(port, 0);
    bool writes_ok = (rb & PS_PP) == 0;
    klogf(LOG_TRACE, "xHCI: port %u PP=0 write test: readback=%x writes_ok=%d",
          (uint)port, rb, (int)writes_ok);
    // Restore.
    PORT32(port, 0) = ps & ~PS_RW1C;
    mb();
    if (!writes_ok) {
      klogf(LOG_TRACE, "xHCI: port %u PORTSC writes NOT reaching hardware - check BIOS handoff",
            (uint)port);
      // Still try PR=1 below — might work even if PP is hardwired.
    }
  }

  if (pls == 7) {
    klogf(LOG_TRACE, "xHCI: port %u Polling at entry, waiting for hw reset to finish...",
          (uint)port);
    // Use spin-delay between checks so timing isn't tied to MMIO read speed.
    for (int i = 0; i < 50; i++) {
      for (volatile long j = 0; j < 5000000LL; j++) ;  // ~5-10ms
      ps = PORT32(port, 0);
      pls = (ps >> 5) & 0xf;
      if (ps & PS_PED) {
        klogf(LOG_TRACE, "xHCI: port %u PED=1 naturally after ~%dms", (uint)port, (i+1)*8);
        return true;
      }
      if (pls != 7) {
        klogf(LOG_TRACE, "xHCI: port %u PLS changed %u->%u after ~%dms",
              (uint)port, 7u, pls, (i+1)*8);
        break;
      }
    }
    ps = PORT32(port, 0);
    pls = (ps >> 5) & 0xf;
    if (ps & PS_PED) return true;
    klogf(LOG_TRACE, "xHCI: port %u Polling wait ended PLS=%u PORTSC=%x", (uint)port, pls, ps);

    if (pls == 7) {
      // Still stuck. Try power-cycling.
      uint wval = (ps & ~PS_RW1C) & ~PS_PP;
      PORT32(port, 0) = wval;
      mb();
      bool pp_worked = false;
      for (volatile long j = 0; j < 5000000LL; j++) ;  // ~5ms for PP to take effect
      uint rb = PORT32(port, 0);
      klogf(LOG_TRACE, "xHCI: port %u PP=0 cycle: readback=%x CCS=%u",
            (uint)port, rb, rb & 1u);
      if (!(rb & PS_CCS)) pp_worked = true;

      PORT32(port, 0) = wval | PS_PP;
      mb();
      if (pp_worked) {
        for (int i = 0; i < 50; i++) {
          for (volatile long j = 0; j < 5000000LL; j++) ;
          ps = PORT32(port, 0);
          if (ps & PS_CCS) break;
        }
        for (volatile long j = 0; j < 5000000LL; j++) ;  // settle
        ps = PORT32(port, 0);
        pls = (ps >> 5) & 0xf;
        klogf(LOG_TRACE, "xHCI: port %u after power cycle: PORTSC=%x PLS=%u", (uint)port, ps, pls);
        if (ps & PS_PED) return true;
        if (pls == 7) {
          for (int i = 0; i < 50; i++) {
            for (volatile long j = 0; j < 5000000LL; j++) ;
            ps = PORT32(port, 0);
            pls = (ps >> 5) & 0xf;
            if (ps & PS_PED) return true;
            if (pls != 7) break;
          }
          ps = PORT32(port, 0);
          pls = (ps >> 5) & 0xf;
          if (ps & PS_PED) return true;
        }
      } else {
        ps = PORT32(port, 0);
        pls = (ps >> 5) & 0xf;
        klogf(LOG_TRACE, "xHCI: port %u PP hardwired, skip cycle PORTSC=%x", (uint)port, ps);
      }

      if (pls == 7) {
        klogf(LOG_TRACE, "xHCI: port %u cannot escape Polling (pp_worked=%d), trying PR=1 anyway",
              (uint)port, (int)pp_worked);
      }
    }
  }

  // Issue PR=1.
  ps = PORT32(port, 0);
  ps &= ~PS_RW1C;
  ps |= PS_PR;
  PORT32(port, 0) = ps;
  mb();
  {
    uint rb = PORT32(port, 0);
    klogf(LOG_TRACE, "xHCI: port %u PR=1 written, readback=%x PR_bit=%u",
          (uint)port, rb, (rb >> 4) & 1u);
  }

  for (int i = 0; i < 5000000; i++)
    if (!(PORT32(port, 0) & PS_PR))
      break;

  uint ps2 = PORT32(port, 0);
  ps2 &= ~PS_RW1C;
  ps2 |= PS_PRC;
  PORT32(port, 0) = ps2;
  mb();

  for (int i = 0; i < 5000000; i++)
    if (PORT32(port, 0) & PS_PED)
      return true;

  klogf(LOG_TRACE, "xHCI: port %u no PED PORTSC=%x", (uint)port, PORT32(port, 0));
  return false;
}

// ── Allocate slot resources ───────────────────────────────────────────────────

static bool alloc_slot(int sid) {
  slot_t *s = &g_slots[sid];

  s->dev_ctx_phys = pmm_alloc_pages(1);
  memset8((ubyte *)PHYS_TO_HHDM(s->dev_ctx_phys), 0, 4096);
  g_dcbaa[sid] = s->dev_ctx_phys;

  s->ep0_ring_phys = pmm_alloc_pages(1);
  s->ep0_ring = (volatile trb_t *)PHYS_TO_HHDM(s->ep0_ring_phys);
  memset8((ubyte *)s->ep0_ring, 0, 4096);
  s->ep0_enq = 0;
  s->ep0_pcs = 1;

  s->kbd_ring_phys = pmm_alloc_pages(1);
  s->kbd_ring = (volatile trb_t *)PHYS_TO_HHDM(s->kbd_ring_phys);
  memset8((ubyte *)s->kbd_ring, 0, 4096);
  s->kbd_enq = 0;
  s->kbd_pcs = 1;

  ulong kbd_buf_phys = pmm_alloc_pages(1);
  s->kbd_buf_phys = kbd_buf_phys;
  s->kbd_buf = (ubyte *)PHYS_TO_HHDM(kbd_buf_phys);
  memset8(s->kbd_buf, 0, 4096);

  mb();
  return true;
}

// ── Address Device ────────────────────────────────────────────────────────────

static int address_device(int sid, int port, int speed, int max_pkt0, bool bsr) {
  zero_ictx();

  // ICC: Add[A0]=slot, Add[A1]=EP0
  ictx_icc()[1] = (1u << 0) | (1u << 1); // A0, A1

  // Slot context DW0: speed, num_ctx_entries=1, route_string=0
  ictx_slot()[0] = ((uint)speed << 20) | (1u << 27);
  // Slot context DW1: root hub port number (1-based)
  ictx_slot()[1] = (uint)(port + 1) << 16;

  // EP0 context: type=Control(4), cerr=3, max_packet_size
  slot_t *s = &g_slots[sid];
  ictx_ep(1)[1] = (3u << 1) | (4u << 3) | ((uint)max_pkt0 << 16);
  ictx_ep(1)[2] = (uint)(s->ep0_ring_phys & 0xFFFFFFFF) | 1; // deq low + DCS
  ictx_ep(1)[3] = (uint)(s->ep0_ring_phys >> 32);            // deq high
  ictx_ep(1)[4] = 8;                                         // avg TRB length

  mb();

  cmd_push(g_ictx_phys, 0,
           TRB_TYPE(TT_ADDR_DEV) | TRB_SLOT(sid) | (bsr ? TRB_BSR : 0));
  return cmd_wait(NULL);
}

// ── Control transfer ──────────────────────────────────────────────────────────

static int ctrl_xfer(int sid, ubyte bmRT, ubyte bReq,
                     ushort wVal, ushort wIdx, ushort wLen,
                     ulong data_phys) {
  // Pack setup packet into 64-bit immediate
  ulong setup = (ulong)bmRT | ((ulong)bReq << 8) | ((ulong)wVal << 16) | ((ulong)wIdx << 32) | ((ulong)wLen << 48);

  bool dir_in = (bmRT & 0x80) != 0;
  uint trt = (wLen == 0) ? 0u : (dir_in ? 3u : 2u);

  // Setup Stage TRB
  ep0_push(sid, setup, 8, TRB_TYPE(TT_SETUP) | TRB_IDT | (trt << 16));

  // Data Stage TRB
  if (wLen > 0)
    ep0_push(sid, data_phys, wLen, TRB_TYPE(TT_DATA) | (dir_in ? TRB_DIR_IN : 0));

  // Status Stage TRB (direction opposite to data, IN if no data)
  uint sdir = (!dir_in || wLen == 0) ? TRB_DIR_IN : 0;
  ep0_push(sid, 0, 0, TRB_TYPE(TT_STATUS) | TRB_IOC | sdir);

  g_db[(uint)sid] = 1; // ring EP0 doorbell
  return xfer_wait(sid, 1);
}

// ── USB descriptor buffer ─────────────────────────────────────────────────────
// One shared page for control transfer data (used sequentially, not concurrently)
static ubyte *g_dbuf;
static ulong g_dbuf_phys;

// ── Configure interrupt IN endpoint ──────────────────────────────────────────

static int configure_endpoint(int sid, ubyte ep_addr, int interval, int max_pkt) {
  int ep_num = ep_addr & 0x7f;
  int dir_in = (ep_addr & 0x80) != 0;
  // DCI = ep_num * 2 + (dir_in ? 1 : 0), but for interrupt in: DCI = ep_num*2+1
  int dci = ep_num * 2 + (dir_in ? 1 : 0);

  slot_t *s = &g_slots[sid];
  s->kbd_dci = dci;
  s->kbd_max_pkt = max_pkt;

  zero_ictx();

  // ICC: Add slot ctx + this endpoint
  ictx_icc()[1] = 1u | (1u << (uint)dci);

  // Copy current slot context DW0..DW3 from device context output
  volatile uint *dslot = (volatile uint *)PHYS_TO_HHDM(s->dev_ctx_phys);
  ictx_slot()[0] = dslot[0];
  ictx_slot()[1] = dslot[1];
  ictx_slot()[2] = dslot[2];
  ictx_slot()[3] = dslot[3];
  // Update num_ctx_entries field in DW0 (bits[31:27])
  uint num_ctx = (uint)dci + 1;
  ictx_slot()[0] = (ictx_slot()[0] & ~(0x1fu << 27)) | (num_ctx << 27);

  // Endpoint context: type=7 (Interrupt IN), cerr=3, interval, max_pkt
  volatile uint *ep = ictx_ep((uint)dci);
  ep[0] = (uint)interval << 16;
  ep[1] = (3u << 1) | (7u << 3) | ((uint)max_pkt << 16); // cerr=3, type=INT_IN
  ep[2] = (uint)(s->kbd_ring_phys & 0xFFFFFFFF) | 1;     // deq low + DCS
  ep[3] = (uint)(s->kbd_ring_phys >> 32);
  ep[4] = (uint)max_pkt; // avg TRB length

  mb();

  cmd_push(g_ictx_phys, 0, TRB_TYPE(TT_CONFIG_EP) | TRB_SLOT(sid));
  return cmd_wait(NULL);
}

// ── HID boot protocol keycode → ASCII ────────────────────────────────────────

static const ubyte hid_unshifted[256] = {
    [0x04] = 'a',
    [0x05] = 'b',
    [0x06] = 'c',
    [0x07] = 'd',
    [0x08] = 'e',
    [0x09] = 'f',
    [0x0A] = 'g',
    [0x0B] = 'h',
    [0x0C] = 'i',
    [0x0D] = 'j',
    [0x0E] = 'k',
    [0x0F] = 'l',
    [0x10] = 'm',
    [0x11] = 'n',
    [0x12] = 'o',
    [0x13] = 'p',
    [0x14] = 'q',
    [0x15] = 'r',
    [0x16] = 's',
    [0x17] = 't',
    [0x18] = 'u',
    [0x19] = 'v',
    [0x1A] = 'w',
    [0x1B] = 'x',
    [0x1C] = 'y',
    [0x1D] = 'z',
    [0x1E] = '1',
    [0x1F] = '2',
    [0x20] = '3',
    [0x21] = '4',
    [0x22] = '5',
    [0x23] = '6',
    [0x24] = '7',
    [0x25] = '8',
    [0x26] = '9',
    [0x27] = '0',
    [0x28] = '\n',
    [0x29] = 27,
    [0x2A] = '\b',
    [0x2B] = '\t',
    [0x2C] = ' ',
    [0x2D] = '-',
    [0x2E] = '=',
    [0x2F] = '[',
    [0x30] = ']',
    [0x31] = '\\',
    [0x33] = ';',
    [0x34] = '\'',
    [0x35] = '`',
    [0x36] = ',',
    [0x37] = '.',
    [0x38] = '/',
};

static const ubyte hid_shifted[256] = {
    [0x04] = 'A',
    [0x05] = 'B',
    [0x06] = 'C',
    [0x07] = 'D',
    [0x08] = 'E',
    [0x09] = 'F',
    [0x0A] = 'G',
    [0x0B] = 'H',
    [0x0C] = 'I',
    [0x0D] = 'J',
    [0x0E] = 'K',
    [0x0F] = 'L',
    [0x10] = 'M',
    [0x11] = 'N',
    [0x12] = 'O',
    [0x13] = 'P',
    [0x14] = 'Q',
    [0x15] = 'R',
    [0x16] = 'S',
    [0x17] = 'T',
    [0x18] = 'U',
    [0x19] = 'V',
    [0x1A] = 'W',
    [0x1B] = 'X',
    [0x1C] = 'Y',
    [0x1D] = 'Z',
    [0x1E] = '!',
    [0x1F] = '@',
    [0x20] = '#',
    [0x21] = '$',
    [0x22] = '%',
    [0x23] = '^',
    [0x24] = '&',
    [0x25] = '*',
    [0x26] = '(',
    [0x27] = ')',
    [0x28] = '\n',
    [0x29] = 27,
    [0x2A] = '\b',
    [0x2B] = '\t',
    [0x2C] = ' ',
    [0x2D] = '_',
    [0x2E] = '+',
    [0x2F] = '{',
    [0x30] = '}',
    [0x31] = '|',
    [0x33] = ':',
    [0x34] = '"',
    [0x35] = '~',
    [0x36] = '<',
    [0x37] = '>',
    [0x38] = '?',
};

static void process_report(int sid) {
  slot_t *s = &g_slots[sid];
  ubyte *r = s->kbd_buf;
  ubyte mods = r[0];
  bool shift = (mods & 0x22) != 0; // Left Shift | Right Shift

  for (int ki = 2; ki < 8; ki++) {
    ubyte kc = r[ki];
    if (kc < 4 || kc == 0x01)
      continue;

    // Check if this key was already pressed
    bool was_pressed = false;
    for (int pi = 2; pi < 8; pi++) {
      if (s->prev_report[pi] == kc) {
        was_pressed = true;
        break;
      }
    }
    if (was_pressed)
      continue;

    KeyEvent ev = {KEY_NONE, 0};

    switch (kc) {
    case 0x28:
      ev.code = KEY_RETURN;
      ev.character = '\n';
      break;
    case 0x4F:
      ev.code = KEY_RIGHT;
      break;
    case 0x50:
      ev.code = KEY_LEFT;
      break;
    case 0x51:
      ev.code = KEY_DOWN;
      break;
    case 0x52:
      ev.code = KEY_UP;
      break;
    case 0x2A:
      ev.code = KEY_BACKSPACE;
      break;
    case 0x2B:
      ev.code = KEY_TAB;
      ev.character = '\t';
      break;
    default: {
      ubyte ch = shift ? hid_shifted[kc] : hid_unshifted[kc];
      if (ch) {
        ev.code = 1;
        ev.character = ch;
      }
      break;
    }
    }

    if (ev.code != KEY_NONE)
      keyboard_inject(ev);
  }

  memcpy8(s->prev_report, r, 8);
}

// ── Configure bulk endpoint ───────────────────────────────────────────────────

static int configure_bulk_ep(int sid, ubyte ep_addr, int max_pkt, bool is_out) {
  int ep_num = ep_addr & 0x7f;
  int dir_in = !is_out;
  int dci = ep_num * 2 + (dir_in ? 1 : 0);

  slot_t *s = &g_slots[sid];

  volatile trb_t **ring;
  ulong *ring_phys;
  uint *enq, *pcs;

  if (is_out) {
    s->bulk_out_dci = dci;
    s->bulk_out_max_pkt = max_pkt;
    ring = &s->bulk_out_ring;
    ring_phys = &s->bulk_out_ring_phys;
    enq = &s->bulk_out_enq;
    pcs = &s->bulk_out_pcs;
  } else {
    s->bulk_in_dci = dci;
    s->bulk_in_max_pkt = max_pkt;
    ring = &s->bulk_in_ring;
    ring_phys = &s->bulk_in_ring_phys;
    enq = &s->bulk_in_enq;
    pcs = &s->bulk_in_pcs;
  }

  *ring_phys = pmm_alloc_pages(1);
  *ring = (volatile trb_t *)PHYS_TO_HHDM(*ring_phys);
  memset8((ubyte *)*ring, 0, 4096);
  *enq = 0;
  *pcs = 1;

  zero_ictx();

  ictx_icc()[1] = 1u | (1u << (uint)dci);

  volatile uint *dslot = (volatile uint *)PHYS_TO_HHDM(s->dev_ctx_phys);
  ictx_slot()[0] = dslot[0];
  ictx_slot()[1] = dslot[1];
  ictx_slot()[2] = dslot[2];
  ictx_slot()[3] = dslot[3];
  uint num_ctx = (uint)dci + 1;
  ictx_slot()[0] = (ictx_slot()[0] & ~(0x1fu << 27)) | (num_ctx << 27);

  // EP type: 2=Bulk OUT, 6=Bulk IN
  uint ep_type = is_out ? 2u : 6u;
  volatile uint *ep = ictx_ep((uint)dci);
  ep[0] = 0;
  ep[1] = (3u << 1) | (ep_type << 3) | ((uint)max_pkt << 16);
  ep[2] = (uint)(*ring_phys & 0xFFFFFFFF) | 1;
  ep[3] = (uint)(*ring_phys >> 32);
  ep[4] = (uint)max_pkt;

  mb();
  cmd_push(g_ictx_phys, 0, TRB_TYPE(TT_CONFIG_EP) | TRB_SLOT(sid));
  return cmd_wait(NULL);
}

// ── USB Mass Storage / Bulk-Only Transport ────────────────────────────────────

typedef struct {
  uint sig; // 0x43425355
  uint tag;
  uint data_len;
  ubyte flags; // 0x80=IN, 0x00=OUT
  ubyte lun;
  ubyte cb_len;
  ubyte cb[16];
} __attribute__((packed)) cbw_t;

typedef struct {
  uint sig; // 0x53425355
  uint tag;
  uint residue;
  ubyte status;
} __attribute__((packed)) csw_t;

// DMA buffers for BOT protocol (shared; single-threaded)
static ubyte *g_bot_cbw;
static ulong g_bot_cbw_phys;
static ubyte *g_bot_csw;
static ulong g_bot_csw_phys;
static ubyte *g_bot_data;
static ulong g_bot_data_phys;

static uint g_bot_tag = 1;

// Returns 0 on success
static int bot_command(int sid, ubyte *cb, ubyte cb_len,
                       bool dir_in, uint data_len) {
  slot_t *s = &g_slots[sid];
  uint tag = g_bot_tag++;

  cbw_t *cbw = (cbw_t *)g_bot_cbw;
  cbw->sig = 0x43425355;
  cbw->tag = tag;
  cbw->data_len = data_len;
  cbw->flags = dir_in ? 0x80 : 0x00;
  cbw->lun = 0;
  cbw->cb_len = cb_len;
  for (int i = 0; i < 16; i++)
    cbw->cb[i] = (i < cb_len) ? cb[i] : 0;
  mb();

  // Send CBW
  bulk_out_push(sid, g_bot_cbw_phys, 31);
  if (xfer_wait(sid, s->bulk_out_dci) != 0)
    return -1;

  // Data phase
  if (data_len > 0) {
    if (dir_in) {
      bulk_in_push(sid, g_bot_data_phys, data_len);
      if (xfer_wait(sid, s->bulk_in_dci) != 0)
        return -1;
    } else {
      bulk_out_push(sid, g_bot_data_phys, data_len);
      if (xfer_wait(sid, s->bulk_out_dci) != 0)
        return -1;
    }
  }

  // Receive CSW
  bulk_in_push(sid, g_bot_csw_phys, 13);
  if (xfer_wait(sid, s->bulk_in_dci) != 0)
    return -1;

  csw_t *csw = (csw_t *)g_bot_csw;
  if (csw->sig != 0x53425355 || csw->tag != tag || csw->status != 0)
    return -1;

  return 0;
}

static bool enumerate_msc(int slot_id, ubyte config_val,
                          ubyte msc_ep_out, ubyte msc_ep_in,
                          int msc_max_pkt) {
  // Set Configuration
  int cc = ctrl_xfer(slot_id, 0x00, 9, config_val, 0, 0, 0);
  if (cc != 0) {
    serial_write_line("xHCI MSC: Set Configuration failed");
    return false;
  }

  // Configure bulk OUT endpoint
  cc = configure_bulk_ep(slot_id, msc_ep_out, msc_max_pkt, true);
  if (cc != CC_SUCCESS) {
    serial_write_line("xHCI MSC: configure bulk OUT failed");
    return false;
  }

  // Configure bulk IN endpoint
  cc = configure_bulk_ep(slot_id, msc_ep_in, msc_max_pkt, false);
  if (cc != CC_SUCCESS) {
    serial_write_line("xHCI MSC: configure bulk IN failed");
    return false;
  }

  // Allocate BOT DMA buffers if not yet done
  if (!g_bot_cbw) {
    g_bot_cbw_phys = pmm_alloc_pages(1);
    g_bot_cbw = (ubyte *)PHYS_TO_HHDM(g_bot_cbw_phys);
    g_bot_csw_phys = pmm_alloc_pages(1);
    g_bot_csw = (ubyte *)PHYS_TO_HHDM(g_bot_csw_phys);
    g_bot_data_phys = pmm_alloc_pages(16); // 64 KiB max per transfer
    g_bot_data = (ubyte *)PHYS_TO_HHDM(g_bot_data_phys);
  }

  // Issue TEST UNIT READY to confirm the device is online
  ubyte cb[10] = {0};
  cb[0] = 0x00; // TEST UNIT READY
  if (bot_command(slot_id, cb, 6, false, 0) != 0) {
    serial_write_line("xHCI MSC: TEST UNIT READY failed");
    // Non-fatal — device may need a moment
  }

  g_slots[slot_id].msc_ready = true;
  g_msc_slot = slot_id;
  serial_write_line("xHCI MSC: USB mass storage ready");
  return true;
}

// ── Public MSC API ────────────────────────────────────────────────────────────

bool usb_msc_ok(void) {
  return g_msc_slot > 0 && g_slots[g_msc_slot].msc_ready;
}

bool usb_msc_read(uint lba, ubyte count, void *buf) {
  if (!usb_msc_ok())
    return false;

  uint bytes = (uint)count * 512;
  if (bytes > 16 * 4096)
    return false;

  ubyte cb[10];
  cb[0] = 0x28; // READ(10)
  cb[1] = 0;
  cb[2] = (ubyte)(lba >> 24);
  cb[3] = (ubyte)(lba >> 16);
  cb[4] = (ubyte)(lba >> 8);
  cb[5] = (ubyte)(lba);
  cb[6] = 0;
  cb[7] = 0;
  cb[8] = count;
  cb[9] = 0;

  if (bot_command(g_msc_slot, cb, 10, true, bytes) != 0)
    return false;

  memcpy8((ubyte *)buf, g_bot_data, bytes);
  return true;
}

// ── USB enumeration ───────────────────────────────────────────────────────────

// Returns true if keyboard was found and configured
static bool enumerate_port(int port) {
  serial_write("xHCI: enumerating port ");
  serial_write_hex8((ubyte)port);
  serial_write_char('\n');
  klogf(LOG_TRACE, "xHCI: enum port %u\n", (uint)port);

  if (!port_reset(port)) {
    serial_write_line("xHCI: port reset failed / not enabled");
    klogf(LOG_TRACE, "xHCI: port %u reset fail PORTSC=%x",
          (uint)port, PORT32(port, 0));
    return false;
  }
  klogf(LOG_TRACE, "xHCI: port %u reset ok PORTSC=%x",
        (uint)port, PORT32(port, 0));

  uint portsc = PORT32(port, 0);
  int speed = (int)PS_SPEED(portsc);

  // Enable Slot
  cmd_push(0, 0, TRB_TYPE(TT_ENABLE_SLOT));
  int slot_id;
  int cc = cmd_wait(&slot_id);
  if (cc != CC_SUCCESS || slot_id == 0) {
    serial_write_line("xHCI: Enable Slot failed");
    return false;
  }

  serial_write("xHCI: slot ");
  serial_write_hex8((ubyte)slot_id);
  serial_write_char('\n');

  slot_t *s = &g_slots[slot_id];
  s->active = 1;
  s->port = port;
  s->speed = speed;

  alloc_slot(slot_id);

  // Address Device (BSR=1, max_pkt=8 initial)
  cc = address_device(slot_id, port, speed, 8, true);
  if (cc != CC_SUCCESS) {
    serial_write_line("xHCI: Address Device (BSR) failed");
    return false;
  }

  // Get first 8 bytes of device descriptor to learn bMaxPacketSize0
  int max_pkt0 = 8;
  {
    cc = ctrl_xfer(slot_id, 0x80, 6, (1u << 8) | 0, 0, 8, g_dbuf_phys);
    if (cc == 0) {
      max_pkt0 = (int)g_dbuf[7];
      if (max_pkt0 < 8)
        max_pkt0 = 8;
    }
  }

  // Address Device (BSR=0, with real max packet size)
  cc = address_device(slot_id, port, speed, max_pkt0, false);
  if (cc != CC_SUCCESS) {
    serial_write_line("xHCI: Address Device failed");
    return false;
  }

  // Get full device descriptor (18 bytes) — mainly for logging
  ctrl_xfer(slot_id, 0x80, 6, (1u << 8) | 0, 0, 18, g_dbuf_phys);

  // Get configuration descriptor (first 4 bytes to learn wTotalLength)
  cc = ctrl_xfer(slot_id, 0x80, 6, (2u << 8) | 0, 0, 4, g_dbuf_phys);
  if (cc != 0) {
    serial_write_line("xHCI: get config descriptor failed");
    return false;
  }
  ushort total_len = (ushort)((ushort)g_dbuf[2] | ((ushort)g_dbuf[3] << 8));
  if (total_len > 512)
    total_len = 512;

  // Get full configuration descriptor
  cc = ctrl_xfer(slot_id, 0x80, 6, (2u << 8) | 0, 0, total_len, g_dbuf_phys);
  if (cc != 0) {
    serial_write_line("xHCI: get full config descriptor failed");
    return false;
  }

  // Parse configuration descriptor for HID keyboard or MSC
  ubyte config_val = g_dbuf[5];

  int kbd_iface = -1;
  ubyte kbd_ep_addr = 0;
  int kbd_interval = 10;
  int kbd_max_pkt = 8;

  int msc_iface = -1;
  ubyte msc_ep_out = 0;
  ubyte msc_ep_in = 0;
  int msc_max_pkt = 512;

  ubyte *desc = g_dbuf;
  ubyte *end = g_dbuf + total_len;
  int cur_iface = -1;
  ubyte cur_class = 0, cur_sub = 0, cur_proto = 0;

  while (desc < end && desc[0] >= 2) {
    ubyte dlen = desc[0];
    ubyte dtype = desc[1];

    if (dtype == 4) { // Interface descriptor
      cur_iface = desc[2];
      cur_class = desc[5];
      cur_sub = desc[6];
      cur_proto = desc[7];
      // HID boot keyboard
      if (cur_class == 3 && cur_sub == 1 && cur_proto == 1)
        kbd_iface = cur_iface;
      // Mass Storage / BOT
      if (cur_class == 8 && cur_sub == 6 && cur_proto == 0x50)
        msc_iface = cur_iface;
    } else if (dtype == 5) { // Endpoint descriptor
      int ep_max = (int)((ushort)desc[4] | ((ushort)desc[5] << 8)) & 0x7ff;
      ubyte ep_addr = desc[2];
      ubyte ep_attr = desc[3];

      if (cur_iface == kbd_iface && (ep_attr & 3) == 3 && (ep_addr & 0x80)) {
        kbd_ep_addr = ep_addr;
        kbd_max_pkt = ep_max;
        kbd_interval = (int)desc[6];
      }
      if (cur_iface == msc_iface && (ep_attr & 3) == 2) { // Bulk
        if (ep_addr & 0x80)
          msc_ep_in = ep_addr;
        else
          msc_ep_out = ep_addr;
        msc_max_pkt = ep_max;
      }
    }

    desc += dlen;
  }

  klogf(LOG_TRACE, "xHCI: kbd_iface=%d msc_iface=%d", kbd_iface, msc_iface);

  // Prefer MSC over keyboard if both somehow appear
  if (msc_iface != -1 && msc_ep_out != 0 && msc_ep_in != 0) {
    klogf(LOG_TRACE, "xHCI: trying MSC ep_out=%x ep_in=%x", msc_ep_out, msc_ep_in);
    return enumerate_msc(slot_id, config_val, msc_ep_out, msc_ep_in, msc_max_pkt);
  }

  if (kbd_iface == -1 || kbd_ep_addr == 0) {
    serial_write_line("xHCI: no HID boot keyboard or MSC found on device");
    klogf(LOG_TRACE, "xHCI: unknown device (class=%u sub=%u proto=%u)",
          cur_class, cur_sub, cur_proto);
    return false;
  }

  // Set Configuration
  cc = ctrl_xfer(slot_id, 0x00, 9, config_val, 0, 0, 0);
  if (cc != 0) {
    serial_write_line("xHCI: Set Configuration failed");
    return false;
  }

  // Set Protocol = 0 (Boot Protocol) — class request to interface
  cc = ctrl_xfer(slot_id, 0x21, 0x0B, 0, (ushort)kbd_iface, 0, 0);
  if (cc != 0) {
    serial_write_line("xHCI: Set Protocol failed");
    // Non-fatal — some keyboards ignore this
  }

  // Set Idle = 0 (no repeat rate) — class request to interface
  ctrl_xfer(slot_id, 0x21, 0x0A, 0, (ushort)kbd_iface, 0, 0);

  // Configure the interrupt IN endpoint in xHCI
  cc = configure_endpoint(slot_id, kbd_ep_addr, kbd_interval, kbd_max_pkt);
  if (cc != CC_SUCCESS) {
    serial_write_line("xHCI: Configure Endpoint failed");
    return false;
  }

  // Queue the first interrupt IN transfer
  s->kbd_ready = true;
  kbd_push_one(slot_id);

  serial_write_line("xHCI: USB keyboard ready");
  return true;
}

// ── xhci_poll ─────────────────────────────────────────────────────────────────

void xhci_poll(void) {
  if (!g_xhci_ok)
    return;

  evt_t e;
  while (evt_dequeue(&e)) {
    serial_write_line("USB EV");
    if (e.type == TT_XFER_EVT) {
      int sid = (int)GET_SLOT(e.ctrl);
      int ep_id = (int)GET_EPID(e.ctrl);
      int cc = (int)GET_CC(e.status);

      if (sid >= 1 && sid <= MAX_SLOTS) {
        slot_t *s = &g_slots[sid];
        if (s->kbd_ready && ep_id == s->kbd_dci && (cc == CC_SUCCESS || cc == CC_SHORT)) {
          process_report(sid);
          kbd_push_one(sid); // re-queue
        }
      }
    }
  }
}

// ── xhci_init ─────────────────────────────────────────────────────────────────

void xhci_init(void) {
  // Scan PCI for xHCI: class=0x0C, subclass=0x03, progif=0x30
  // Scan all functions — on real hardware xHCI is often func>0 on a
  // multi-function PCH device.
  // Collect up to 4 xHCI controllers; also log all USB-class devices found.
  struct {
    ubyte bus, dev, func;
  } xhci_list[4];
  int n_xhci = 0;

  for (int bus = 0; bus < 256; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      uint hdr_reg = pci_read32((ubyte)bus, (ubyte)dev, 0, 0x0C);
      int max_func = ((hdr_reg >> 16) & 0x80) ? 8 : 1;

      for (int func = 0; func < max_func; func++) {
        uint id = pci_read32((ubyte)bus, (ubyte)dev, (ubyte)func, 0);
        if ((id & 0xFFFF) == 0xFFFF)
          continue;

        uint class_reg = pci_read32((ubyte)bus, (ubyte)dev, (ubyte)func, 0x08);
        ubyte class_code = (ubyte)(class_reg >> 24);
        ubyte subclass = (ubyte)(class_reg >> 16);
        ubyte progif = (ubyte)(class_reg >> 8);

        if (class_code == 0x0C && subclass == 0x03) {
          const char *kind = (progif == 0x30)   ? "xHCI"
                             : (progif == 0x20) ? "EHCI"
                             : (progif == 0x10) ? "OHCI"
                                                : "USB?";
          klogf(LOG_TRACE, "USB: %s at %02x:%02x:%02x",
                kind, (uint)bus, (uint)dev, (uint)func);
          if (progif == 0x30 && n_xhci < 4) {
            xhci_list[n_xhci].bus = (ubyte)bus;
            xhci_list[n_xhci].dev = (ubyte)dev;
            xhci_list[n_xhci].func = (ubyte)func;
            n_xhci++;
          }
        }
      }
    }
  }

  if (n_xhci == 0) {
    serial_write_line("xHCI: no controller found");
    klogf(LOG_TRACE, "xHCI: no controller found");
    return;
  }

  // Try each xHCI controller in order until an MSC device is found.
  for (int ci = 0; ci < n_xhci && g_msc_slot == 0; ci++) {
    ubyte xhci_bus = xhci_list[ci].bus;
    ubyte xhci_dev = xhci_list[ci].dev;
    ubyte xhci_func = xhci_list[ci].func;

    serial_printf("xHCI: trying %02x:%02x:%02x\n", xhci_bus, xhci_dev, xhci_func);
    klogf(LOG_TRACE, "xHCI: trying %02x:%02x:%02x",
          (uint)xhci_bus, (uint)xhci_dev, (uint)xhci_func);

    pci_enable_bus_master(xhci_bus, xhci_dev, xhci_func);

    ulong mmio_phys = pci_get_bar(xhci_bus, xhci_dev, xhci_func);

    // Map 1MB of MMIO space as UC (cache-disable).
    vmm_map_bytes(&g_kernel_address_space,
                  (ulong)PHYS_TO_HHDM(mmio_phys), mmio_phys,
                  1024 * 1024, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE);

    // Invalidate TLB for the remapped range. vmm_map_pages never calls invlpg,
    // so the old WB entries from vmm_map_hhdm are still live; without this,
    // MMIO reads return stale cached data on hardware where MTRRs aren't UC.
    {
      ulong va = (ulong)PHYS_TO_HHDM(mmio_phys);
      ulong va_end = va + 1024 * 1024;
      for (; va < va_end; va += 4096)
        __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
    }

    g_cap = (volatile ubyte *)PHYS_TO_HHDM(mmio_phys);

    uint caplength = CAP8(CAP_CAPLENGTH);
    g_op = g_cap + caplength;
    g_rt = g_cap + (CAP32(CAP_RTSOFF) & ~0x1Fu);
    g_db = (volatile uint *)(g_cap + (CAP32(CAP_DBOFF) & ~3u));

    // Context size
    g_ctx_sz = (CAP32(CAP_HCCPARAMS1) & (1u << 2)) ? 64u : 32u;

    serial_write("xHCI: ctx_sz=");
    serial_write_hex8((ubyte)g_ctx_sz);
    serial_write_char('\n');

    // BIOS handoff
    uint xecp_off = ((CAP32(CAP_HCCPARAMS1) >> 16) & 0xFFFF) * 4;
    if (xecp_off >= 0x10)
      bios_handoff(xecp_off);

    if (!ctrl_reset()) {
      klogf(LOG_TRACE, "xHCI: ctrl_reset failed");
      continue;
    }

    // HCRST resets USBLEGSUP to 0, clearing both OS-owned and BIOS-owned bits.
    // BIOS firmware watches for this and may reclaim the controller and re-enable
    // USB SMI intercepts, causing all future PORTSC writes to be discarded.
    // Re-run the handoff after reset to reassert OS ownership.
    if (xecp_off >= 0x10)
      bios_handoff(xecp_off);

    // Allocate descriptor buffer
    g_dbuf_phys = pmm_alloc_pages(1);
    g_dbuf = (ubyte *)PHYS_TO_HHDM(g_dbuf_phys);
    memset8(g_dbuf, 0, 4096);

    if (!ctrl_init()) {
      klogf(LOG_TRACE, "xHCI: ctrl_init failed");
      continue;
    }

    uint max_ports = (CAP32(CAP_HCSPARAMS1) >> 24) & 0xFF;
    serial_write("xHCI: max ports=");
    serial_write_hex8((ubyte)max_ports);
    serial_write_char('\n');
    klogf(LOG_TRACE, "xHCI: max_ports=%u", max_ports);

    g_xhci_ok = true;

    // Wait ~200ms for devices to reconnect and complete link training after HCRST.
    for (volatile long i = 0; i < 200000000LL; i++)
      ;

    // Track which ports have been attempted so retries don't re-enumerate them.
    bool port_tried[32];
    for (int i = 0; i < 32; i++)
      port_tried[i] = false;

    // Initial scan — log all port states.
    for (uint p = 0; p < max_ports && p < 32; p++) {
      uint portsc = PORT32(p, 0);
      klogf(LOG_TRACE, "xHCI: port %u PORTSC=%x", p, portsc);
      if (portsc & PS_CCS) {
        port_tried[p] = true;
        enumerate_port((int)p);
      }
    }

    // Retry for up to ~2 s for SuperSpeed devices that need longer link training.
    for (int retry = 0; retry < 20 && g_msc_slot == 0; retry++) {
      for (volatile long j = 0; j < 100000000LL; j++)
        ;
      for (uint p = 0; p < max_ports && p < 32; p++) {
        if (port_tried[p])
          continue;
        uint portsc = PORT32(p, 0);
        if (portsc & PS_CCS) {
          port_tried[p] = true;
          klogf(LOG_TRACE, "xHCI: retry%d port %u PORTSC=%x", retry, p, portsc);
          enumerate_port((int)p);
        }
      }
    }

    klogf(LOG_TRACE, "xHCI: msc_slot=%u", (uint)g_msc_slot);
  } // end for (ci)
}
