#ifndef H_CPIO
#define H_CPIO

/** \ingroup payload
 * \file rpmio/cpio.h
 *  Structures used for cpio(1) archives.
 */

#define CPIO_NEWC_MAGIC	"070701"
#define CPIO_CRC_MAGIC	"070702"
#define CPIO_TRAILER	"TRAILER!!!"

/** \ingroup payload
 * Cpio archive header information.
 */
struct cpioCrcPhysicalHeader {
    char magic[6];
    char inode[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devMajor[8];
    char devMinor[8];
    char rdevMajor[8];
    char rdevMinor[8];
    char namesize[8];
    char checksum[8];			/* ignored !! */
};

#define	PHYS_HDR_SIZE	110		/* Don't depend on sizeof(struct) */

/*@unchecked@*/
extern int _cpio_debug;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write cpio trailer.
 * @retval _iosm	file path and stat info
 * @return		0 on success
 */
int cpioTrailerWrite(void * _iosm)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies _iosm, fileSystem, internalState @*/;

/**
 * Write cpio header.
 * @retval _iosm	file path and stat info
 * @param st
 * @return		0 on success
 */
int cpioHeaderWrite(void * _iosm, struct stat * st)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies _iosm, fileSystem, internalState @*/;

/**
 * Read cpio header.
 * @retval _iosm	file path and stat info
 * @retval st
 * @return		0 on success
 */
int cpioHeaderRead(void * _iosm, struct stat * st)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies _iosm, *st, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_CPIO */
