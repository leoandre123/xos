#include "e1000.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/vmm.h"
#include "types.h"

// e1000 register offsets (relative to MMIO base)
#define E1000_CTRL      0x0000  // Device control
#define E1000_STATUS    0x0008  // Device status
#define E1000_EERD      0x0014  // EEPROM read
#define E1000_ICR       0x00C0  // Interrupt cause read
#define E1000_IMS       0x00D0  // Interrupt mask set
#define E1000_RCTL      0x0100  // Receive control
#define E1000_TCTL      0x0400  // Transmit control
#define E1000_RDBAL     0x2800  // Rx descriptor base low
#define E1000_RDBAH     0x2804  // Rx descriptor base high
#define E1000_RDLEN     0x2808  // Rx descriptor ring length (bytes)
#define E1000_RDH       0x2810  // Rx descriptor head
#define E1000_RDT       0x2818  // Rx descriptor tail
#define E1000_TDBAL     0x3800  // Tx descriptor base low
#define E1000_TDBAH     0x3804  // Tx descriptor base high
#define E1000_TDLEN     0x3808  // Tx descriptor ring length (bytes)
#define E1000_TDH       0x3810  // Tx descriptor head
#define E1000_TDT       0x3818  // Tx descriptor tail
#define E1000_MTA       0x5200  // Multicast table array (128 x 4 bytes)
#define E1000_RAL       0x5400  // Receive address low
#define E1000_RAH       0x5404  // Receive address high

// CTRL bits
#define E1000_CTRL_RST  (1 << 26) // Full reset

// RCTL bits
#define E1000_RCTL_EN       (1 << 1)  // Receiver enable
#define E1000_RCTL_BAM      (1 << 15) // Broadcast accept
#define E1000_RCTL_BSIZE_2K 0         // 2KB buffers (default)
#define E1000_RCTL_SECRC    (1 << 26) // Strip CRC

// TCTL bits
#define E1000_TCTL_EN   (1 << 1)  // Transmitter enable
#define E1000_TCTL_PSP  (1 << 3)  // Pad short packets

// Tx descriptor status bits
#define E1000_TXD_STAT_DD (1 << 0) // Descriptor done
// Tx descriptor command bits
#define E1000_TXD_CMD_EOP (1 << 0) // End of packet
#define E1000_TXD_CMD_RS  (1 << 3) // Report status

// Rx descriptor status bits
#define E1000_RXD_STAT_DD  (1 << 0) // Descriptor done
#define E1000_RXD_STAT_EOP (1 << 1) // End of packet

#define TX_DESC_COUNT 8
#define RX_DESC_COUNT 8
#define RX_BUFFER_SIZE 2048

// Transmit descriptor (16 bytes)
typedef struct {
  ulong addr;       // Physical address of packet buffer
  ushort length;
  ubyte  cso;       // Checksum offset (unused)
  ubyte  cmd;       // Command flags
  ubyte  status;
  ubyte  css;       // Checksum start (unused)
  ushort special;   // Unused
} __attribute__((packed)) tx_desc;

// Receive descriptor (16 bytes)
typedef struct {
  ulong addr;       // Physical address of packet buffer
  ushort length;
  ushort checksum;
  ubyte  status;
  ubyte  errors;
  ushort special;
} __attribute__((packed)) rx_desc;

static volatile ubyte *mmio_base;
static tx_desc *tx_descs;
static rx_desc *rx_descs;
static ubyte   *rx_buffers[RX_DESC_COUNT];
static int      tx_tail = 0;
static int      rx_tail = 0;
static ubyte    g_mac[6];

// Forward declaration — ethernet layer provides this
void ethernet_receive(ubyte *data, ushort len);

static uint e1000_read(uint reg) {
  return *((volatile uint *)(mmio_base + reg));
}

static void e1000_write(uint reg, uint val) {
  *((volatile uint *)(mmio_base + reg)) = val;
}

