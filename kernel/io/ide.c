#include "ide.h"
#include "io/io.h"
#include "io/pci.h"
#include "types.h"

typedef struct {
  ushort base;           // I/O Base.
  ushort ctrl;           // Control Base
  ushort busmaster_base; // Bus Master IDE
  ubyte irq;             // nIEN (No Interrupt);
} ide_channel;

typedef struct {
  ubyte bus;
  ubyte dev;
  ubyte func;
  ubyte prog_if;
  ide_channel channels[2];
} ide_controller;

typedef struct {
  bool exists;         // 0 (Empty) or 1 (This Drive really exists).
  ubyte channel;       // 0 (Primary Channel) or 1 (Secondary Channel).
  ubyte drive;         // 0 (Master Drive) or 1 (Slave Drive).
  ushort type;         // 0: ATA, 1:ATAPI.
  ushort signature;    // Drive Signature
  ushort capabilities; // Features.
  uint command_sets;   // Command Sets Supported.
  uint sector_count;   // Size in Sectors.
  ubyte model[41];     // Model in string.
  ide_controller *ctrl;
} ide_device;

#define IDE_MAX_CONTROLLERS 4
#define IDE_MAX_DEVICES     16

static ide_controller g_controllers[IDE_MAX_CONTROLLERS];
static bool g_controller_used[IDE_MAX_CONTROLLERS] = {0};

static ide_device g_devices[IDE_MAX_DEVICES];
static bool g_device_used[IDE_MAX_DEVICES] = {0};

static ide_controller *allocate_controller() {
  for (int i = 0; i < IDE_MAX_CONTROLLERS; i++) {
    if (!g_controller_used[i]) {
      g_controller_used[i] = true;
      return &g_controllers[i];
    }
  }
  return 0;
}
static void free_controller(ide_controller *ctrl) {
  int idx = ctrl - g_controllers;
  if (idx >= 0 && idx < IDE_MAX_CONTROLLERS) {
    g_controller_used[idx] = false;
  }
}
static ide_device *allocate_device() {
  for (int i = 0; i < IDE_MAX_DEVICES; i++) {
    if (!g_device_used[i]) {
      g_device_used[i] = true;
      return &g_devices[i];
    }
  }
  return 0;
}
static void free_device(ide_device *dev) {
  int idx = dev - g_devices;
  if (idx >= 0 && idx < IDE_MAX_DEVICES) {
    g_device_used[idx] = false;
  }
}

static void ide_setup_channels(ide_controller *ctrl,
                               uint bar0, uint bar1, uint bar2, uint bar3, uint bar4) {

  // primary
  if (ctrl->prog_if & 0x01) {
    ctrl->channels[0].base = (bar0 & ~0x3);
    ctrl->channels[0].ctrl = (bar1 & ~0x3);
  } else {
    ctrl->channels[0].base = 0x1F0;
    ctrl->channels[0].ctrl = 0x3F6;
  }

  // secondary
  if (ctrl->prog_if & 0x04) {
    ctrl->channels[1].base = (bar2 & ~0x3);
    ctrl->channels[1].ctrl = (bar3 & ~0x3);
  } else {
    ctrl->channels[1].base = 0x170;
    ctrl->channels[1].ctrl = 0x376;
  }

  if (ctrl->prog_if & 0x80) {
    ushort bm = (bar4 & ~0x3);
    ctrl->channels[0].busmaster_base = bm + 0;
    ctrl->channels[1].busmaster_base = bm + 8;
  } else {
    ctrl->channels[0].busmaster_base = 0;
    ctrl->channels[1].busmaster_base = 0;
  }

  ctrl->channels[0].irq = (ctrl->prog_if & 0x01) ? 0 : 14;
  ctrl->channels[1].irq = (ctrl->prog_if & 0x04) ? 0 : 15;
}

