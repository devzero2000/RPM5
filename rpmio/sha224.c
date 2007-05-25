/*
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file sha224.c
 * \brief SHA-224 hash function, as specified by NIST DFIPS 180-2.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HASH_m HASH_sha224_m
 */
 
#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "sha224.h"
#include "beecrypt/sha256.h"

#if HAVE_ENDIAN_H && HAVE_ASM_BYTEORDER_H
# include <endian.h>
#endif

#include "beecrypt/endianness.h"

/*!\addtogroup HASH_sha224_m
 * \{
 */

/*@unchecked@*/ /*@observer@*/
static const uint32_t hinit[8] = {
	0xc1059ed8U, 0x367cd507U, 0x3070dd17U, 0xf70e5939U, 0xffc00b31U, 0x68581511U, 0x64f98fa7U, 0xbefa4fa4U

};

/*@-sizeoftype@*/
/*@unchecked@*/ /*@observer@*/
const hashFunction sha224 = { "SHA-224", sizeof(sha224Param), 64, 28, (hashFunctionReset) sha224Reset, (hashFunctionUpdate) sha224Update, (hashFunctionDigest) sha224Digest };
/*@=sizeoftype@*/

int sha224Reset(register sha224Param* sp)
{
	int rc = sha256Reset((sha256Param*)sp);
/*@-sizeoftype@*/
	memcpy(sp->h, hinit, 8 * sizeof(uint32_t));
/*@=sizeoftype@*/
	return rc;
}

int sha224Update(register sha224Param* sp, const byte* data, size_t size)
{
	return sha256Update((sha256Param*)sp, data, size);
}

/*@-protoparammatch@*/
int sha224Digest(register sha224Param* sp, byte* data)
{
	byte _sha256digest[32];
	int rc;

	_sha256digest[0] = 0;
	rc = sha256Digest((sha256Param*)sp, _sha256digest);
	if (rc == 0)
	    memcpy(data, _sha256digest, 32-4);
/*@-sizeoftype@*/
	memcpy(sp->h, hinit, 8 * sizeof(uint32_t));
/*@=sizeoftype@*/
	return rc;
}
/*@=protoparammatch@*/

/*!\}
 */
