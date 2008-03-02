#ifndef H_IOSM
#define H_IOSM

/** \ingroup payload
 * \file rpmio/iosm.h
 * File state machine to handle archive I/O and system call's.
 */

/** \ingroup payload
 * File state machine data.
 */
typedef /*@abstract@*/ struct iosm_s * IOSM_t;

#include "cpio.h"

typedef	int iosmAction;

/*@-exportlocal@*/
/*@unchecked@*/
extern int _iosm_debug;
/*@=exportlocal@*/

/**
 */
#define	IOSM_VERBOSE	0x8000
#define	IOSM_INTERNAL	0x4000
#define	IOSM_SYSCALL	0x2000
#define	IOSM_DEAD	0x1000

#define	_fv(_a)		((_a) | IOSM_VERBOSE)
#define	_fi(_a)		((_a) | IOSM_INTERNAL)
#define	_fs(_a)		((_a) | (IOSM_INTERNAL | IOSM_SYSCALL))
#define	_fd(_a)		((_a) | (IOSM_INTERNAL | IOSM_DEAD))

typedef enum iosmStage_e {
    IOSM_UNKNOWN =   0,
    IOSM_INIT	=  _fd(1),
    IOSM_PRE	=  _fd(2),
    IOSM_PROCESS=  _fv(3),
    IOSM_POST	=  _fd(4),
    IOSM_UNDO	=  5,
    IOSM_FINI	=  6,

    IOSM_PKGINSTALL	= _fd(7),
    IOSM_PKGERASE	= _fd(8),
    IOSM_PKGBUILD	= _fd(9),
    IOSM_PKGCOMMIT	= _fd(10),
    IOSM_PKGUNDO	= _fd(11),

    IOSM_CREATE	=  _fd(17),
    IOSM_MAP	=  _fd(18),
    IOSM_MKDIRS	=  _fi(19),
    IOSM_RMDIRS	=  _fi(20),
    IOSM_MKLINKS=  _fi(21),
    IOSM_NOTIFY	=  _fd(22),
    IOSM_DESTROY=  _fd(23),
    IOSM_VERIFY	=  _fd(24),
    IOSM_COMMIT	=  _fd(25),

    IOSM_UNLINK	=  _fs(33),
    IOSM_RENAME	=  _fs(34),
    IOSM_MKDIR	=  _fs(35),
    IOSM_RMDIR	=  _fs(36),
    IOSM_LSETFCON= _fs(39),
    IOSM_CHOWN	=  _fs(40),
    IOSM_LCHOWN	=  _fs(41),
    IOSM_CHMOD	=  _fs(42),
    IOSM_UTIME	=  _fs(43),
    IOSM_SYMLINK=  _fs(44),
    IOSM_LINK	=  _fs(45),
    IOSM_MKFIFO	=  _fs(46),
    IOSM_MKNOD	=  _fs(47),
    IOSM_LSTAT	=  _fs(48),
    IOSM_STAT	=  _fs(49),
    IOSM_READLINK= _fs(50),
    IOSM_CHROOT	=  _fs(51),

    IOSM_NEXT	=  _fd(65),
    IOSM_EAT	=  _fd(66),
    IOSM_POS	=  _fd(67),
    IOSM_PAD	=  _fd(68),
    IOSM_TRAILER=  _fd(69),
    IOSM_HREAD	=  _fd(70),
    IOSM_HWRITE	=  _fd(71),
    IOSM_DREAD	=  _fs(72),
    IOSM_DWRITE	=  _fs(73),

    IOSM_ROPEN	=  _fs(129),
    IOSM_READ	=  _fs(130),
    IOSM_RCLOSE	=  _fs(131),
    IOSM_WOPEN	=  _fs(132),
    IOSM_WRITE	=  _fs(133),
    IOSM_WCLOSE	=  _fs(134)
} iosmStage;
#undef	_fv
#undef	_fi
#undef	_fs
#undef	_fd

/** \ingroup payload
 * Iterator across package file info, forward on install, backward on erase.
 */
typedef /*@abstract@*/ struct iosmIterator_s * IOSMI_t;

#if defined(_RPMIOSM_INTERNAL)
/** \ingroup payload
 * Keeps track of the set of all hard links to a file in an archive.
 */
struct hardLink_s {
/*@owned@*/ /*@relnull@*/
    struct hardLink_s * next;
/*@owned@*/
    const char ** nsuffix;
/*@owned@*/
    int * filex;
    struct stat sb;
    int nlink;
    int linksLeft;
    int linkIndex;
    int createdPath;
};

/** \ingroup payload
 * Iterator across package file info, forward on install, backward on erase.
 */
