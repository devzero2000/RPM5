/*!\file crc.h
 * \brief CRC32, CRC64 and ADLER32 checksums.
 */

#include <sys/types.h>
#include <stdint.h>

#ifndef  _CRC_H
#define  _CRC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
uint32_t __crc32(uint32_t crc, const uint8_t * data, size_t size)
        /*@*/;

/**
 */
uint32_t __crc32_combine(uint32_t crc1, uint32_t crc2, size_t len2)
        /*@*/;

/**
 */
uint64_t __crc64(uint64_t crc, const uint8_t * data, size_t size)
        /*@*/;

/**
 */
uint64_t __crc64_combine(uint64_t crc1, uint64_t crc2, size_t len2)
        /*@*/;

/**
 */
uint32_t __adler32(uint32_t adler, const uint8_t * buf, uint32_t len)
        /*@*/;

/**
 */
uint32_t __adler32_combine(uint32_t adler1, uint32_t adler2, size_t len2)
        /*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* _CRC_H */