// Read MAC address from EEPROM
static void e1000_read_mac(ubyte mac[6]) {
  // Trigger EEPROM read for word 0
  e1000_write(E1000_EERD, (0 << 8) | 1);
  uint val;
  while (!((val = e1000_read(E1000_EERD)) & (1 << 4)));
  mac[0] = val >> 16;
  mac[1] = val >> 24;

  e1000_write(E1000_EERD, (1 << 8) | 1);
  while (!((val = e1000_read(E1000_EERD)) & (1 << 4)));
  mac[2] = val >> 16;
  mac[3] = val >> 24;

  e1000_write(E1000_EERD, (2 << 8) | 1);
  while (!((val = e1000_read(E1000_EERD)) & (1 << 4)));
  mac[4] = val >> 16;
  mac[5] = val >> 24;
}

static void e1000_init_rx(void) {
  rx_descs = kmalloc(sizeof(rx_desc) * RX_DESC_COUNT);

  for (int i = 0; i < RX_DESC_COUNT; i++) {
    rx_buffers[i] = kmalloc(RX_BUFFER_SIZE);
    rx_descs[i].addr   = vmm_virt_to_phys(&g_kernel_address_space, (ulong)rx_buffers[i]);
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
    tx_descs[i].status = E1000_TXD_STAT_DD; // mark all as done so they're free
  }

  ulong phys = vmm_virt_to_phys(&g_kernel_address_space, (ulong)tx_descs);
  e1000_write(E1000_TDBAL, (uint)(phys & 0xFFFFFFFF));
  e1000_write(E1000_TDBAH, (uint)(phys >> 32));
  e1000_write(E1000_TDLEN, TX_DESC_COUNT * sizeof(tx_desc));
  e1000_write(E1000_TDH, 0);
  e1000_write(E1000_TDT, 0);
  e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
}

void e1000_init(ulong mmio_phys) {
  // MMIO regions are not RAM so they're not covered by the HHDM mapping.
  // Explicitly map the NIC's 128KB register space at its HHDM-equivalent address.
  vmm_map_bytes(&g_kernel_address_space,
                (ulong)PHYS_TO_HHDM(mmio_phys), mmio_phys,
                128 * 1024, PAGE_PRESENT | PAGE_WRITABLE);

  mmio_base = (volatile ubyte *)PHYS_TO_HHDM(mmio_phys);

  // Reset
  e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
  // Wait for reset to clear
  while (e1000_read(E1000_CTRL) & E1000_CTRL_RST);

  // Clear multicast table
  for (int i = 0; i < 128; i++)
    e1000_write(E1000_MTA + i * 4, 0);

  e1000_read_mac(g_mac);
  serial_write("e1000 MAC: ");
  for (int i = 0; i < 6; i++) {
    serial_write_hex8(g_mac[i]);
    if (i < 5) serial_write_char(':');
  }
  serial_write_char('\n');

  e1000_init_rx();
  e1000_init_tx();

  serial_write_line("e1000 initialized!");
}

void e1000_get_mac(ubyte mac[6]) {
  for (int i = 0; i < 6; i++)
    mac[i] = g_mac[i];
}

void e1000_poll(void) {
  while (rx_descs[rx_tail].status & E1000_RXD_STAT_DD) {
    serial_write("e1000: got packet len=");
    serial_write_hex16(rx_descs[rx_tail].length);
    serial_write_char('\n');
    ethernet_receive(rx_buffers[rx_tail], rx_descs[rx_tail].length);

    // Give descriptor back to NIC
    rx_descs[rx_tail].status = 0;
    e1000_write(E1000_RDT, rx_tail);
    rx_tail = (rx_tail + 1) % RX_DESC_COUNT;
  }
}

void e1000_send(void *data, ushort len) {
  // Wait for the descriptor to be free
  while (!(tx_descs[tx_tail].status & E1000_TXD_STAT_DD));

  tx_descs[tx_tail].addr   = vmm_virt_to_phys(&g_kernel_address_space, (ulong)data);
  tx_descs[tx_tail].length = len;
  tx_descs[tx_tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_descs[tx_tail].status = 0;

  tx_tail = (tx_tail + 1) % TX_DESC_COUNT;
  e1000_write(E1000_TDT, tx_tail);
}
