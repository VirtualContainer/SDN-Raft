#ifndef _CRC32_H
#define _CRC32_H

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32_1(const void* data, size_t len, uint32_t prev_value);
uint32_t crc32_8(const void* data, size_t len, uint32_t prev_value);
uint32_t crc32_8_last8(const void* data, size_t len, uint32_t prev_value);

#ifdef __cplusplus
}
#endif

#endif
