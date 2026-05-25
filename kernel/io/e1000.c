#include "e1000.h"
#include "io/logging.h"
#include "io/pci.h"
#include "memory/heap.h"
#include "memory/vmm.h"
#include "net/drivers/ethernet_driver.h"
#include "net/ethernet.h"
#include "net_types.h"
#include "types.h"

// e1000 register offsets (relative to MMIO base)
#define E1000_CTRL   0x0000 // Device control
#define E1000_STATUS 0x0008 // Device status
#define E1000_EERD   0x0014 // EEPROM read
#define E1000_ICR    0x00C0 // Interrupt cause read
#define E1000_IMS    0x00D0 // Interrupt mask set
#define E1000_RCTL   0x0100 // Receive control
#define E1000_TCTL   0x0400 // Transmit control
#define E1000_RDBAL  0x2800 // Rx descriptor base low
#define E1000_RDBAH  0x2804 // Rx descriptor base high
#define E1000_RDLEN  0x2808 // Rx descriptor ring length (bytes)
#define E1000_RDH    0x2810 // Rx descriptor head
#define E1000_RDT    0x2818 // Rx descriptor tail
#define E1000_TDBAL  0x3800 // Tx descriptor base low
#define E1000_TDBAH  0x3804 // Tx descriptor base high
#define E1000_TDLEN  0x3808 // Tx descriptor ring length (bytes)
#define E1000_TDH    0x3810 // Tx descriptor head
#define E1000_TDT    0x3818 // Tx descriptor tail
#define E1000_MTA    0x5200 // Multicast table array (128 x 4 bytes)
#define E1000_RAL    0x5400 // Receive address low
#define E1000_RAH    0x5404 // Receive address high

// CTRL bits
#define E1000_CTRL_SLU (1 << 6)  // Set Link Up (required on I219)
#define E1000_CTRL_RST (1 << 26) // Full reset

// RCTL bits
#define E1000_RCTL_EN       (1 << 1)  // Receiver enable
#define E1000_RCTL_BAM      (1 << 15) // Broadcast accept
#define E1000_RCTL_BSIZE_2K 0         // 2KB buffers (default)
#define E1000_RCTL_SECRC    (1 << 26) // Strip CRC

// TCTL bits
#define E1000_TCTL_EN  (1 << 1) // Transmitter enable
#define E1000_TCTL_PSP (1 << 3) // Pad short packets

// Tx descriptor status bits
#define E1000_TXD_STAT_DD (1 << 0) // Descriptor done
// Tx descriptor command bits
#define E1000_TXD_CMD_EOP (1 << 0) // End of packet
#define E1000_TXD_CMD_RS  (1 << 3) // Report status

// Rx descriptor status bits
#define E1000_RXD_STAT_DD  (1 << 0) // Descriptor done
#define E1000_RXD_STAT_EOP (1 << 1) // End of packet

#define TX_DESC_COUNT  8
#define RX_DESC_COUNT  64
#define RX_BUFFER_SIZE 2048

// Transmit descriptor (16 bytes)
typedef struct {
  ulong addr;
  ushort length;
  ubyte cso;
  ubyte cmd;
  ubyte status;
  ubyte css;
  ushort special;
} __attribute__((packed)) tx_desc;

// Receive descriptor (16 bytes)
typedef struct {
  ulong addr;
  ushort length;
  ushort checksum;
  ubyte status;
  ubyte errors;
  ushort special;
} __attribute__((packed)) rx_desc;

static volatile ubyte *mmio_base;
static volatile tx_desc *tx_descs;
static volatile rx_desc *rx_descs;
static ubyte *rx_buffers[RX_DESC_COUNT];
static int tx_tail = 0;
static int rx_tail = 0;
static mac_addr g_mac;

// void ethernet_receive(ubyte *data, ushort len);

static uint e1000_read(uint reg) {
  return *((volatile uint *)(mmio_base + reg));
}

static void e1000_write(uint reg, uint val) {
  *((volatile uint *)(mmio_base + reg)) = val;
}

// Read MAC from Receive Address registers — works on both e1000 and e1000e/I219
static void e1000_read_mac(mac_addr *mac) {
  uint ral = e1000_read(E1000_RAL);
  uint rah = e1000_read(E1000_RAH);
  mac->parts[0] = (ral >> 0) & 0xFF;
  mac->parts[1] = (ral >> 8) & 0xFF;
  mac->parts[2] = (ral >> 16) & 0xFF;
  mac->parts[3] = (ral >> 24) & 0xFF;
  mac->parts[4] = (rah >> 0) & 0xFF;
  mac->parts[5] = (rah >> 8) & 0xFF;
}

static void e1000_init_rx(void) {
  rx_descs = kmalloc(sizeof(rx_desc) * RX_DESC_COUNT);

  for (int i = 0; i < RX_DESC_COUNT; i++) {
    rx_buffers[i] = kmalloc(RX_BUFFER_SIZE);
    rx_descs[i].addr = vmm_virt_to_phys(&g_kernel_address_space, (ulong)rx_buffers[i]);
    rx_descs[i].status = 0;
  }

  ulong phys = vmm_virt_to_phys(&g_kernel_address_space, (ulong)rx_descs);
  e1000_write(E1000_RDBAL, (uint)(phys & 0xFFFFFFFF));
  e1000_write(E1000_RDBAH, (uint)(phys >> 32));
  e1000_write(E1000_RDLEN, RX_DESC_COUNT * sizeof(rx_desc));
  e1000_write(E1000_RDH, 0);
  e1000_write(E1000_RDT, RX_DESC_COUNT - 1);
  e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);
}