struct iosmIterator_s {
    void * ts;			/*!< transaction set. */
    void * fi;			/*!< transaction element file info. */
    int reverse;		/*!< reversed traversal? */
    int isave;			/*!< last returned iterator index. */
    int i;			/*!< iterator index. */
};
#endif

/** \ingroup payload
 * File name and stat information.
 */
struct iosm_s {
/*@owned@*/ /*@relnull@*/
    const char * path;		/*!< Current file name. */
/*@owned@*/ /*@relnull@*/
    const char * lpath;		/*!< Current link name. */
/*@owned@*/ /*@relnull@*/
    const char * opath;		/*!< Original file name. */
/*@relnull@*/
    FD_t cfd;			/*!< Payload file handle. */
/*@relnull@*/
    FD_t rfd;			/*!<  read: File handle. */
/*@dependent@*/ /*@relnull@*/
    char * rdbuf;		/*!<  read: Buffer. */
/*@owned@*/ /*@relnull@*/
    char * rdb;			/*!<  read: Buffer allocated. */
    size_t rdsize;		/*!<  read: Buffer allocated size. */
    size_t rdlen;		/*!<  read: Number of bytes requested.*/
    size_t rdnb;		/*!<  read: Number of bytes returned. */
    FD_t wfd;			/*!< write: File handle. */
/*@dependent@*/ /*@relnull@*/
    char * wrbuf;		/*!< write: Buffer. */
/*@owned@*/ /*@relnull@*/
    char * wrb;			/*!< write: Buffer allocated. */
    size_t wrsize;		/*!< write: Buffer allocated size. */
    size_t wrlen;		/*!< write: Number of bytes requested.*/
    size_t wrnb;		/*!< write: Number of bytes returned. */
/*@only@*/ /*@null@*/
    IOSMI_t iter;		/*!< File iterator. */
    int ix;			/*!< Current file iterator index. */
/*@only@*/ /*@relnull@*/
    struct hardLink_s * links;	/*!< Pending hard linked file(s). */
/*@only@*/ /*@relnull@*/
    struct hardLink_s * li;	/*!< Current hard linked file(s). */
/*@kept@*/ /*@null@*/
    unsigned int * archiveSize;	/*!< Pointer to archive size. */
/*@kept@*/ /*@null@*/
    const char ** failedFile;	/*!< First file name that failed. */
/*@shared@*/ /*@relnull@*/
    const char * subdir;	/*!< Current file sub-directory. */
/*@unused@*/
    char subbuf[64];	/* XXX eliminate */
/*@observer@*/ /*@relnull@*/
    const char * osuffix;	/*!< Old, preserved, file suffix. */
/*@observer@*/ /*@relnull@*/
    const char * nsuffix;	/*!< New, created, file suffix. */
/*@shared@*/ /*@relnull@*/
    const char * suffix;	/*!< Current file suffix. */
    char sufbuf[64];	/* XXX eliminate */
/*@only@*/ /*@null@*/
    short * dnlx;		/*!< Last dirpath verified indexes. */
/*@only@*/ /*@null@*/
    char * ldn;			/*!< Last dirpath verified. */
    int ldnlen;			/*!< Last dirpath current length. */
    int ldnalloc;		/*!< Last dirpath allocated length. */
    int postpone;		/*!< Skip remaining stages? */
    int diskchecked;		/*!< Has stat(2) been performed? */
    int exists;			/*!< Does current file exist on disk? */
    int mkdirsdone;		/*!< Have "orphan" dirs been created? */
    int astriplen;		/*!< Length of buildroot prefix. */
    int rc;			/*!< External file stage return code. */
    int commit;			/*!< Commit synchronously? */
    int repackaged;		/*!< Is payload repackaged? */
    cpioMapFlags mapFlags;	/*!< Bit(s) to control mapping. */
    int fdigestalgo;		/*!< Digest algorithm (~= PGPHASHALGO_MD5) */
    int digestlen;		/*!< No. of bytes in binary digest (~= 16) */
/*@shared@*/ /*@relnull@*/
    const char * dirName;	/*!< File directory name. */
/*@shared@*/ /*@relnull@*/
    const char * baseName;	/*!< File base name. */
/*@shared@*/ /*@relnull@*/
    const char * fdigest;	/*!< Hex digest (usually MD5, NULL disables). */
/*@shared@*/ /*@relnull@*/
    const unsigned char * digest;/*!< Bin digest (usually MD5, NULL disables). */
/*@dependent@*/ /*@observer@*/ /*@null@*/
    const char * fcontext;	/*!< File security context (NULL disables). */
    
