#include "../../shared/boot_info.h"
#include "console.h"
#include <efi.h>
#include <efilib.h>
#include <stdbool.h>

#define KERNEL_LOAD_ADDR 0x200000
#define MEMMAP_LOAD_ADDR 0x300000
#define MEMMAP_MAX_SIZE (64 * 1024)

typedef void (*KernelEntry)(BootInfo *);

// Load kernel.bin via TFTP when booted over PXE (no EFI filesystem on device
// handle).
static EFI_STATUS pxe_load_file(EFI_HANDLE image_handle, void **buffer,
                                UINTN *buffer_size) {
  EFI_LOADED_IMAGE *loaded_image = NULL;
  EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
  EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, image_handle,
                                        &li_guid, (void **)&loaded_image);
  if (EFI_ERROR(status))
    return status;

  EFI_GUID pxe_guid = EFI_PXE_BASE_CODE_PROTOCOL_GUID;
  EFI_PXE_BASE_CODE_PROTOCOL *pxe = NULL;
  status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle,
                             &pxe_guid, (void **)&pxe);
  if (EFI_ERROR(status))
    status = uefi_call_wrapper(BS->LocateProtocol, 3, &pxe_guid, NULL,
                               (void **)&pxe);
  if (EFI_ERROR(status))
    return status;

  // Server IP comes from the siaddr field of the DHCP ACK (the "next server").
  EFI_IP_ADDRESS server_ip;
  CopyMem(&server_ip, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr, 4);

  CHAR8 *filename = (CHAR8 *)"kernel.bin";

  // Probe file size first so we can allocate the right amount.
  UINT64 file_size = 0;
  status = uefi_call_wrapper(
      pxe->Mtftp, 10, pxe, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE, NULL, FALSE,
      &file_size, NULL, &server_ip, filename, NULL, FALSE);
  if (EFI_ERROR(status))
    return status;

  EFI_PHYSICAL_ADDRESS addr = KERNEL_LOAD_ADDR;
  status =
      uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData,
                        EFI_SIZE_TO_PAGES(file_size), &addr);
  if (EFI_ERROR(status))
    return status;

  UINT64 buf_size = file_size;
  status =
      uefi_call_wrapper(pxe->Mtftp, 10, pxe, EFI_PXE_BASE_CODE_TFTP_READ_FILE,
                        (void *)KERNEL_LOAD_ADDR, FALSE, &buf_size, NULL,
                        &server_ip, filename, NULL, FALSE);
  if (EFI_ERROR(status))
    return status;

  *buffer = (void *)KERNEL_LOAD_ADDR;
  *buffer_size = (UINTN)file_size;
  return EFI_SUCCESS;
}

static EFI_STATUS load_file(EFI_HANDLE image_handle, CHAR16 *path,
                            void **buffer, UINTN *buffer_size) {
  EFI_LOADED_IMAGE *loaded_image = NULL;
  EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

  EFI_STATUS status =
      uefi_call_wrapper(BS->HandleProtocol, 3, image_handle, &loaded_image_guid,
                        (void **)&loaded_image);
  if (EFI_ERROR(status))
    return status;

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
  EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

  status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle,
                             &fs_guid, (void **)&fs);
  if (EFI_ERROR(status))
    return status;

  EFI_FILE_HANDLE root;
  status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
  if (EFI_ERROR(status))
    return status;

  EFI_FILE_HANDLE file;
  status = uefi_call_wrapper(root->Open, 5, root, &file, path,
                             EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status))
    return status;

  UINTN info_size = SIZE_OF_EFI_FILE_INFO + 200;
  EFI_FILE_INFO *file_info = NULL;

  status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_size,
                             (void **)&file_info);
  if (EFI_ERROR(status))
    return status;

  status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo,
                             &info_size, file_info);
  if (EFI_ERROR(status))
    return status;

  *buffer_size = file_info->FileSize;
  *buffer = (void *)KERNEL_LOAD_ADDR;

  UINTN pages = EFI_SIZE_TO_PAGES(*buffer_size);
  EFI_PHYSICAL_ADDRESS addr = KERNEL_LOAD_ADDR;

  status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
                             EfiLoaderData, pages, &addr);
  if (EFI_ERROR(status))
    return status;

  status = uefi_call_wrapper(file->Read, 3, file, buffer_size, *buffer);

  return status;
}

static EFI_STATUS get_memory_map_copy(EFI_MEMORY_DESCRIPTOR **out_map,
                                      UINTN *out_map_size, UINTN *out_map_key,
                                      UINTN *out_desc_size,
                                      UINT32 *out_desc_version) {
  EFI_PHYSICAL_ADDRESS mmap_addr = MEMMAP_LOAD_ADDR;
  UINTN mmap_size = MEMMAP_MAX_SIZE;

  EFI_STATUS status =
      uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData,
                        EFI_SIZE_TO_PAGES(MEMMAP_MAX_SIZE), &mmap_addr);

  if (EFI_ERROR(status) && status != EFI_ALREADY_STARTED) {
    return status;
  }

  status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_size,
                             (EFI_MEMORY_DESCRIPTOR *)mmap_addr, out_map_key,
                             out_desc_size, out_desc_version);

  if (EFI_ERROR(status)) {
    return status;
  }

  *out_map = (EFI_MEMORY_DESCRIPTOR *)mmap_addr;
  *out_map_size = mmap_size;
  return EFI_SUCCESS;
}

