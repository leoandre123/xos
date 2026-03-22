#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include "console.h"
#include "../../shared/boot_info.h"

#define KERNEL_LOAD_ADDR 0x100000
#define MEMMAP_LOAD_ADDR 0x300000
#define MEMMAP_MAX_SIZE (64 * 1024)

typedef void (*KernelEntry)(BootInfo *);

static EFI_STATUS load_file(EFI_HANDLE image_handle,
                            CHAR16 *path,
                            void **buffer,
                            UINTN *buffer_size)
{
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    EFI_STATUS status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        image_handle,
        &loaded_image_guid,
        (void **)&loaded_image);
    if (EFI_ERROR(status))
        return status;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        loaded_image->DeviceHandle,
        &fs_guid,
        (void **)&fs);
    if (EFI_ERROR(status))
        return status;

    EFI_FILE_HANDLE root;
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status))
        return status;

    EFI_FILE_HANDLE file;
    status = uefi_call_wrapper(
        root->Open,
        5,
        root,
        &file,
        path,
        EFI_FILE_MODE_READ,
        0);
    if (EFI_ERROR(status))
        return status;

    UINTN info_size = SIZE_OF_EFI_FILE_INFO + 200;
    EFI_FILE_INFO *file_info = NULL;

    status = uefi_call_wrapper(
        BS->AllocatePool,
        3,
        EfiLoaderData,
        info_size,
        (void **)&file_info);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(
        file->GetInfo,
        4,
        file,
        &GenericFileInfo,
        &info_size,
        file_info);
    if (EFI_ERROR(status))
        return status;

    *buffer_size = file_info->FileSize;
    *buffer = (void *)KERNEL_LOAD_ADDR;

    UINTN pages = EFI_SIZE_TO_PAGES(*buffer_size);
    EFI_PHYSICAL_ADDRESS addr = KERNEL_LOAD_ADDR;

    status = uefi_call_wrapper(
        BS->AllocatePages,
        4,
        AllocateAddress,
        EfiLoaderData,
        pages,
        &addr);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(
        file->Read,
        3,
        file,
        buffer_size,
        *buffer);

    return status;
}

static EFI_STATUS get_memory_map_copy(
    EFI_MEMORY_DESCRIPTOR **out_map,
    UINTN *out_map_size,
    UINTN *out_map_key,
    UINTN *out_desc_size,
    UINT32 *out_desc_version)
{
    EFI_PHYSICAL_ADDRESS mmap_addr = MEMMAP_LOAD_ADDR;
    UINTN mmap_size = MEMMAP_MAX_SIZE;

    EFI_STATUS status = uefi_call_wrapper(
        BS->AllocatePages, 4,
        AllocateAddress,
        EfiLoaderData,
        EFI_SIZE_TO_PAGES(MEMMAP_MAX_SIZE),
        &mmap_addr);

    if (EFI_ERROR(status) && status != EFI_ALREADY_STARTED)
    {
        return status;
    }

    status = uefi_call_wrapper(
        BS->GetMemoryMap, 5,
        &mmap_size,
        (EFI_MEMORY_DESCRIPTOR *)mmap_addr,
        out_map_key,
        out_desc_size,
        out_desc_version);

    if (EFI_ERROR(status))
    {
        return status;
    }

    *out_map = (EFI_MEMORY_DESCRIPTOR *)mmap_addr;
    *out_map_size = mmap_size;
    return EFI_SUCCESS;
}

void menu()
{
    EFI_INPUT_KEY key;

    int selection = 0;

    while (true)
    {
        console_clear();
        console_set_color(EFI_RED);
        console_write_line(L" __  _____  ___ ___ ");
        console_write_line(L" \\ \\/ / _ \\/ __| _ )");
        console_write_line(L"  >  < (_) \\__ \\ _ \\");
        console_write_line(L" /_/\\_\\___/|___/___/");
        console_write_line(L"XOS Bootloader!");

        console_draw_box(2, 10, 40, 10);
        console_set_color(selection == 0 ? EFI_WHITE | EFI_BACKGROUND_GREEN : EFI_WHITE);
        console_write_pos(L"Boot", 3, 11);
        console_set_color(selection == 1 ? EFI_WHITE | EFI_BACKGROUND_GREEN : EFI_WHITE);
        console_write_pos(L"Settings", 3, 12);
        console_set_color(selection == 2 ? EFI_WHITE | EFI_BACKGROUND_GREEN : EFI_WHITE);
        console_write_pos(L"Exit", 3, 13);
        console_set_color(EFI_WHITE);
        console_write_pos(L"Use the ↑ and ↓ to navigate", 3, 17);
        console_write_pos(L"Press ENTER so select", 3, 18);

        key = wait_for_key();

        switch (key.ScanCode)
        {
        case SCAN_UP:
            selection--;
            break;
        case SCAN_DOWN:
            selection++;
            break;
        }
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN)
        {
            break;
        }

        selection = (selection + 3) % 3;
    }
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hello from my bootloader\r\n");
    console_write(L"Hello, World!");
    menu();

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    EFI_STATUS status = uefi_call_wrapper(
        BS->LocateProtocol,
        3,
        &gop_guid,
        NULL,
        (void **)&gop);

    if (EFI_ERROR(status) || gop == NULL)
    {
        Print(L"No GOP / framebuffer\r\n");
        return status;
    }

    UINT32 *fb = (UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
    UINTN width = gop->Mode->Info->HorizontalResolution;
    UINTN height = gop->Mode->Info->VerticalResolution;
    UINTN pitch = gop->Mode->Info->PixelsPerScanLine;

    for (UINTN y = 0; y < height; y++)
    {
        for (UINTN x = 0; x < width; x++)
        {
            fb[y * pitch + x] = 0x0000FF00;
        }
    }

    Print(L"Framebuffer OK\r\n");

    void *kernel_buffer = NULL;
    UINTN kernel_size = 0;

    status = load_file(ImageHandle, L"\\kernel.bin", &kernel_buffer, &kernel_size);
    if (EFI_ERROR(status))
    {
        Print(L"Failed to load kernel.bin\r\n");
        return status;
    }

    static BootInfo boot_info;
    boot_info.framebuffer_base = gop->Mode->FrameBufferBase;
    boot_info.framebuffer_width = gop->Mode->Info->HorizontalResolution;
    boot_info.framebuffer_height = gop->Mode->Info->VerticalResolution;
    boot_info.framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * 4;

    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN memory_map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;

    status = get_memory_map_copy(
        &memory_map,
        &memory_map_size,
        &map_key,
        &desc_size,
        &desc_version);
    if (EFI_ERROR(status))
    {
        Print(L"GetMemoryMap failed\r\n");
        return status;
    }

    boot_info.memory_map = (uint64_t)(UINTN)memory_map;
    boot_info.memory_map_size = memory_map_size;
    boot_info.memory_map_desc_size = desc_size;
    boot_info.memory_map_desc_version = desc_version;

    status = uefi_call_wrapper(
        BS->ExitBootServices, 2,
        ImageHandle,
        map_key);

    KernelEntry entry = (KernelEntry)KERNEL_LOAD_ADDR;
    entry(&boot_info);

    for (;;)
    {
        uefi_call_wrapper(BS->Stall, 1, 1000000);
    }

    return EFI_SUCCESS;
}
