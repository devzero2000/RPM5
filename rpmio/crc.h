/*!\file crc.h
 * \brief CRC32, CRC64 and ADLER32 checksums.
 */

#include <sys/types.h>
#include <stdint.h>

#ifndef  _CRC_H
#define  _CRC_H

/**
 */
typedef struct {
    uint32_t crc;
    uint32_t (*update)  (uint32_t crc, const uint8_t * data, size_t size);
    uint32_t (*combine) (uint32_t crc1, uint32_t crc2, size_t len2);
} sum32Param;

/**
 */
typedef struct {
    uint64_t crc;
    uint64_t (*update)  (uint64_t crc, const uint8_t * data, size_t size);
    uint64_t (*combine) (uint64_t crc1, uint64_t crc2, size_t len2);
} sum64Param;

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

/**
 */
int sum32Reset(sum32Param * mp)
	/*@modifies *mp @*/;

/**
 */
int sum32Update(sum32Param * mp, const uint8_t * data, size_t size)
	/*@modifies *mp @*/;

/**
 */
int sum32Digest(sum32Param * mp, uint8_t * data)
        /*@modifies *mp, data @*/;

/**
 */
int sum64Reset(sum64Param * mp)
	/*@modifies *mp @*/;

/**
 */
int sum64Update(sum64Param * mp, const uint8_t * data, size_t size)
	/*@modifies *mp @*/;

/**
 */
int sum64Digest(sum64Param * mp, uint8_t * data)
        /*@modifies *mp, data @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _CRC_H */