bool menu() {
  EFI_INPUT_KEY key;

  int selection = 0;

  while (true) {
    console_clear();
    console_set_color(EFI_RED);
    console_write_line(L" __  _____  ___ ___ ");
    console_write_line(L" \\ \\/ / _ \\/ __| _ )");
    console_write_line(L"  >  < (_) \\__ \\ _ \\");
    console_write_line(L" /_/\\_\\___/|___/___/");
    console_write_line(L"XOS Bootloader!");

    console_draw_box(2, 10, 40, 10);
    console_set_color(selection == 0 ? EFI_WHITE | EFI_BACKGROUND_GREEN
                                     : EFI_WHITE);
    console_write_pos(L"Boot", 3, 11);
    console_set_color(selection == 1 ? EFI_WHITE | EFI_BACKGROUND_GREEN
                                     : EFI_WHITE);
    console_write_pos(L"Fast Boot", 3, 12);
    console_set_color(selection == 2 ? EFI_WHITE | EFI_BACKGROUND_GREEN
                                     : EFI_WHITE);
    console_write_pos(L"Exit", 3, 13);
    console_set_color(EFI_WHITE);
    console_write_pos(L"Use the ↑ and ↓ to navigate", 3, 17);
    console_write_pos(L"Press ENTER so select", 3, 18);

    key = wait_for_key();

    switch (key.ScanCode) {
    case SCAN_UP:
      selection--;
      break;
    case SCAN_DOWN:
      selection++;
      break;
    }
    if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      break;
    }

    selection = (selection + 3) % 3;
  }
  return selection == 1;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  InitializeLib(ImageHandle, SystemTable);

  Print(L"Hello from my bootloader\r\n");
  console_write(L"Hello, World!");
  bool fast_boot = menu();

  EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

  EFI_STATUS status =
      uefi_call_wrapper(BS->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);

  if (EFI_ERROR(status) || gop == NULL) {
    Print(L"No GOP / framebuffer\r\n");
    return status;
  }

  UINT32 *fb = (UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
  UINTN width = gop->Mode->Info->HorizontalResolution;
  UINTN height = gop->Mode->Info->VerticalResolution;
  UINTN pitch = gop->Mode->Info->PixelsPerScanLine;

  for (UINTN y = 0; y < height; y++) {
    for (UINTN x = 0; x < width; x++) {
      fb[y * pitch + x] = 0x0000FF00;
    }
  }

  Print(L"Framebuffer OK\r\n");

  void *kernel_buffer = NULL;
  UINTN kernel_size = 0;

  status =
      load_file(ImageHandle, L"\\kernel.bin", &kernel_buffer, &kernel_size);
  if (EFI_ERROR(status)) {
    Print(L"Disk load failed, trying PXE TFTP...\r\n");
    status = pxe_load_file(ImageHandle, &kernel_buffer, &kernel_size);
    if (EFI_ERROR(status)) {
      Print(L"Failed to load kernel.bin\r\n");
      return status;
    }
  }

  static BootInfo boot_info;
  boot_info.framebuffer_base = gop->Mode->FrameBufferBase;
  boot_info.framebuffer_width = gop->Mode->Info->HorizontalResolution;
  boot_info.framebuffer_height = gop->Mode->Info->VerticalResolution;
  boot_info.framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
  boot_info.fast_boot = fast_boot;

  // Detect boot device: walk the device path to determine type and PCI
  // location, then read GPT to find the data partition start LBA.
  EFI_LOADED_IMAGE *loaded_image = NULL;
  EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
  uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &li_guid,
                    (void **)&loaded_image);

  boot_info.boot_device.type = BOOT_DEVICE_IDE;
  boot_info.boot_device.data_lba = 0;
  boot_info.boot_device.pci.bus = 0;
  boot_info.boot_device.pci.dev = 0;
  boot_info.boot_device.pci.func = 0;

  if (loaded_image) {
    EFI_DEVICE_PATH *dp = DevicePathFromHandle(loaded_image->DeviceHandle);
    UINTN usb_depth = 0;
    while (dp && !IsDevicePathEnd(dp)) {
      UINT8 *b = (UINT8 *)dp;
      if (dp->Type == 1 && dp->SubType == 1) {
        // PCI node: byte[4]=Function, byte[5]=Device
        boot_info.boot_device.pci.func = b[4];
        boot_info.boot_device.pci.dev = b[5];
      } else if (dp->Type == 3 && dp->SubType == 5) {
        // USB node: byte[4]=ParentPortNumber — collect the full port chain
        boot_info.boot_device.type = BOOT_DEVICE_USB;
        if (usb_depth < 7)
          boot_info.boot_device.usb.port_path[usb_depth++] = b[4];
      } else if (dp->Type == 3 && dp->SubType == 18) {
        boot_info.boot_device.type = BOOT_DEVICE_AHCI;
      } else if (dp->Type == 3 && dp->SubType == 23) {
        boot_info.boot_device.type = BOOT_DEVICE_NVME;
      }
      dp = NextDevicePathNode(dp);
    }
    boot_info.boot_device.usb.port_path[usb_depth] = 0;

    // Find the data partition LBA by reading the GPT on the disk.
    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    UINTN num_handles = 0;
    EFI_HANDLE *handles = NULL;
    EFI_STATUS gpt_status =
        uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &bio_guid,
                          NULL, &num_handles, &handles);

    if (!EFI_ERROR(gpt_status)) {
      for (UINTN i = 0; i < num_handles; i++) {
        EFI_BLOCK_IO_PROTOCOL *bio = NULL;
        if (EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
                                        &bio_guid, (void **)&bio)))
          continue;
        // Skip logical partition handles; we want the raw disk.
        if (!bio || bio->Media->LogicalPartition)
          continue;

        // Read LBA 1 (GPT header).
        UINT8 gpt_hdr[512];
        if (EFI_ERROR(uefi_call_wrapper(bio->ReadBlocks, 5, bio,
                                        bio->Media->MediaId, 1, sizeof(gpt_hdr),
                                        gpt_hdr)))
          continue;

        // Verify GPT signature "EFI PART".
        const UINT8 sig[8] = {0x45, 0x46, 0x49, 0x20, 0x50, 0x41, 0x52, 0x54};
        bool is_gpt = true;
        for (int s = 0; s < 8; s++) {
          if (gpt_hdr[s] != sig[s]) {
            is_gpt = false;
            break;
          }
        }
        if (!is_gpt)
          continue;

        // PartitionEntryLBA at offset 72 (8 bytes), entry size at offset 84 (4
        // bytes), num entries at offset 80 (4 bytes).
        UINT64 entry_lba = *(UINT64 *)(gpt_hdr + 72);
        UINT32 num_entries = *(UINT32 *)(gpt_hdr + 80);
        UINT32 entry_size = *(UINT32 *)(gpt_hdr + 84);
        if (entry_size < 128 || num_entries == 0)
          continue;

        // Read all partition entries (up to 128 entries × 128 bytes = 16 KB).
        UINTN entries_bytes = num_entries * entry_size;
        UINT8 *entries = NULL;
        if (EFI_ERROR(uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
                                        entries_bytes, (void **)&entries)))
          continue;

        UINTN lbas_needed = (entries_bytes + 511) / 512;
        UINT8 *entry_buf = NULL;
        uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, lbas_needed * 512,
                          (void **)&entry_buf);

        bool found = false;
        if (entry_buf && !EFI_ERROR(uefi_call_wrapper(
                             bio->ReadBlocks, 5, bio, bio->Media->MediaId,
                             entry_lba, lbas_needed * 512, entry_buf))) {
          // ESP partition type GUID (little-endian):
          // C12A7328-F81F-11D2-BA4B-00A0C93EC93B
          const UINT8 esp_guid[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8,
                                      0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0,
                                      0xC9, 0x3E, 0xC9, 0x3B};

          for (UINT32 p = 0; p < num_entries && !found; p++) {
            UINT8 *ent = entry_buf + (UINTN)p * entry_size;
            // Skip empty entries (type GUID all zero).
            bool empty = true;
            for (int g = 0; g < 16; g++) {
              if (ent[g]) {
                empty = false;
                break;
              }
            }
            if (empty)
              continue;

            // Skip ESP.
            bool is_esp = true;
            for (int g = 0; g < 16; g++) {
              if (ent[g] != esp_guid[g]) {
                is_esp = false;
                break;
              }
            }
            if (is_esp)
              continue;

            // Use the first non-ESP partition as the data partition.
            UINT64 start_lba = *(UINT64 *)(ent + 32);
            boot_info.boot_device.data_lba = start_lba;
            found = true;
          }
        }

        if (entry_buf)
          uefi_call_wrapper(BS->FreePool, 1, entry_buf);
        uefi_call_wrapper(BS->FreePool, 1, entries);

        if (found)
          break;
      }
      uefi_call_wrapper(BS->FreePool, 1, handles);
    }
  }

  EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
  UINTN memory_map_size = 0;
  UINTN map_key = 0;
  UINTN desc_size = 0;
  UINT32 desc_version = 0;

  status = get_memory_map_copy(&memory_map, &memory_map_size, &map_key,
                               &desc_size, &desc_version);
  if (EFI_ERROR(status)) {
    Print(L"GetMemoryMap failed\r\n");
    return status;
  }

  boot_info.memory_map = (uint64_t)(UINTN)memory_map;
  boot_info.memory_map_size = memory_map_size;
  boot_info.memory_map_desc_size = desc_size;
  boot_info.memory_map_desc_version = desc_version;

  status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, map_key);

  KernelEntry entry = (KernelEntry)KERNEL_LOAD_ADDR;
  entry(&boot_info);

  for (;;) {
    uefi_call_wrapper(BS->Stall, 1, 1000000);
  }

  return EFI_SUCCESS;
}
