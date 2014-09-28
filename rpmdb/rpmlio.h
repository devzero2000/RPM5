#ifndef H_RPMLIO
#define H_RPMLIO

/*@unchecked@*/
extern int _rpmlio_debug;

int rpmlioCreat(rpmdb rpmdb, const char * fn, mode_t mode,
		const uint8_t * b, size_t blen,
		const uint8_t * d, size_t dlen, uint32_t dalgo)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioUnlink(rpmdb rpmdb, const char * fn, mode_t mode,
		const uint8_t * b, size_t blen,
		const uint8_t * d, size_t dlen, uint32_t dalgo)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioRename(rpmdb rpmdb, const char * oldname, const char * newname,
		mode_t mode,
		const uint8_t * b, size_t blen,
		const uint8_t * d, size_t dlen, uint32_t dalgo)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioMkdir(rpmdb rpmdb, const char * dn, mode_t mode)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioRmdir(rpmdb rpmdb, const char * dn, mode_t mode)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioLsetfilecon(rpmdb rpmdb, const char * fn, const char * context)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioChown(rpmdb rpmdb, const char * fn, uid_t uid, gid_t gid)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioLchown(rpmdb rpmdb, const char * fn, uid_t uid, gid_t gid)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioChmod(rpmdb rpmdb, const char * fn, mode_t mode)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioUtime(rpmdb rpmdb, const char * fn, time_t actime, time_t modtime)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioSymlink(rpmdb rpmdb, const char * ln, const char * fn)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioLink(rpmdb rpmdb, const char * ln, const char * fn)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioMknod(rpmdb rpmdb, const char * fn, mode_t mode, dev_t dev)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioMkfifo(rpmdb rpmdb, const char * fn, mode_t mode)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioPrein(rpmdb rpmdb, const char ** av, const char * body)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioPostin(rpmdb rpmdb, const char ** av, const char * body)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioPreun(rpmdb rpmdb, const char ** av, const char * body)
	RPM_GNUC_CONST
	/*@*/;

int rpmlioPostun(rpmdb rpmdb, const char ** av, const char * body)
	RPM_GNUC_CONST
	/*@*/;

#endif	/* H_RPMLIO */
