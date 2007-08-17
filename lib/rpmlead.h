#ifndef _H_RPMLEAD
#define _H_RPMLEAD

/** \ingroup lead
 * \file lib/rpmlead.h
 * Routines to read and write an rpm lead structure for a a package.
 */

#if defined(_RPMLEAD_INTERNAL)

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

#define	RPMLEAD_SIZE 96		/*!< Don't rely on sizeof(struct) */

/** \ingroup lead
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead {
    unsigned char magic[4];
    unsigned char major;
    unsigned char minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;	/*!< Signature header type (RPMSIG_HEADERSIG) */
/*@unused@*/ char reserved[16];	/*!< Pad to 96 bytes -- 8 byte aligned! */
} ;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*@unchecked@*/
extern int _nolead;

/** \ingroup lead
 * Write lead to file handle.
 * @param fd		file handle
 * @param lead		package lead
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
rpmRC writeLead(FD_t fd, const struct rpmlead *lead)
	/*@globals fileSystem @*/
	/*@modifies fd, fileSystem @*/;

/** \ingroup lead
 * Read lead from file handle.
 * @param fd		file handle
 * @retval *leadp	package lead
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
rpmRC readLead(FD_t fd, /*@null@*/ /*@out@*/ struct rpmlead ** leadp,
		const char ** msg)
	/*@modifies fd, *lead, *msg @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMLEAD */
