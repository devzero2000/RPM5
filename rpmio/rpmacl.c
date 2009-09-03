/** \ingroup rpmio
 * \file rpmio/rpmacl.c
 */

#include "system.h"

#include <sys/acl.h>
#include <rpmacl.h>

#include "debug.h"

static inline int __acl_entries(acl_t a)
{
    int count;
#if defined(__FreeBSD__)	/* XXX others too. HAVE_ACL_ACL_CNT AutoFu? */
    struct acl * aclp = &a->ats_acl;
    count = aclp->acl_cnt;
#else
    int id = ACL_FIRST_ENTRY;
    acl_entry_t ace;
	
    count = 0;
    while (acl_get_entry(a, id, &ace) > 0) {
	id = ACL_NEXT_ENTRY;
	count++;
    }
#endif
    return count;
}

rpmRC rpmaclCopyFd(FD_t ifd, FD_t ofd)
{
    int ifdno = Fileno(ifd);
    int ofdno = Fileno(ofd);
    acl_t a;
    int count;

    if (ifdno < 0 || ofdno < 0)
	return RPMRC_OK;

#if defined(_PC_ACL_EXTENDED)
    if (fpathconf(ifdno, _PC_ACL_EXTENDED) != 1
     || fpathconf(ofdno, _PC_ACL_EXTENDED) != 1)
	return RPMRC_OK;
#endif

    a = acl_get_fd(ifdno);
    if (a == NULL)
	return RPMRC_FAIL;
    count = __acl_entries(a);
    if (count > 0 && count != 3 && acl_set_fd(ofdno, a) < 0)
	return RPMRC_FAIL;
    return RPMRC_OK;
}

rpmRC rpmaclCopyDir(const char * sdn, const char * tdn, mode_t mode)
{
    acl_t (*aclgetf)(const char *, acl_type_t);
    int (*aclsetf)(const char *, acl_type_t, acl_t);
    acl_t a;
    int count;

    if (!(sdn && *sdn && tdn && *tdn))
	return RPMRC_OK;

#if defined(_PC_ACL_EXTENDED)
    if (pathconf(sdn, _PC_ACL_EXTENDED) != 1
     || pathconf(tdn, _PC_ACL_EXTENDED) != 1)
        return RPMRC_OK;
#endif
#if defined(__FreeBSD__)
    /* If the file is a link we will not follow it */
    if (S_ISLNK(mode)) {
	aclgetf = acl_get_link_np;
	aclsetf = acl_set_link_np;
    } else
#endif
    {
	aclgetf = acl_get_file;
	aclsetf = acl_set_file;
    }

    /*
     * Even if there is no ACL_TYPE_DEFAULT entry here, a zero
     * size ACL will be returned. So it is not safe to simply
     * check the pointer to see if the default ACL is present.
     */
    a = aclgetf(sdn, ACL_TYPE_DEFAULT);
    if (a == NULL)
	return RPMRC_FAIL;

    count = __acl_entries(a);
    if (count > 0 && aclsetf(tdn, ACL_TYPE_DEFAULT, a) < 0)
	return RPMRC_FAIL;

    a = aclgetf(sdn, ACL_TYPE_ACCESS);
    if (a == NULL)
	return RPMRC_FAIL;
    if (aclsetf(tdn, ACL_TYPE_ACCESS, a) < 0)
	return RPMRC_FAIL;

    return RPMRC_OK;
}
