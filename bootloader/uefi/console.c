#include "console.h"

void console_clear()
{
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
}

void console_set_cursor(UINTN col, UINTN row)
{
    uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, col, row);
}

void console_set_color(UINTN attr)
{
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, attr);
}

void console_show_cursor(BOOLEAN visible)
{
    uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, visible);
}

void console_write(const CHAR16 *text)
{
    uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, text);
}

void console_write_pos(const CHAR16 *text, int x, int y)
{
    console_set_cursor(x, y);
    console_write(text);
}

void console_write_line(const CHAR16 *text)
{
    console_write(text);
    console_write(L"\r\n");
}

EFI_INPUT_KEY wait_for_key()
{
    UINTN index;
    EFI_INPUT_KEY key;

    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);

    return key;
}

void console_draw_box(int x, int y, int width, int height)
{
    console_set_cursor(x, y);
    console_write(L"┌");
    for (int dx = 0; dx < width - 2; dx++)
    {
        console_write(L"─");
    }
    console_write(L"┐");
    for (int dy = 0; dy < height - 2; dy++)
    {
        console_set_cursor(x, y + dy + 1);
        console_write(L"│");
        console_set_cursor(x + width - 1, y + dy + 1);
        console_write(L"│");
    }

    console_set_cursor(x, y + height - 1);
    console_write(L"└");
    for (int dx = 0; dx < width - 2; dx++)
    {
        console_write(L"─");
    }
    console_write(L"┘");
}