static void ide_reset_channel(ide_channel *ch) {
  outb(ch->ctrl, 0x04);
  io_wait();
  outb(ch->ctrl, 0x00);

  for (volatile int i = 0; i < 100000; i++) {
  }
}
static bool ide_identify(ide_channel *ch, ubyte drive, ushort *buf) {
  ushort io = ch->base;

  outb(io + 6, drive ? 0xB0 : 0xA0);
  io_wait();

  outb(io + 2, 0);
  outb(io + 3, 0);
  outb(io + 4, 0);
  outb(io + 5, 0);
  outb(io + 7, 0xEC);

  ubyte status = inb(io + 7);
  if (status == 0)
    return false;

  while (inb(io + 7) & 0x80) {
  }

  while (true) {
    status = inb(io + 7);
    if (status & 0x01)
      return false;
    if (status & 0x08)
      break;
  }

  for (int i = 0; i < 256; i++) {
    buf[i] = inw(io + 0);
  }

  return true;
}
static void ide_probe_device(ide_controller *ctrl, ubyte channel, ubyte drive) {
  ushort id[256];
  if (!ide_identify(&ctrl->channels[channel], drive, id))
    return;

  ide_device *dev = allocate_device();
  if (!dev)
    return;
  dev->exists = true;
  dev->ctrl = ctrl;
  dev->channel = channel;
  dev->drive = drive;
  dev->sector_count = ((uint)id[61] << 16) | id[60];
}
static void ide_probe_channel(ide_controller *ctrl, ubyte channel_index) {
  ide_probe_device(ctrl, channel_index, 0); // master
  ide_probe_device(ctrl, channel_index, 1); // slave
}

static ide_device *ide_first_device(void) {
  for (int i = 0; i < IDE_MAX_DEVICES; i++) {
    if (g_device_used[i] && g_devices[i].exists)
      return &g_devices[i];
  }
  return 0;
}

static bool ide_wait_ready(ushort io) {
  ubyte status;
  while ((status = inb(io + ATA_REG_STATUS)) & ATA_SR_BSY) {
  }
  if (status & ATA_SR_ERR)
    return false;
  return true;
}

static bool ide_wait_drq(ushort io) {
  ubyte status;
  while (true) {
    status = inb(io + ATA_REG_STATUS);
    if (status & ATA_SR_ERR)
      return false;
    if (status & ATA_SR_DRQ)
      return true;
  }
}

static bool ide_pio_access(ide_device *dev, uint lba, ubyte count, void *buf, bool write) {
  ide_channel *ch = &dev->ctrl->channels[dev->channel];
  ushort io = ch->base;

  if (!ide_wait_ready(io))
    return false;

  // LBA28: top 4 bits of LBA go in bits[3:0] of HDDEVSEL
  outb(io + ATA_REG_HDDEVSEL, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
  io_wait();

  outb(io + ATA_REG_FEATURES, 0x00);
  outb(io + ATA_REG_SECCOUNT0, count);
  outb(io + ATA_REG_LBA0, (lba >> 0) & 0xFF);
  outb(io + ATA_REG_LBA1, (lba >> 8) & 0xFF);
  outb(io + ATA_REG_LBA2, (lba >> 16) & 0xFF);
  outb(io + ATA_REG_COMMAND, write ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO);

  for (ubyte s = 0; s < count; s++) {
    if (!ide_wait_ready(io) || !ide_wait_drq(io))
      return false;

    ushort *p = (ushort *)buf + s * 256;
    if (write) {
      for (int i = 0; i < 256; i++)
        outw(io + ATA_REG_DATA, p[i]);
    } else {
      for (int i = 0; i < 256; i++)
        p[i] = inw(io + ATA_REG_DATA);
    }
  }

  if (write) {
    outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ide_wait_ready(io);
  }

  return true;
}

bool ide_read_sectors(uint lba, ubyte count, void *buf) {
  ide_device *dev = ide_first_device();
  if (!dev)
    return false;
  return ide_pio_access(dev, lba, count, buf, false);
}

bool ide_write_sectors(uint lba, ubyte count, const void *buf) {
  ide_device *dev = ide_first_device();
  if (!dev)
    return false;
  return ide_pio_access(dev, lba, count, (void *)buf, true);
}

void ide_init_controller(ubyte bus, ubyte dev, ubyte func) {
  ide_controller *ctrl = allocate_controller();

  ctrl->bus = bus;
  ctrl->dev = dev;
  ctrl->func = func;
  ctrl->prog_if = pci_get_progif(bus, dev, func);

  uint bar0 = pci_get_bar0(bus, dev, func);
  uint bar1 = pci_get_bar1(bus, dev, func);
  uint bar2 = pci_get_bar2(bus, dev, func);
  uint bar3 = pci_get_bar3(bus, dev, func);
  uint bar4 = pci_get_bar4(bus, dev, func);

  ide_setup_channels(ctrl, bar0, bar1, bar2, bar3, bar4);

  ide_reset_channel(&ctrl->channels[0]);
  ide_reset_channel(&ctrl->channels[1]);

  ide_probe_device(ctrl, 0, 0);
  ide_probe_device(ctrl, 0, 1);
  ide_probe_device(ctrl, 1, 0);
  ide_probe_device(ctrl, 1, 1);
}
