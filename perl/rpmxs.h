/* common function for perl module */

#ifndef _HAVE_RPMXS_H_
#define _HAVE_RPMXS_H_

/*@observer@*/ /*@unchecked@*/
static unsigned char header_magic[8] = {
        0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/*@observer@*/ /*@unchecked@*/
static unsigned char sigh_magic[8] = {
        0x8e, 0xad, 0xe8, 0x3e, 0x00, 0x00, 0x00, 0x00
};

/*@observer@*/ /*@unchecked@*/
static unsigned char meta_magic[8] = {
        0x8e, 0xad, 0xe8, 0x3f, 0x00, 0x00, 0x00, 0x00
};

void _rpm2header(rpmts ts, char * filename, int checkmode);

int sv2constant(SV * svconstant, const char * context);

#endif