static void e1000_init_tx(void) {
  tx_descs = kmalloc(sizeof(tx_desc) * TX_DESC_COUNT);

  for (int i = 0; i < TX_DESC_COUNT; i++) {
    tx_descs[i].status = E1000_TXD_STAT_DD;
  }

  ulong phys = vmm_virt_to_phys(&g_kernel_address_space, (ulong)tx_descs);
  e1000_write(E1000_TDBAL, (uint)(phys & 0xFFFFFFFF));
  e1000_write(E1000_TDBAH, (uint)(phys >> 32));
  e1000_write(E1000_TDLEN, TX_DESC_COUNT * sizeof(tx_desc));
  e1000_write(E1000_TDH, 0);
  e1000_write(E1000_TDT, 0);
  e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
}

void e1000_init(ubyte bus, ubyte dev, ubyte func) {
  pci_enable_bus_master(bus, dev, func);
  ulong mmio_phys = pci_get_bar(bus, dev, func);

  vmm_map_bytes(&g_kernel_address_space,
                (ulong)PHYS_TO_HHDM(mmio_phys), mmio_phys,
                128 * 1024, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE);
  {
    ulong va = (ulong)PHYS_TO_HHDM(mmio_phys);
    ulong va_end = va + 128 * 1024;
    for (; va < va_end; va += 4096)
      __asm__ volatile("invlpg (%0)" ::"r"(va) : "memory");
  }

  mmio_base = (volatile ubyte *)PHYS_TO_HHDM(mmio_phys);
  klogf(LOG_TRACE, "e1000: mmio_phys=%x", mmio_phys);

  uint ctrl_before = e1000_read(E1000_CTRL);
  klogf(LOG_TRACE, "e1000: CTRL before reset=%x", ctrl_before);

  // On I219-LM, writing CTRL.RST puts the PCIe device briefly offline.
  // Polling MMIO immediately causes completion timeouts (50-200ms each).
  // Spin without touching MMIO first, then poll with a bounded timeout.
  e1000_write(E1000_CTRL, ctrl_before | E1000_CTRL_RST);
  klogf(LOG_TRACE, "e1000: RST written, waiting...");
  for (volatile int j = 0; j < 2000000; j++)
    ;
  klogf(LOG_TRACE, "e1000: post-RST delay done, polling...");

  int rst_timeout = 1000;
  while ((e1000_read(E1000_CTRL) & E1000_CTRL_RST) && --rst_timeout)
    for (volatile int j = 0; j < 10000; j++)
      ;
  klogf(LOG_TRACE, "e1000: RST poll done timeout_left=%d", rst_timeout);
  if (!rst_timeout) {
    klogf(LOG_ERROR, "e1000: reset timeout, NIC not responding");
    return;
  }

  e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU);
  klogf(LOG_TRACE, "e1000: SLU set");

  for (int i = 0; i < 128; i++)
    e1000_write(E1000_MTA + i * 4, 0);
  klogf(LOG_TRACE, "e1000: MTA cleared");

  e1000_read_mac(&g_mac);
  klogf(LOG_TRACE, "e1000: MAC=%x:%x:%x:%x:%x:%x",
        g_mac.parts[0], g_mac.parts[1], g_mac.parts[2],
        g_mac.parts[3], g_mac.parts[4], g_mac.parts[5]);

  e1000_init_rx();
  klogf(LOG_TRACE, "e1000: RX init done");
  e1000_init_tx();
  klogf(LOG_TRACE, "e1000: initialized");
}

void e1000_get_mac(mac_addr *mac_out) {
  *mac_out = g_mac;
}

void e1000_poll(nic *nic) {
  while (rx_descs[rx_tail].status & E1000_RXD_STAT_DD) {
    ethernet_receive(rx_buffers[rx_tail], rx_descs[rx_tail].length, nic);
    rx_descs[rx_tail].status = 0;
    e1000_write(E1000_RDT, rx_tail);
    rx_tail = (rx_tail + 1) % RX_DESC_COUNT;
  }
}

void e1000_send(void *data, ushort len) {
  while (!(tx_descs[tx_tail].status & E1000_TXD_STAT_DD))
    ;

  tx_descs[tx_tail].addr = vmm_virt_to_phys(&g_kernel_address_space, (ulong)data);
  tx_descs[tx_tail].length = len;
  tx_descs[tx_tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_descs[tx_tail].status = 0;

  tx_tail = (tx_tail + 1) % TX_DESC_COUNT;
  e1000_write(E1000_TDT, tx_tail);
}

net_ops g_e1000_ops = {
    .init = e1000_init,
    .get_mac = e1000_get_mac,
    .poll = e1000_poll,
    .transmit = e1000_send,
    .send = ethernet_driver_send};