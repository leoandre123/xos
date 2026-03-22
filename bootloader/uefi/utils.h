#pragma once

#define UEFI_STR(str) reinterpret_cast<const CHAR16 *>(u##str)
#define UEFI_CALL(func, argc, ...) uefi_call_wrapper(reinterpret_cast<void *>(func), argc, __VA_ARGS__)