    unsigned fflags;		/*!< File flags. */
    iosmAction action;		/*!< File disposition. */
    iosmStage goal;		/*!< Package state machine goal. */
    iosmStage stage;		/*!< External file stage. */
    iosmStage nstage;		/*!< Next file stage. */
    struct stat sb;		/*!< Current file stat(2) info. */
    struct stat osb;		/*!< Original file stat(2) info. */

    unsigned blksize;		/*!< Archive block size. */
    int (*headerRead) (void * _iosm, struct stat *st)
	/*@modifies _iosm, st @*/;
    int (*headerWrite) (void * _iosm, struct stat *st)
	/*@modifies _iosm, st @*/;
    int (*trailerWrite) (void * _iosm)
	/*@modifies _iosm @*/;

    char * lmtab;		/*!< ar(1) long member name table. */
    size_t lmtablen;		/*!< ar(1) no. bytes in lmtab. */
    size_t lmtaboff;		/*!< ar(1) current offset in lmtab. */
};

#ifdef __cplusplus
extern "C" {
#endif

/*@-exportlocal@*/
/**
 * Return formatted string representation of file stages.
 * @param a		file stage
 * @return		formatted string
 */
/*@observer@*/ const char * iosmStageString(iosmStage a)	/*@*/;

/**
 * Return formatted string representation of file disposition.
 * @param a		file dispostion
 * @return		formatted string
 */
/*@observer@*/ const char * iosmActionString(iosmAction a)	/*@*/;
/*@=exportlocal@*/

/**
 * Create I/O state machine instance.
 * @return		I/O state machine
 */
/*@only@*/ IOSM_t newIOSM(void)
	/*@*/;

/**
 * Destroy I/O state machine instance.
 * @param iosm		I/O state machine
 * @return		always NULL
 */
/*@null@*/ IOSM_t freeIOSM(/*@only@*/ /*@null@*/ IOSM_t iosm)
	/*@globals fileSystem @*/
	/*@modifies iosm, fileSystem @*/;

/**
 * Load external data into I/O state machine.
 * @param iosm		I/O state machine
 * @param goal
 * @param afmt		archive format (NULL uses cpio)
 * @param _ts		transaction set
 * @param _fi		transaction element file info
 * @param cfd		payload descriptor
 * @retval archiveSize	pointer to archive size
 * @retval failedFile	pointer to first file name that failed.
 * @return		0 on success
 */
int iosmSetup(IOSM_t iosm, iosmStage goal, /*@null@*/ const char * afmt,
		const void * _ts,
		const void * _fi,
		FD_t cfd,
		/*@out@*/ unsigned int * archiveSize,
		/*@out@*/ const char ** failedFile)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies iosm, ts, fi, *archiveSize, *failedFile,
		fileSystem, internalState @*/;

/**
 * Clean I/O state machine.
 * @param iosm		I/O state machine
 * @return		0 on success
 */
int iosmTeardown(IOSM_t iosm)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies iosm, fileSystem, internalState @*/;

#if defined(_RPMIOSM_INTERNAL)
/*@-exportlocal@*/
/**
 * Retrieve transaction set from I/O state machine iterator.
 * @param iosm		I/O state machine
 * @return		transaction set
 */
void * iosmGetTs(const IOSM_t iosm)
	/*@*/;

/**
 * Retrieve transaction element file info from I/O state machine iterator.
 * @param iosm		I/O state machine
 * @return		transaction element file info
 */
void * iosmGetFi(/*@partial@*/ const IOSM_t iosm)
	/*@*/;
#endif

/**
 * Map next file path and action.
 * @param iosm		I/O state machine
 */
int iosmMapPath(IOSM_t iosm)
	/*@modifies iosm @*/;

/**
 * Map file stat(2) info.
 * @param iosm		I/O state machine
 */
int iosmMapAttrs(IOSM_t iosm)
	/*@modifies iosm @*/;
/*@=exportlocal@*/

/**
 * File state machine driver.
 * @param iosm		I/O state machine
 * @param nstage	next stage
 * @return		0 on success
 */
int iosmNext(IOSM_t iosm, iosmStage nstage)
	/*@globals errno, h_errno, fileSystem, internalState @*/
	/*@modifies iosm, errno, fileSystem, internalState @*/;

/**
 * File state machine driver.
 * @param iosm		I/O state machine
 * @param stage		next stage
 * @return		0 on success
 */
/*@-exportlocal@*/
int XiosmStage(/*@partial@*/ IOSM_t iosm, iosmStage stage)
	/*@globals errno, h_errno, fileSystem, internalState @*/
	/*@modifies iosm, errno, fileSystem, internalState @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif	/* H_IOSM */
