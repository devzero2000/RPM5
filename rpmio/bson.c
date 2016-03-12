/* bson.c */

/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"

#include <stdarg.h>
#include <stddef.h>
#include <math.h>

#ifdef __APPLE__
# include <mach/clock.h>
# include <mach/mach.h>
# include <mach/mach_time.h>
#endif

#if defined(__linux__)
#include <sys/syscall.h>
#endif

#include <yajl.h>
#include <bson.h>

#include "debug.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

/*==============================================================*/
/* --- b64_ntop.h */

/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#define Assert(Cond) if (!(Cond)) abort ()

static const char Base64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
 * The following encoding technique is taken from RFC 1521 by Borenstein
 * and Freed.  It is reproduced here in a slightly edited form for
 * convenience.
 *
 * A 65-character subset of US-ASCII is used, enabling 6 bits to be
 * represented per printable character. (The extra 65th character, "=",
 * is used to signify a special processing function.)
 *
 * The encoding process represents 24-bit groups of input bits as output
 * strings of 4 encoded characters. Proceeding from left to right, a
 * 24-bit input group is formed by concatenating 3 8-bit input groups.
 * These 24 bits are then treated as 4 concatenated 6-bit groups, each
 * of which is translated into a single digit in the base64 alphabet.
 *
 * Each 6-bit group is used as an index into an array of 64 printable
 * characters. The character referenced by the index is placed in the
 * output string.
 *
 *                       Table 1: The Base64 Alphabet
 *
 *    Value Encoding  Value Encoding  Value Encoding  Value Encoding
 *        0 A            17 R            34 i            51 z
 *        1 B            18 S            35 j            52 0
 *        2 C            19 T            36 k            53 1
 *        3 D            20 U            37 l            54 2
 *        4 E            21 V            38 m            55 3
 *        5 F            22 W            39 n            56 4
 *        6 G            23 X            40 o            57 5
 *        7 H            24 Y            41 p            58 6
 *        8 I            25 Z            42 q            59 7
 *        9 J            26 a            43 r            60 8
 *       10 K            27 b            44 s            61 9
 *       11 L            28 c            45 t            62 +
 *       12 M            29 d            46 u            63 /
 *       13 N            30 e            47 v
 *       14 O            31 f            48 w         (pad) =
 *       15 P            32 g            49 x
 *       16 Q            33 h            50 y
 *
 * Special processing is performed if fewer than 24 bits are available
 * at the end of the data being encoded.  A full encoding quantum is
 * always completed at the end of a quantity.  When fewer than 24 input
 * bits are available in an input group, zero bits are added (on the
 * right) to form an integral number of 6-bit groups.  Padding at the
 * end of the data is performed using the '=' character.
 *
 * Since all base64 input is an integral number of octets, only the
 * following cases can arise:
 *
 *     (1) the final quantum of encoding input is an integral
 *         multiple of 24 bits; here, the final unit of encoded
 *    output will be an integral multiple of 4 characters
 *    with no "=" padding,
 *     (2) the final quantum of encoding input is exactly 8 bits;
 *         here, the final unit of encoded output will be two
 *    characters followed by two "=" padding characters, or
 *     (3) the final quantum of encoding input is exactly 16 bits;
 *         here, the final unit of encoded output will be three
 *    characters followed by one "=" padding character.
 */

static ssize_t
b64_ntop (uint8_t const *src,
          size_t         srclength,
          char          *target,
          size_t         targsize)
{
   size_t datalength = 0;
   uint8_t input[3];
   uint8_t output[4];
   size_t i;

   while (2 < srclength) {
      input[0] = *src++;
      input[1] = *src++;
      input[2] = *src++;
      srclength -= 3;

      output[0] = input[0] >> 2;
      output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
      output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
      output[3] = input[2] & 0x3f;
      Assert (output[0] < 64);
      Assert (output[1] < 64);
      Assert (output[2] < 64);
      Assert (output[3] < 64);

      if (datalength + 4 > targsize) {
         return -1;
      }
      target[datalength++] = Base64[output[0]];
      target[datalength++] = Base64[output[1]];
      target[datalength++] = Base64[output[2]];
      target[datalength++] = Base64[output[3]];
   }

   /* Now we worry about padding. */
   if (0 != srclength) {
      /* Get what's left. */
      input[0] = input[1] = input[2] = '\0';

      for (i = 0; i < srclength; i++) {
         input[i] = *src++;
      }
      output[0] = input[0] >> 2;
      output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
      output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
      Assert (output[0] < 64);
      Assert (output[1] < 64);
      Assert (output[2] < 64);

      if (datalength + 4 > targsize) {
         return -1;
      }
      target[datalength++] = Base64[output[0]];
      target[datalength++] = Base64[output[1]];

      if (srclength == 1) {
         target[datalength++] = Pad64;
      } else{
         target[datalength++] = Base64[output[2]];
      }
      target[datalength++] = Pad64;
   }

   if (datalength >= targsize) {
      return -1;
   }
   target[datalength] = '\0'; /* Returned value doesn't count \0. */
   return datalength;
}

/*==============================================================*/
/* --- b64_pton.h */

/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifdef	DYING
#define Assert(Cond) if (!(Cond)) abort()

static const char Base64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';
#endif

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

                         Table 1: The Base64 Alphabet

      Value Encoding  Value Encoding  Value Encoding  Value Encoding
          0 A            17 R            34 i            51 z
          1 B            18 S            35 j            52 0
          2 C            19 T            36 k            53 1
          3 D            20 U            37 l            54 2
          4 E            21 V            38 m            55 3
          5 F            22 W            39 n            56 4
          6 G            23 X            40 o            57 5
          7 H            24 Y            41 p            58 6
          8 I            25 Z            42 q            59 7
          9 J            26 a            43 r            60 8
         10 K            27 b            44 s            61 9
         11 L            28 c            45 t            62 +
         12 M            29 d            46 u            63 /
         13 N            30 e            47 v
         14 O            31 f            48 w         (pad) =
         15 P            32 g            49 x
         16 Q            33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
   following cases can arise:

       (1) the final quantum of encoding input is an integral
           multiple of 24 bits; here, the final unit of encoded
	   output will be an integral multiple of 4 characters
	   with no "=" padding,
       (2) the final quantum of encoding input is exactly 8 bits;
           here, the final unit of encoded output will be two
	   characters followed by two "=" padding characters, or
       (3) the final quantum of encoding input is exactly 16 bits;
           here, the final unit of encoded output will be three
	   characters followed by one "=" padding character.
   */

/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
 */

static int b64rmap_initialized = 0;
static uint8_t b64rmap[256];

static const uint8_t b64rmap_special = 0xf0;
static const uint8_t b64rmap_end = 0xfd;
static const uint8_t b64rmap_space = 0xfe;
static const uint8_t b64rmap_invalid = 0xff;

/**
 * Initializing the reverse map is not thread safe.
 * Which is fine for NSD. For now...
 **/
static void
b64_initialize_rmap ()
{
	int i;
	unsigned char ch;

	/* Null: end of string, stop parsing */
	b64rmap[0] = b64rmap_end;

	for (i = 1; i < 256; ++i) {
		ch = (unsigned char)i;
		/* Whitespaces */
		if (isspace(ch))
			b64rmap[i] = b64rmap_space;
		/* Padding: stop parsing */
		else if (ch == Pad64)
			b64rmap[i] = b64rmap_end;
		/* Non-base64 char */
		else
			b64rmap[i] = b64rmap_invalid;
	}

	/* Fill reverse mapping for base64 chars */
	for (i = 0; Base64[i] != '\0'; ++i)
		b64rmap[(uint8_t)Base64[i]] = i;

	b64rmap_initialized = 1;
}

static int
b64_pton_do(char const *src, uint8_t *target, size_t targsize)
{
	int tarindex, state, ch;
	uint8_t ofs;

	state = 0;
	tarindex = 0;

	while (1)
	{
		ch = *src++;
		ofs = b64rmap[ch];

		if (ofs >= b64rmap_special) {
			/* Ignore whitespaces */
			if (ofs == b64rmap_space)
				continue;
			/* End of base64 characters */
			if (ofs == b64rmap_end)
				break;
			/* A non-base64 character. */
			return (-1);
		}

		switch (state) {
		case 0:
			if ((size_t)tarindex >= targsize)
				return (-1);
			target[tarindex] = ofs << 2;
			state = 1;
			break;
		case 1:
			if ((size_t)tarindex + 1 >= targsize)
				return (-1);
			target[tarindex]   |=  ofs >> 4;
			target[tarindex+1]  = (ofs & 0x0f)
						<< 4 ;
			tarindex++;
			state = 2;
			break;
		case 2:
			if ((size_t)tarindex + 1 >= targsize)
				return (-1);
			target[tarindex]   |=  ofs >> 2;
			target[tarindex+1]  = (ofs & 0x03)
						<< 6;
			tarindex++;
			state = 3;
			break;
		case 3:
			if ((size_t)tarindex >= targsize)
				return (-1);
			target[tarindex] |= ofs;
			tarindex++;
			state = 0;
			break;
		default:
			abort();
		}
	}

	/*
	 * We are done decoding Base-64 chars.  Let's see if we ended
	 * on a byte boundary, and/or with erroneous trailing characters.
	 */

	if (ch == Pad64) {		/* We got a pad char. */
		ch = *src++;		/* Skip it, get next. */
		switch (state) {
		case 0:		/* Invalid = in first position */
		case 1:		/* Invalid = in second position */
			return (-1);

		case 2:		/* Valid, means one byte of info */
			/* Skip any number of spaces. */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (b64rmap[ch] != b64rmap_space)
					break;
			/* Make sure there is another trailing = sign. */
			if (ch != Pad64)
				return (-1);
			ch = *src++;		/* Skip the = */
			/* Fall through to "single trailing =" case. */
			/* FALLTHROUGH */

		case 3:		/* Valid, means two bytes of info */
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (b64rmap[ch] != b64rmap_space)
					return (-1);

			/*
			 * Now make sure for cases 2 and 3 that the "extra"
			 * bits that slopped past the last full byte were
			 * zeros.  If we don't check them, they become a
			 * subliminal channel.
			 */
			if (target[tarindex] != 0)
				return (-1);
		default:
			break;
		}
	} else {
		/*
		 * We ended by seeing the end of the string.  Make sure we
		 * have no partial bytes lying around.
		 */
		if (state != 0)
			return (-1);
	}

	return (tarindex);
}


static BSON_GNUC_PURE int
b64_pton_len(char const *src)
{
	int tarindex, state, ch;
	uint8_t ofs;

	state = 0;
	tarindex = 0;

	while (1)
	{
		ch = *src++;
		ofs = b64rmap[ch];

		if (ofs >= b64rmap_special) {
			/* Ignore whitespaces */
			if (ofs == b64rmap_space)
				continue;
			/* End of base64 characters */
			if (ofs == b64rmap_end)
				break;
			/* A non-base64 character. */
			return (-1);
		}

		switch (state) {
		case 0:
			state = 1;
			break;
		case 1:
			tarindex++;
			state = 2;
			break;
		case 2:
			tarindex++;
			state = 3;
			break;
		case 3:
			tarindex++;
			state = 0;
			break;
		default:
			abort();
		}
	}

	/*
	 * We are done decoding Base-64 chars.  Let's see if we ended
	 * on a byte boundary, and/or with erroneous trailing characters.
	 */

	if (ch == Pad64) {		/* We got a pad char. */
		ch = *src++;		/* Skip it, get next. */
		switch (state) {
		case 0:		/* Invalid = in first position */
		case 1:		/* Invalid = in second position */
			return (-1);

		case 2:		/* Valid, means one byte of info */
			/* Skip any number of spaces. */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (b64rmap[ch] != b64rmap_space)
					break;
			/* Make sure there is another trailing = sign. */
			if (ch != Pad64)
				return (-1);
			ch = *src++;		/* Skip the = */
			/* Fall through to "single trailing =" case. */
			/* FALLTHROUGH */

		case 3:		/* Valid, means two bytes of info */
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (b64rmap[ch] != b64rmap_space)
					return (-1);

		default:
			break;
		}
	} else {
		/*
		 * We ended by seeing the end of the string.  Make sure we
		 * have no partial bytes lying around.
		 */
		if (state != 0)
			return (-1);
	}

	return (tarindex);
}


static int
b64_pton(char const *src, uint8_t *target, size_t targsize)
{
	if (!b64rmap_initialized)
		b64_initialize_rmap ();

	if (target)
		return b64_pton_do (src, target, targsize);
	else
		return b64_pton_len (src);
}

/*==============================================================*/
/* --- bson.c */

#ifndef BSON_MAX_RECURSION
# define BSON_MAX_RECURSION 100
#endif


typedef enum {
   BSON_VALIDATE_PHASE_START,
   BSON_VALIDATE_PHASE_TOP,
   BSON_VALIDATE_PHASE_LF_REF_KEY,
   BSON_VALIDATE_PHASE_LF_REF_UTF8,
   BSON_VALIDATE_PHASE_LF_ID_KEY,
   BSON_VALIDATE_PHASE_LF_DB_KEY,
   BSON_VALIDATE_PHASE_LF_DB_UTF8,
   BSON_VALIDATE_PHASE_NOT_DBREF,
} bson_validate_phase_t;


/*
 * Structures.
 */
typedef struct
{
   bson_validate_flags_t flags;
   ssize_t               err_offset;
   bson_validate_phase_t phase;
} bson_validate_state_t;


typedef struct
{
   uint32_t       count;
   bool           keys;
   uint32_t       depth;
   bson_string_t *str;
} bson_json_state_t;


/*
 * Forward declarations.
 */
static bool _bson_as_json_visit_array    (const bson_iter_t *iter,
                                          const char        *key,
                                          const bson_t      *v_array,
                                          void              *data);
static bool _bson_as_json_visit_document (const bson_iter_t *iter,
                                          const char        *key,
                                          const bson_t      *v_document,
                                          void              *data);


/*
 * Globals.
 */
static const uint8_t gZero;


/*
 *--------------------------------------------------------------------------
 *
 * _bson_impl_inline_grow --
 *
 *       Document growth implementation for documents that currently
 *       contain stack based buffers. The document may be switched to
 *       a malloc based buffer.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_impl_inline_grow (bson_impl_inline_t *impl, /* IN */
                        size_t              size) /* IN */
{
   bson_impl_alloc_t *alloc = (bson_impl_alloc_t *)impl;
   uint8_t *data;
   size_t req;

   if (((size_t)impl->len + size) <= sizeof impl->data) {
      return true;
   }

   req = bson_next_power_of_two (impl->len + size);

   if (req <= INT32_MAX) {
      data = bson_malloc (req);

      memcpy (data, impl->data, impl->len);
      alloc->flags &= ~BSON_FLAG_INLINE;
      alloc->parent = NULL;
      alloc->depth = 0;
      alloc->buf = &alloc->alloc;
      alloc->buflen = &alloc->alloclen;
      alloc->offset = 0;
      alloc->alloc = data;
      alloc->alloclen = req;
      alloc->realloc = bson_realloc_ctx;
      alloc->realloc_func_ctx = NULL;

      return true;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_impl_alloc_grow --
 *
 *       Document growth implementation for documents containing malloc
 *       based buffers.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_impl_alloc_grow (bson_impl_alloc_t *impl, /* IN */
                       size_t             size) /* IN */
{
   size_t req;

   /*
    * Determine how many bytes we need for this document in the buffer
    * including necessary trailing bytes for parent documents.
    */
   req = (impl->offset + impl->len + size + impl->depth);

   if (req <= *impl->buflen) {
      return true;
   }

   req = bson_next_power_of_two (req);

   if ((req <= INT32_MAX) && impl->realloc) {
      *impl->buf = impl->realloc (*impl->buf, req, impl->realloc_func_ctx);
      *impl->buflen = req;
      return true;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_grow --
 *
 *       Grows the bson_t structure to be large enough to contain @size
 *       bytes.
 *
 * Returns:
 *       true if successful, false if the size would overflow.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_grow (bson_t   *bson, /* IN */
            uint32_t  size) /* IN */
{
   if ((bson->flags & BSON_FLAG_INLINE)) {
      return _bson_impl_inline_grow ((bson_impl_inline_t *)bson, size);
   }

   return _bson_impl_alloc_grow ((bson_impl_alloc_t *)bson, size);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_data --
 *
 *       A helper function to return the contents of the bson document
 *       taking into account the polymorphic nature of bson_t.
 *
 * Returns:
 *       A buffer which should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE uint8_t *
_bson_data (const bson_t *bson) /* IN */
{
   if ((bson->flags & BSON_FLAG_INLINE)) {
      return ((bson_impl_inline_t *)bson)->data;
   } else {
      bson_impl_alloc_t *impl = (bson_impl_alloc_t *)bson;
      return (*impl->buf) + impl->offset;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_encode_length --
 *
 *       Helper to encode the length of the bson_t in the first 4 bytes
 *       of the bson document. Little endian format is used as specified
 *       by bsonspec.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_bson_encode_length (bson_t *bson) /* IN */
{
#if BSON_BYTE_ORDER == BSON_LITTLE_ENDIAN
   memcpy (_bson_data (bson), &bson->len, sizeof (bson->len));
#else
   uint32_t length_le = BSON_UINT32_TO_LE (bson->len);
   memcpy (_bson_data (bson), &length_le, sizeof (length_le));
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_append_va --
 *
 *       Appends the length,buffer pairs to the bson_t. @n_bytes is an
 *       optimization to perform one array growth rather than many small
 *       growths.
 *
 *       @bson: A bson_t
 *       @n_bytes: The number of bytes to append to the document.
 *       @n_pairs: The number of length,buffer pairs.
 *       @first_len: Length of first buffer.
 *       @first_data: First buffer.
 *       @args: va_list of additional tuples.
 *
 * Returns:
 *       true if the bytes were appended successfully.
 *       false if it bson would overflow INT_MAX.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE bool
_bson_append_va (bson_t        *bson,        /* IN */
                 uint32_t       n_bytes,     /* IN */
                 uint32_t       n_pairs,     /* IN */
                 uint32_t       first_len,   /* IN */
                 const uint8_t *first_data,  /* IN */
                 va_list        args)        /* IN */
{
   const uint8_t *data;
   uint32_t data_len;
   uint8_t *buf;

   BSON_ASSERT (!(bson->flags & BSON_FLAG_IN_CHILD));
   BSON_ASSERT (!(bson->flags & BSON_FLAG_RDONLY));

   if (BSON_UNLIKELY (!_bson_grow (bson, n_bytes))) {
      return false;
   }

   data = first_data;
   data_len = first_len;

   buf = _bson_data (bson) + bson->len - 1;

   do {
      n_pairs--;
      memcpy (buf, data, data_len);
      bson->len += data_len;
      buf += data_len;

      if (n_pairs) {
         data_len = va_arg (args, uint32_t);
         data = va_arg (args, const uint8_t *);
      }
   } while (n_pairs);

   _bson_encode_length (bson);

   *buf = '\0';

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_append --
 *
 *       Variadic function to append length,buffer pairs to a bson_t. If the
 *       append would cause the bson_t to overflow a 32-bit length, it will
 *       return false and no append will have occurred.
 *
 * Parameters:
 *       @bson: A bson_t.
 *       @n_pairs: Number of length,buffer pairs.
 *       @n_bytes: the total number of bytes being appended.
 *       @first_len: Length of first buffer.
 *       @first_data: First buffer.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_append (bson_t        *bson,        /* IN */
              uint32_t       n_pairs,     /* IN */
              uint32_t       n_bytes,     /* IN */
              uint32_t       first_len,   /* IN */
              const uint8_t *first_data,  /* IN */
              ...)
{
   va_list args;
   bool ok;

   BSON_ASSERT (n_pairs);
   BSON_ASSERT (first_len);
   BSON_ASSERT (first_data);

   /*
    * Check to see if this append would overflow 32-bit signed integer. I know
    * what you're thinking. BSON uses a signed 32-bit length field? Yeah. It
    * does.
    */
   if (BSON_UNLIKELY (n_bytes > (BSON_MAX_SIZE - bson->len))) {
      return false;
   }

   va_start (args, first_data);
   ok = _bson_append_va (bson, n_bytes, n_pairs, first_len, first_data, args);
   va_end (args);

   return ok;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_append_bson_begin --
 *
 *       Begin appending a subdocument or subarray to the document using
 *       the key provided by @key.
 *
 *       If @key_length is < 0, then strlen() will be called on @key
 *       to determine the length.
 *
 *       @key_type MUST be either BSON_TYPE_DOCUMENT or BSON_TYPE_ARRAY.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       @child is initialized if true is returned.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_append_bson_begin (bson_t      *bson,        /* IN */
                         const char  *key,         /* IN */
                         int          key_length,  /* IN */
                         bson_type_t  child_type,  /* IN */
                         bson_t      *child)       /* OUT */
{
   const uint8_t type = child_type;
   const uint8_t empty[5] = { 5 };
   bson_impl_alloc_t *aparent = (bson_impl_alloc_t *)bson;
   bson_impl_alloc_t *achild = (bson_impl_alloc_t *)child;

   BSON_ASSERT (!(bson->flags & BSON_FLAG_RDONLY));
   BSON_ASSERT (!(bson->flags & BSON_FLAG_IN_CHILD));
   BSON_ASSERT (key);
   BSON_ASSERT ((child_type == BSON_TYPE_DOCUMENT) ||
                (child_type == BSON_TYPE_ARRAY));
   BSON_ASSERT (child);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   /*
    * If the parent is an inline bson_t, then we need to convert
    * it to a heap allocated buffer. This makes extending buffers
    * of child bson documents much simpler logic, as they can just
    * realloc the *buf pointer.
    */
   if ((bson->flags & BSON_FLAG_INLINE)) {
      BSON_ASSERT (bson->len <= 120);
      if (!_bson_grow (bson, 128 - bson->len)) {
         return false;
      }
      BSON_ASSERT (!(bson->flags & BSON_FLAG_INLINE));
   }

   /*
    * Append the type and key for the field.
    */
   if (!_bson_append (bson, 4,
                      (1 + key_length + 1 + 5),
                      1, &type,
                      key_length, key,
                      1, &gZero,
                      5, empty)) {
      return false;
   }

   /*
    * Mark the document as working on a child document so that no
    * further modifications can happen until the caller has called
    * bson_append_{document,array}_end().
    */
   bson->flags |= BSON_FLAG_IN_CHILD;

   /*
    * Initialize the child bson_t structure and point it at the parents
    * buffers. This allows us to realloc directly from the child without
    * walking up to the parent bson_t.
    */
   achild->flags = (BSON_FLAG_CHILD | BSON_FLAG_NO_FREE | BSON_FLAG_STATIC);

   if ((bson->flags & BSON_FLAG_CHILD)) {
      achild->depth = ((bson_impl_alloc_t *)bson)->depth + 1;
   } else {
      achild->depth = 1;
   }

   achild->parent = bson;
   achild->buf = aparent->buf;
   achild->buflen = aparent->buflen;
   achild->offset = aparent->offset + aparent->len - 1 - 5;
   achild->len = 5;
   achild->alloc = NULL;
   achild->alloclen = 0;
   achild->realloc = aparent->realloc;
   achild->realloc_func_ctx = aparent->realloc_func_ctx;

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_append_bson_end --
 *
 *       Complete a call to _bson_append_bson_begin.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       @child is destroyed and no longer valid after calling this
 *       function.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_append_bson_end (bson_t *bson,   /* IN */
                       bson_t *child)  /* IN */
{
   BSON_ASSERT (bson);
   BSON_ASSERT ((bson->flags & BSON_FLAG_IN_CHILD));
   BSON_ASSERT (!(child->flags & BSON_FLAG_IN_CHILD));

   /*
    * Unmark the IN_CHILD flag.
    */
   bson->flags &= ~BSON_FLAG_IN_CHILD;

   /*
    * Now that we are done building the sub-document, add the size to the
    * parent, not including the default 5 byte empty document already added.
    */
   bson->len = (bson->len + child->len - 5);

   /*
    * Ensure we have a \0 byte at the end and proper length encoded at
    * the beginning of the document.
    */
   _bson_data (bson)[bson->len - 1] = '\0';
   _bson_encode_length (bson);

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_array_begin --
 *
 *       Start appending a new array.
 *
 *       Use @child to append to the data area for the given field.
 *
 *       It is a programming error to call any other bson function on
 *       @bson until bson_append_array_end() has been called. It is
 *       valid to call bson_append*() functions on @child.
 *
 *       This function is useful to allow building nested documents using
 *       a single buffer owned by the top-level bson document.
 *
 * Returns:
 *       true if successful; otherwise false and @child is invalid.
 *
 * Side effects:
 *       @child is initialized if true is returned.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_array_begin (bson_t     *bson,         /* IN */
                         const char *key,          /* IN */
                         int         key_length,   /* IN */
                         bson_t     *child)        /* IN */
{
   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (child);

   return _bson_append_bson_begin (bson, key, key_length, BSON_TYPE_ARRAY,
                                   child);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_array_end --
 *
 *       Complete a call to bson_append_array_begin().
 *
 *       It is safe to append other fields to @bson after calling this
 *       function.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       @child is invalid after calling this function.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_array_end (bson_t *bson,   /* IN */
                       bson_t *child)  /* IN */
{
   BSON_ASSERT (bson);
   BSON_ASSERT (child);

   return _bson_append_bson_end (bson, child);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_document_begin --
 *
 *       Start appending a new document.
 *
 *       Use @child to append to the data area for the given field.
 *
 *       It is a programming error to call any other bson function on
 *       @bson until bson_append_document_end() has been called. It is
 *       valid to call bson_append*() functions on @child.
 *
 *       This function is useful to allow building nested documents using
 *       a single buffer owned by the top-level bson document.
 *
 * Returns:
 *       true if successful; otherwise false and @child is invalid.
 *
 * Side effects:
 *       @child is initialized if true is returned.
 *
 *--------------------------------------------------------------------------
 */
bool
bson_append_document_begin (bson_t     *bson,         /* IN */
                            const char *key,          /* IN */
                            int         key_length,   /* IN */
                            bson_t     *child)        /* IN */
{
   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (child);

   return _bson_append_bson_begin (bson, key, key_length, BSON_TYPE_DOCUMENT,
                                   child);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_document_end --
 *
 *       Complete a call to bson_append_document_begin().
 *
 *       It is safe to append new fields to @bson after calling this
 *       function, if true is returned.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       @child is destroyed and invalid after calling this function.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_document_end (bson_t *bson,   /* IN */
                          bson_t *child)  /* IN */
{
   BSON_ASSERT (bson);
   BSON_ASSERT (child);

   return _bson_append_bson_end (bson, child);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_array --
 *
 *       Append an array to @bson.
 *
 *       Generally, bson_append_array_begin() will result in faster code
 *       since few buffers need to be malloced.
 *
 * Returns:
 *       true if successful; otherwise false indicating INT_MAX overflow.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_array (bson_t       *bson,       /* IN */
                   const char   *key,        /* IN */
                   int           key_length, /* IN */
                   const bson_t *array)      /* IN */
{
   static const uint8_t type = BSON_TYPE_ARRAY;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (array);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   /*
    * Let's be a bit pedantic and ensure the array has properly formatted key
    * names.  We will verify this simply by checking the first element for "0"
    * if the array is non-empty.
    */
   if (array && !bson_empty (array)) {
      bson_iter_t iter;

      if (bson_iter_init (&iter, array) && bson_iter_next (&iter)) {
         if (0 != strcmp ("0", bson_iter_key (&iter))) {
            fprintf (stderr,
                     "%s(): invalid array detected. first element of array "
                     "parameter is not \"0\".\n",
                     BSON_FUNC);
         }
      }
   }

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + array->len),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        array->len, _bson_data (array));
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_binary --
 *
 *       Append binary data to @bson. The field will have the
 *       BSON_TYPE_BINARY type.
 *
 * Parameters:
 *       @subtype: the BSON Binary Subtype. See bsonspec.org for more
 *                 information.
 *       @binary: a pointer to the raw binary data.
 *       @length: the size of @binary in bytes.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_binary (bson_t         *bson,       /* IN */
                    const char     *key,        /* IN */
                    int             key_length, /* IN */
                    bson_subtype_t  subtype,    /* IN */
                    const uint8_t  *binary,     /* IN */
                    uint32_t        length)     /* IN */
{
   static const uint8_t type = BSON_TYPE_BINARY;
   uint32_t length_le;
   uint32_t deprecated_length_le;
   uint8_t subtype8 = 0;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (binary);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   subtype8 = subtype;

   if (subtype == BSON_SUBTYPE_BINARY_DEPRECATED) {
      length_le = BSON_UINT32_TO_LE (length + 4);
      deprecated_length_le = BSON_UINT32_TO_LE (length);

      return _bson_append (bson, 7,
                           (1 + key_length + 1 + 4 + 1 + 4 + length),
                           1, &type,
                           key_length, key,
                           1, &gZero,
                           4, &length_le,
                           1, &subtype8,
                           4, &deprecated_length_le,
                           length, binary);
   } else {
      length_le = BSON_UINT32_TO_LE (length);

      return _bson_append (bson, 6,
                           (1 + key_length + 1 + 4 + 1 + length),
                           1, &type,
                           key_length, key,
                           1, &gZero,
                           4, &length_le,
                           1, &subtype8,
                           length, binary);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_bool --
 *
 *       Append a new field to @bson with the name @key. The value is
 *       a boolean indicated by @value.
 *
 * Returns:
 *       true if succesful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_bool (bson_t     *bson,       /* IN */
                  const char *key,        /* IN */
                  int         key_length, /* IN */
                  bool        value)      /* IN */
{
   static const uint8_t type = BSON_TYPE_BOOL;
   uint8_t abyte = !!value;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        1, &abyte);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_code --
 *
 *       Append a new field to @bson containing javascript code.
 *
 *       @javascript MUST be a zero terminated UTF-8 string. It MUST NOT
 *       containing embedded \0 characters.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 * See also:
 *       bson_append_code_with_scope().
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_code (bson_t     *bson,       /* IN */
                  const char *key,        /* IN */
                  int         key_length, /* IN */
                  const char *javascript) /* IN */
{
   static const uint8_t type = BSON_TYPE_CODE;
   uint32_t length;
   uint32_t length_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (javascript);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   length = (int)strlen (javascript) + 1;
   length_le = BSON_UINT32_TO_LE (length);

   return _bson_append (bson, 5,
                        (1 + key_length + 1 + 4 + length),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        4, &length_le,
                        length, javascript);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_code_with_scope --
 *
 *       Append a new field to @bson containing javascript code with
 *       supplied scope.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_code_with_scope (bson_t       *bson,         /* IN */
                             const char   *key,          /* IN */
                             int           key_length,   /* IN */
                             const char   *javascript,   /* IN */
                             const bson_t *scope)        /* IN */
{
   static const uint8_t type = BSON_TYPE_CODEWSCOPE;
   uint32_t codews_length_le;
   uint32_t codews_length;
   uint32_t js_length_le;
   uint32_t js_length;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (javascript);

   if (bson_empty0 (scope)) {
      return bson_append_code (bson, key, key_length, javascript);
   }

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   js_length = (int)strlen (javascript) + 1;
   js_length_le = BSON_UINT32_TO_LE (js_length);

   codews_length = 4 + 4 + js_length + scope->len;
   codews_length_le = BSON_UINT32_TO_LE (codews_length);

   return _bson_append (bson, 7,
                        (1 + key_length + 1 + 4 + 4 + js_length + scope->len),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        4, &codews_length_le,
                        4, &js_length_le,
                        js_length, javascript,
                        scope->len, _bson_data (scope));
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_dbpointer --
 *
 *       This BSON data type is DEPRECATED.
 *
 *       Append a BSON dbpointer field to @bson.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_dbpointer (bson_t           *bson,       /* IN */
                       const char       *key,        /* IN */
                       int               key_length, /* IN */
                       const char       *collection, /* IN */
                       const bson_oid_t *oid)
{
   static const uint8_t type = BSON_TYPE_DBPOINTER;
   uint32_t length;
   uint32_t length_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (collection);
   BSON_ASSERT (oid);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   length = (int)strlen (collection) + 1;
   length_le = BSON_UINT32_TO_LE (length);

   return _bson_append (bson, 6,
                        (1 + key_length + 1 + 4 + length + 12),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        4, &length_le,
                        length, collection,
                        12, oid);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_append_document --
 *
 *       Append a new field to @bson containing a BSON document.
 *
 *       In general, using bson_append_document_begin() results in faster
 *       code and less memory fragmentation.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 * See also:
 *       bson_append_document_begin().
 *
 *--------------------------------------------------------------------------
 */

bool
bson_append_document (bson_t       *bson,       /* IN */
                      const char   *key,        /* IN */
                      int           key_length, /* IN */
                      const bson_t *value)      /* IN */
{
   static const uint8_t type = BSON_TYPE_DOCUMENT;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (value);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + value->len),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        value->len, _bson_data (value));
}


bool
bson_append_double (bson_t     *bson,
                    const char *key,
                    int         key_length,
                    double      value)
{
   static const uint8_t type = BSON_TYPE_DOUBLE;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN
   value = BSON_DOUBLE_TO_LE (value);
#endif

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 8),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        8, &value);
}


bool
bson_append_int32 (bson_t      *bson,
                   const char  *key,
                   int          key_length,
                   int32_t value)
{
   static const uint8_t type = BSON_TYPE_INT32;
   uint32_t value_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   value_le = BSON_UINT32_TO_LE (value);

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 4),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        4, &value_le);
}


bool
bson_append_int64 (bson_t      *bson,
                   const char  *key,
                   int          key_length,
                   int64_t value)
{
   static const uint8_t type = BSON_TYPE_INT64;
   uint64_t value_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   value_le = BSON_UINT64_TO_LE (value);

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 8),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        8, &value_le);
}


bool
bson_append_iter (bson_t            *bson,
                  const char        *key,
                  int                key_length,
                  const bson_iter_t *iter)
{
   bool ret = false;

   BSON_ASSERT (bson);
   BSON_ASSERT (iter);

   if (!key) {
      key = bson_iter_key (iter);
      key_length = -1;
   }

   switch (bson_iter_type_unsafe (iter)) {
   case BSON_TYPE_EOD:
      return false;
   case BSON_TYPE_DOUBLE:
      ret = bson_append_double (bson, key, key_length, bson_iter_double (iter));
      break;
   case BSON_TYPE_UTF8:
      {
         uint32_t len = 0;
         const char *str;

         str = bson_iter_utf8 (iter, &len);
         ret = bson_append_utf8 (bson, key, key_length, str, len);
      }
      break;
   case BSON_TYPE_DOCUMENT:
      {
         const uint8_t *buf = NULL;
         uint32_t len = 0;
         bson_t doc;

         bson_iter_document (iter, &len, &buf);

         if (bson_init_static (&doc, buf, len)) {
            ret = bson_append_document (bson, key, key_length, &doc);
            bson_destroy (&doc);
         }
      }
      break;
   case BSON_TYPE_ARRAY:
      {
         const uint8_t *buf = NULL;
         uint32_t len = 0;
         bson_t doc;

         bson_iter_array (iter, &len, &buf);

         if (bson_init_static (&doc, buf, len)) {
            ret = bson_append_array (bson, key, key_length, &doc);
            bson_destroy (&doc);
         }
      }
      break;
   case BSON_TYPE_BINARY:
      {
         const uint8_t *binary = NULL;
         bson_subtype_t subtype = BSON_SUBTYPE_BINARY;
         uint32_t len = 0;

         bson_iter_binary (iter, &subtype, &len, &binary);
         ret = bson_append_binary (bson, key, key_length,
                                   subtype, binary, len);
      }
      break;
   case BSON_TYPE_UNDEFINED:
      ret = bson_append_undefined (bson, key, key_length);
      break;
   case BSON_TYPE_OID:
      ret = bson_append_oid (bson, key, key_length, bson_iter_oid (iter));
      break;
   case BSON_TYPE_BOOL:
      ret = bson_append_bool (bson, key, key_length, bson_iter_bool (iter));
      break;
   case BSON_TYPE_DATE_TIME:
      ret = bson_append_date_time (bson, key, key_length,
                                   bson_iter_date_time (iter));
      break;
   case BSON_TYPE_NULL:
      ret = bson_append_null (bson, key, key_length);
      break;
   case BSON_TYPE_REGEX:
      {
         const char *regex;
         const char *options;

         regex = bson_iter_regex (iter, &options);
         ret = bson_append_regex (bson, key, key_length, regex, options);
      }
      break;
   case BSON_TYPE_DBPOINTER:
      {
         const bson_oid_t *oid;
         uint32_t len;
         const char *collection;

         bson_iter_dbpointer (iter, &len, &collection, &oid);
         ret = bson_append_dbpointer (bson, key, key_length, collection, oid);
      }
      break;
   case BSON_TYPE_CODE:
      {
         uint32_t len;
         const char *code;

         code = bson_iter_code (iter, &len);
         ret = bson_append_code (bson, key, key_length, code);
      }
      break;
   case BSON_TYPE_SYMBOL:
      {
         uint32_t len;
         const char *symbol;

         symbol = bson_iter_symbol (iter, &len);
         ret = bson_append_symbol (bson, key, key_length, symbol, len);
      }
      break;
   case BSON_TYPE_CODEWSCOPE:
      {
         const uint8_t *scope = NULL;
         uint32_t scope_len = 0;
         uint32_t len = 0;
         const char *javascript = NULL;
         bson_t doc;

         javascript = bson_iter_codewscope (iter, &len, &scope_len, &scope);

         if (bson_init_static (&doc, scope, scope_len)) {
            ret = bson_append_code_with_scope (bson, key, key_length,
                                               javascript, &doc);
            bson_destroy (&doc);
         }
      }
      break;
   case BSON_TYPE_INT32:
      ret = bson_append_int32 (bson, key, key_length, bson_iter_int32 (iter));
      break;
   case BSON_TYPE_TIMESTAMP:
      {
         uint32_t ts;
         uint32_t inc;

         bson_iter_timestamp (iter, &ts, &inc);
         ret = bson_append_timestamp (bson, key, key_length, ts, inc);
      }
      break;
   case BSON_TYPE_INT64:
      ret = bson_append_int64 (bson, key, key_length, bson_iter_int64 (iter));
      break;
   case BSON_TYPE_MAXKEY:
      ret = bson_append_maxkey (bson, key, key_length);
      break;
   case BSON_TYPE_MINKEY:
      ret = bson_append_minkey (bson, key, key_length);
      break;
   default:
      break;
   }

   return ret;
}


bool
bson_append_maxkey (bson_t     *bson,
                    const char *key,
                    int         key_length)
{
   static const uint8_t type = BSON_TYPE_MAXKEY;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   return _bson_append (bson, 3,
                        (1 + key_length + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero);
}


bool
bson_append_minkey (bson_t     *bson,
                    const char *key,
                    int         key_length)
{
   static const uint8_t type = BSON_TYPE_MINKEY;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   return _bson_append (bson, 3,
                        (1 + key_length + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero);
}


bool
bson_append_null (bson_t     *bson,
                  const char *key,
                  int         key_length)
{
   static const uint8_t type = BSON_TYPE_NULL;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   return _bson_append (bson, 3,
                        (1 + key_length + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero);
}


bool
bson_append_oid (bson_t           *bson,
                 const char       *key,
                 int               key_length,
                 const bson_oid_t *value)
{
   static const uint8_t type = BSON_TYPE_OID;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (value);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 12),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        12, value);
}


bool
bson_append_regex (bson_t     *bson,
                   const char *key,
                   int         key_length,
                   const char *regex,
                   const char *options)
{
   static const uint8_t type = BSON_TYPE_REGEX;
   uint32_t regex_len;
   uint32_t options_len;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   if (!regex) {
      regex = "";
   }

   if (!options) {
      options = "";
   }

   regex_len = (int)strlen (regex) + 1;
   options_len = (int)strlen (options) + 1;

   return _bson_append (bson, 5,
                        (1 + key_length + 1 + regex_len + options_len),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        regex_len, regex,
                        options_len, options);
}


bool
bson_append_utf8 (bson_t     *bson,
                  const char *key,
                  int         key_length,
                  const char *value,
                  int         length)
{
   static const uint8_t type = BSON_TYPE_UTF8;
   uint32_t length_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (BSON_UNLIKELY (!value)) {
      return bson_append_null (bson, key, key_length);
   }

   if (BSON_UNLIKELY (key_length < 0)) {
      key_length = (int)strlen (key);
   }

   if (BSON_UNLIKELY (length < 0)) {
      length = (int)strlen (value);
   }

   length_le = BSON_UINT32_TO_LE (length + 1);

   return _bson_append (bson, 6,
                        (1 + key_length + 1 + 4 + length + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        4, &length_le,
                        length, value,
                        1, &gZero);
}


bool
bson_append_symbol (bson_t     *bson,
                    const char *key,
                    int         key_length,
                    const char *value,
                    int         length)
{
   static const uint8_t type = BSON_TYPE_SYMBOL;
   uint32_t length_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (!value) {
      return bson_append_null (bson, key, key_length);
   }

   if (key_length < 0) {
      key_length = (int)strlen (key);
   }

   if (length < 0) {
      length =(int)strlen (value);
   }

   length_le = BSON_UINT32_TO_LE (length + 1);

   return _bson_append (bson, 6,
                        (1 + key_length + 1 + 4 + length + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        4, &length_le,
                        length, value,
                        1, &gZero);
}


bool
bson_append_time_t (bson_t     *bson,
                    const char *key,
                    int         key_length,
                    time_t      value)
{
#ifdef BSON_OS_WIN32
   struct timeval tv = { (long)value, 0 };
#else
   struct timeval tv = { value, 0 };
#endif

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   return bson_append_timeval (bson, key, key_length, &tv);
}


bool
bson_append_timestamp (bson_t       *bson,
                       const char   *key,
                       int           key_length,
                       uint32_t timestamp,
                       uint32_t increment)
{
   static const uint8_t type = BSON_TYPE_TIMESTAMP;
   uint64_t value;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length =(int)strlen (key);
   }

   value = ((((uint64_t)timestamp) << 32) | ((uint64_t)increment));
   value = BSON_UINT64_TO_LE (value);

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 8),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        8, &value);
}


bool
bson_append_now_utc (bson_t     *bson,
                     const char *key,
                     int         key_length)
{
   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (key_length >= -1);

   return bson_append_time_t (bson, key, key_length, time (NULL));
}


bool
bson_append_date_time (bson_t      *bson,
                       const char  *key,
                       int          key_length,
                       int64_t value)
{
   static const uint8_t type = BSON_TYPE_DATE_TIME;
   uint64_t value_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length =(int)strlen (key);
   }

   value_le = BSON_UINT64_TO_LE (value);

   return _bson_append (bson, 4,
                        (1 + key_length + 1 + 8),
                        1, &type,
                        key_length, key,
                        1, &gZero,
                        8, &value_le);
}


bool
bson_append_timeval (bson_t         *bson,
                     const char     *key,
                     int             key_length,
                     struct timeval *value)
{
   uint64_t unix_msec;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (value);

   unix_msec = (((uint64_t)value->tv_sec) * 1000UL) +
                                  (value->tv_usec / 1000UL);
   return bson_append_date_time (bson, key, key_length, unix_msec);
}


bool
bson_append_undefined (bson_t     *bson,
                       const char *key,
                       int         key_length)
{
   static const uint8_t type = BSON_TYPE_UNDEFINED;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (key_length < 0) {
      key_length =(int)strlen (key);
   }

   return _bson_append (bson, 3,
                        (1 + key_length + 1),
                        1, &type,
                        key_length, key,
                        1, &gZero);
}


bool
bson_append_value (bson_t             *bson,
                   const char         *key,
                   int                 key_length,
                   const bson_value_t *value)
{
   bson_t local;
   bool ret = false;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);
   BSON_ASSERT (value);

   switch (value->value_type) {
   case BSON_TYPE_DOUBLE:
      ret = bson_append_double (bson, key, key_length,
                                value->value.v_double);
      break;
   case BSON_TYPE_UTF8:
      ret = bson_append_utf8 (bson, key, key_length,
                              value->value.v_utf8.str,
                              value->value.v_utf8.len);
      break;
   case BSON_TYPE_DOCUMENT:
      if (bson_init_static (&local,
                            value->value.v_doc.data,
                            value->value.v_doc.data_len)) {
         ret = bson_append_document (bson, key, key_length, &local);
         bson_destroy (&local);
      }
      break;
   case BSON_TYPE_ARRAY:
      if (bson_init_static (&local,
                            value->value.v_doc.data,
                            value->value.v_doc.data_len)) {
         ret = bson_append_array (bson, key, key_length, &local);
         bson_destroy (&local);
      }
      break;
   case BSON_TYPE_BINARY:
      ret = bson_append_binary (bson, key, key_length,
                                value->value.v_binary.subtype,
                                value->value.v_binary.data,
                                value->value.v_binary.data_len);
      break;
   case BSON_TYPE_UNDEFINED:
      ret = bson_append_undefined (bson, key, key_length);
      break;
   case BSON_TYPE_OID:
      ret = bson_append_oid (bson, key, key_length, &value->value.v_oid);
      break;
   case BSON_TYPE_BOOL:
      ret = bson_append_bool (bson, key, key_length, value->value.v_bool);
      break;
   case BSON_TYPE_DATE_TIME:
      ret = bson_append_date_time (bson, key, key_length,
                                   value->value.v_datetime);
      break;
   case BSON_TYPE_NULL:
      ret = bson_append_null (bson, key, key_length);
      break;
   case BSON_TYPE_REGEX:
      ret = bson_append_regex (bson, key, key_length,
                               value->value.v_regex.regex,
                               value->value.v_regex.options);
      break;
   case BSON_TYPE_DBPOINTER:
      ret = bson_append_dbpointer (bson, key, key_length,
                                   value->value.v_dbpointer.collection,
                                   &value->value.v_dbpointer.oid);
      break;
   case BSON_TYPE_CODE:
      ret = bson_append_code (bson, key, key_length,
                              value->value.v_code.code);
      break;
   case BSON_TYPE_SYMBOL:
      ret = bson_append_symbol (bson, key, key_length,
                                value->value.v_symbol.symbol,
                                value->value.v_symbol.len);
      break;
   case BSON_TYPE_CODEWSCOPE:
      if (bson_init_static (&local,
                            value->value.v_codewscope.scope_data,
                            value->value.v_codewscope.scope_len)) {
         ret = bson_append_code_with_scope (bson, key, key_length,
                                            value->value.v_codewscope.code,
                                            &local);
         bson_destroy (&local);
      }
      break;
   case BSON_TYPE_INT32:
      ret = bson_append_int32 (bson, key, key_length, value->value.v_int32);
      break;
   case BSON_TYPE_TIMESTAMP:
      ret = bson_append_timestamp (bson, key, key_length,
                                   value->value.v_timestamp.timestamp,
                                   value->value.v_timestamp.increment);
      break;
   case BSON_TYPE_INT64:
      ret = bson_append_int64 (bson, key, key_length, value->value.v_int64);
      break;
   case BSON_TYPE_MAXKEY:
      ret = bson_append_maxkey (bson, key, key_length);
      break;
   case BSON_TYPE_MINKEY:
      ret = bson_append_minkey (bson, key, key_length);
      break;
   case BSON_TYPE_EOD:
   default:
      break;
   }

   return ret;
}


void
bson_init (bson_t *bson)
{
   bson_impl_inline_t *impl = (bson_impl_inline_t *)bson;

   BSON_ASSERT (bson);

   impl->flags = BSON_FLAG_INLINE | BSON_FLAG_STATIC;
   impl->len = 5;
   impl->data[0] = 5;
   impl->data[1] = 0;
   impl->data[2] = 0;
   impl->data[3] = 0;
   impl->data[4] = 0;
}


void
bson_reinit (bson_t *bson)
{
   uint8_t *data;

   BSON_ASSERT (bson);

   data = _bson_data (bson);

   bson->len = 5;

   data [0] = 5;
   data [1] = 0;
   data [2] = 0;
   data [3] = 0;
   data [4] = 0;
}


bool
bson_init_static (bson_t        *bson,
                  const uint8_t *data,
                  size_t         length)
{
   bson_impl_alloc_t *impl = (bson_impl_alloc_t *)bson;
   uint32_t len_le;

   BSON_ASSERT (bson);
   BSON_ASSERT (data);

   if ((length < 5) || (length > INT_MAX)) {
      return false;
   }

   memcpy (&len_le, data, sizeof (len_le));

   if ((size_t)BSON_UINT32_FROM_LE (len_le) != length) {
      return false;
   }

   if (data[length - 1]) {
      return false;
   }

   impl->flags = BSON_FLAG_STATIC | BSON_FLAG_RDONLY;
   impl->len = (uint32_t)length;
   impl->parent = NULL;
   impl->depth = 0;
   impl->buf = &impl->alloc;
   impl->buflen = &impl->alloclen;
   impl->offset = 0;
   impl->alloc = (uint8_t *)data;
   impl->alloclen = length;
   impl->realloc = NULL;
   impl->realloc_func_ctx = NULL;

   return true;
}


bson_t *
bson_new (void)
{
   bson_impl_inline_t *impl;
   bson_t *bson;

   bson = bson_malloc (sizeof *bson);

   impl = (bson_impl_inline_t *)bson;
   impl->flags = BSON_FLAG_INLINE;
   impl->len = 5;
   impl->data[0] = 5;
   impl->data[1] = 0;
   impl->data[2] = 0;
   impl->data[3] = 0;
   impl->data[4] = 0;

   return bson;
}


bson_t *
bson_sized_new (size_t size)
{
   bson_impl_alloc_t *impl_a;
   bson_impl_inline_t *impl_i;
   bson_t *b;

   BSON_ASSERT (size <= INT32_MAX);

   b = bson_malloc (sizeof *b);
   impl_a = (bson_impl_alloc_t *)b;
   impl_i = (bson_impl_inline_t *)b;

   if (size <= sizeof impl_i->data) {
      bson_init (b);
      b->flags &= ~BSON_FLAG_STATIC;
   } else {
      impl_a->flags = BSON_FLAG_NONE;
      impl_a->len = 5;
      impl_a->parent = NULL;
      impl_a->depth = 0;
      impl_a->buf = &impl_a->alloc;
      impl_a->buflen = &impl_a->alloclen;
      impl_a->offset = 0;
      impl_a->alloclen = BSON_MAX (5, size);
      impl_a->alloc = bson_malloc (impl_a->alloclen);
      impl_a->alloc[0] = 5;
      impl_a->alloc[1] = 0;
      impl_a->alloc[2] = 0;
      impl_a->alloc[3] = 0;
      impl_a->alloc[4] = 0;
      impl_a->realloc = bson_realloc_ctx;
      impl_a->realloc_func_ctx = NULL;
   }

   return b;
}


bson_t *
bson_new_from_data (const uint8_t *data,
                    size_t         length)
{
   uint32_t len_le;
   bson_t *bson;

   BSON_ASSERT (data);

   if ((length < 5) || (length > INT_MAX) || data [length - 1]) {
      return NULL;
   }

   memcpy (&len_le, data, sizeof (len_le));

   if (length != (size_t)BSON_UINT32_FROM_LE (len_le)) {
      return NULL;
   }

   bson = bson_sized_new (length);
   memcpy (_bson_data (bson), data, length);
   bson->len = (uint32_t)length;

   return bson;
}


bson_t *
bson_new_from_buffer (uint8_t           **buf,
                      size_t             *buf_len,
                      bson_realloc_func   realloc_func,
                      void               *realloc_func_ctx)
{
   bson_impl_alloc_t *impl;
   uint32_t len_le;
   uint32_t length;
   bson_t *bson;

   BSON_ASSERT (buf);
   BSON_ASSERT (buf_len);

   if (!realloc_func) {
      realloc_func = bson_realloc_ctx;
   }

   bson = bson_malloc0 (sizeof *bson);
   impl = (bson_impl_alloc_t *)bson;

   if (!*buf) {
      length = 5;
      len_le = BSON_UINT32_TO_LE (length);
      *buf_len = 5;
      *buf = realloc_func (*buf, *buf_len, realloc_func_ctx);
      memcpy (*buf, &len_le, sizeof (len_le));
      (*buf) [4] = '\0';
   } else {
      if ((*buf_len < 5) || (*buf_len > INT_MAX)) {
         bson_free (bson);
         return NULL;
      }

      memcpy (&len_le, *buf, sizeof (len_le));
      length = BSON_UINT32_FROM_LE(len_le);
   }

   if ((*buf)[length - 1]) {
      bson_free (bson);
      return NULL;
   }

   impl->flags = BSON_FLAG_NO_FREE;
   impl->len = length;
   impl->buf = buf;
   impl->buflen = buf_len;
   impl->realloc = realloc_func;
   impl->realloc_func_ctx = realloc_func_ctx;

   return bson;
}


bson_t *
bson_copy (const bson_t *bson)
{
   const uint8_t *data;

   BSON_ASSERT (bson);

   data = _bson_data (bson);
   return bson_new_from_data (data, bson->len);
}


void
bson_copy_to (const bson_t *src,
              bson_t       *dst)
{
   const uint8_t *data;
   bson_impl_alloc_t *adst;
   size_t len;

   BSON_ASSERT (src);
   BSON_ASSERT (dst);

   if ((src->flags & BSON_FLAG_INLINE)) {
      memcpy (dst, src, sizeof *dst);
      dst->flags = (BSON_FLAG_STATIC | BSON_FLAG_INLINE);
      return;
   }

   data = _bson_data (src);
   len = bson_next_power_of_two ((size_t)src->len);

   adst = (bson_impl_alloc_t *)dst;
   adst->flags = BSON_FLAG_STATIC;
   adst->len = src->len;
   adst->parent = NULL;
   adst->depth = 0;
   adst->buf = &adst->alloc;
   adst->buflen = &adst->alloclen;
   adst->offset = 0;
   adst->alloc = bson_malloc (len);
   adst->alloclen = len;
   adst->realloc = bson_realloc_ctx;
   adst->realloc_func_ctx = NULL;
   memcpy (adst->alloc, data, src->len);
}


static bool
should_ignore (const char *first_exclude,
               va_list     args,
               const char *name)
{
   bool ret = false;
   const char *exclude = first_exclude;
   va_list args_copy;

   va_copy (args_copy, args);

   do {
      if (!strcmp (name, exclude)) {
         ret = true;
         break;
      }
   } while ((exclude = va_arg (args_copy, const char *)));

   va_end (args_copy);

   return ret;
}


static void
_bson_copy_to_excluding_va (const bson_t *src,
                            bson_t       *dst,
                            const char   *first_exclude,
                            va_list       args)
{
   bson_iter_t iter;

   if (bson_iter_init (&iter, src)) {
      while (bson_iter_next (&iter)) {
         if (!should_ignore (first_exclude, args, bson_iter_key (&iter))) {
            if (!bson_append_iter (dst, NULL, 0, &iter)) {
               /*
                * This should not be able to happen since we are copying
                * from within a valid bson_t.
                */
               BSON_ASSERT (false);
               return;
            }
         }
      }
   }
}


void
bson_copy_to_excluding (const bson_t *src,
                        bson_t       *dst,
                        const char   *first_exclude,
                        ...)
{
   va_list args;

   BSON_ASSERT (src);
   BSON_ASSERT (dst);
   BSON_ASSERT (first_exclude);

   bson_init (dst);

   va_start (args, first_exclude);
   _bson_copy_to_excluding_va (src, dst, first_exclude, args);
   va_end (args);
}

void
bson_copy_to_excluding_noinit (const bson_t *src,
                               bson_t       *dst,
                               const char   *first_exclude,
                               ...)
{
    va_list args;

    BSON_ASSERT (src);
    BSON_ASSERT (dst);
    BSON_ASSERT (first_exclude);

    va_start (args, first_exclude);
    _bson_copy_to_excluding_va (src, dst, first_exclude, args);
    va_end (args);
}


void
bson_destroy (bson_t *bson)
{
   BSON_ASSERT (bson);

   if (!(bson->flags &
         (BSON_FLAG_RDONLY | BSON_FLAG_INLINE | BSON_FLAG_NO_FREE))) {
      bson_free (*((bson_impl_alloc_t *)bson)->buf);
   }

   if (!(bson->flags & BSON_FLAG_STATIC)) {
      bson_free (bson);
   }
}


uint8_t *
bson_destroy_with_steal (bson_t   *bson,
                         bool      steal,
                         uint32_t *length)
{
   uint8_t *ret = NULL;

   BSON_ASSERT (bson);

   if (length) {
      *length = bson->len;
   }

   if (!steal) {
      bson_destroy (bson);
      return NULL;
   }

   if ((bson->flags & (BSON_FLAG_CHILD |
                       BSON_FLAG_IN_CHILD |
                       BSON_FLAG_RDONLY))) {
      /* Do nothing */
   } else if ((bson->flags & BSON_FLAG_INLINE)) {
      bson_impl_inline_t *inl;

      inl = (bson_impl_inline_t *)bson;
      ret = bson_malloc (bson->len);
      memcpy (ret, inl->data, bson->len);
   } else {
      bson_impl_alloc_t *alloc;

      alloc = (bson_impl_alloc_t *)bson;
      ret = *alloc->buf;
      *alloc->buf = NULL;
   }

   bson_destroy (bson);

   return ret;
}


const uint8_t *
bson_get_data (const bson_t *bson)
{
   BSON_ASSERT (bson);

   return _bson_data (bson);
}


uint32_t
bson_count_keys (const bson_t *bson)
{
   uint32_t count = 0;
   bson_iter_t iter;

   BSON_ASSERT (bson);

   if (bson_iter_init (&iter, bson)) {
      while (bson_iter_next (&iter)) {
         count++;
      }
   }

   return count;
}


bool
bson_has_field (const bson_t *bson,
                const char   *key)
{
   bson_iter_t iter;
   bson_iter_t child;

   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   if (NULL != strchr (key, '.')) {
      return (bson_iter_init (&iter, bson) &&
              bson_iter_find_descendant (&iter, key, &child));
   }

   return bson_iter_init_find (&iter, bson, key);
}


int
bson_compare (const bson_t *bson,
              const bson_t *other)
{
   const uint8_t *data1;
   const uint8_t *data2;
   size_t len1;
   size_t len2;
   int64_t ret;

   data1 = _bson_data (bson) + 4;
   len1 = bson->len - 4;

   data2 = _bson_data (other) + 4;
   len2 = other->len - 4;

   if (len1 == len2) {
      return memcmp (data1, data2, len1);
   }

   ret = memcmp (data1, data2, BSON_MIN (len1, len2));

   if (ret == 0) {
      ret = (int64_t) (len1 - len2);
   }

   return (ret < 0) ? -1 : (ret > 0);
}


bool
bson_equal (const bson_t *bson,
            const bson_t *other)
{
   return !bson_compare (bson, other);
}


static bool
_bson_as_json_visit_utf8 (const bson_iter_t *iter,
                          const char        *key,
                          size_t             v_utf8_len,
                          const char        *v_utf8,
                          void              *data)
{
   bson_json_state_t *state = data;
   char *escaped;

   escaped = bson_utf8_escape_for_json (v_utf8, v_utf8_len);

   if (escaped) {
      bson_string_append (state->str, "\"");
      bson_string_append (state->str, escaped);
      bson_string_append (state->str, "\"");
      bson_free (escaped);
      return false;
   }

   return true;
}


static bool
_bson_as_json_visit_int32 (const bson_iter_t *iter,
                           const char        *key,
                           int32_t       v_int32,
                           void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append_printf (state->str, "%" PRId32, v_int32);

   return false;
}


static bool
_bson_as_json_visit_int64 (const bson_iter_t *iter,
                           const char        *key,
                           int64_t       v_int64,
                           void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append_printf (state->str, "%" PRId64, v_int64);

   return false;
}


static bool
_bson_as_json_visit_double (const bson_iter_t *iter,
                            const char        *key,
                            double             v_double,
                            void              *data)
{
   bson_json_state_t *state = data;

#ifdef BSON_NEEDS_SET_OUTPUT_FORMAT
   unsigned int current_format = _set_output_format(_TWO_DIGIT_EXPONENT);
#endif

   bson_string_append_printf (state->str, "%.15g", v_double);

#ifdef BSON_NEEDS_SET_OUTPUT_FORMAT
   _set_output_format(current_format);
#endif

   return false;
}


static bool
_bson_as_json_visit_undefined (const bson_iter_t *iter,
                               const char        *key,
                               void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "{ \"$undefined\" : true }");

   return false;
}


static bool
_bson_as_json_visit_null (const bson_iter_t *iter,
                          const char        *key,
                          void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "null");

   return false;
}


static bool
_bson_as_json_visit_oid (const bson_iter_t *iter,
                         const char        *key,
                         const bson_oid_t  *oid,
                         void              *data)
{
   bson_json_state_t *state = data;
   char str[25];

   bson_oid_to_string (oid, str);
   bson_string_append (state->str, "{ \"$oid\" : \"");
   bson_string_append (state->str, str);
   bson_string_append (state->str, "\" }");

   return false;
}


static bool
_bson_as_json_visit_binary (const bson_iter_t  *iter,
                            const char         *key,
                            bson_subtype_t      v_subtype,
                            size_t              v_binary_len,
                            const uint8_t *v_binary,
                            void               *data)
{
   bson_json_state_t *state = data;
   size_t b64_len;
   char *b64;

   b64_len = (v_binary_len / 3 + 1) * 4 + 1;
   b64 = bson_malloc0 (b64_len);
   b64_ntop (v_binary, v_binary_len, b64, b64_len);

   bson_string_append (state->str, "{ \"$type\" : \"");
   bson_string_append_printf (state->str, "%02x", v_subtype);
   bson_string_append (state->str, "\", \"$binary\" : \"");
   bson_string_append (state->str, b64);
   bson_string_append (state->str, "\" }");
   bson_free (b64);

   return false;
}


static bool
_bson_as_json_visit_bool (const bson_iter_t *iter,
                          const char        *key,
                          bool        v_bool,
                          void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, v_bool ? "true" : "false");

   return false;
}


static bool
_bson_as_json_visit_date_time (const bson_iter_t *iter,
                               const char        *key,
                               int64_t       msec_since_epoch,
                               void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "{ \"$date\" : ");
   bson_string_append_printf (state->str, "%" PRId64, msec_since_epoch);
   bson_string_append (state->str, " }");

   return false;
}


static bool
_bson_as_json_visit_regex (const bson_iter_t *iter,
                           const char        *key,
                           const char        *v_regex,
                           const char        *v_options,
                           void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "{ \"$regex\" : \"");
   bson_string_append (state->str, v_regex);
   bson_string_append (state->str, "\", \"$options\" : \"");
   bson_string_append (state->str, v_options);
   bson_string_append (state->str, "\" }");

   return false;
}


static bool
_bson_as_json_visit_timestamp (const bson_iter_t *iter,
                               const char        *key,
                               uint32_t      v_timestamp,
                               uint32_t      v_increment,
                               void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "{ \"$timestamp\" : { \"t\" : ");
   bson_string_append_printf (state->str, "%u", v_timestamp);
   bson_string_append (state->str, ", \"i\" : ");
   bson_string_append_printf (state->str, "%u", v_increment);
   bson_string_append (state->str, " } }");

   return false;
}


static bool
_bson_as_json_visit_dbpointer (const bson_iter_t *iter,
                               const char        *key,
                               size_t             v_collection_len,
                               const char        *v_collection,
                               const bson_oid_t  *v_oid,
                               void              *data)
{
   bson_json_state_t *state = data;
   char str[25];

   bson_string_append (state->str, "{ \"$ref\" : \"");
   bson_string_append (state->str, v_collection);
   bson_string_append (state->str, "\"");

   if (v_oid) {
      bson_oid_to_string (v_oid, str);
      bson_string_append (state->str, ", \"$id\" : \"");
      bson_string_append (state->str, str);
      bson_string_append (state->str, "\"");
   }

   bson_string_append (state->str, " }");

   return false;
}


static bool
_bson_as_json_visit_minkey (const bson_iter_t *iter,
                            const char        *key,
                            void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "{ \"$minKey\" : 1 }");

   return false;
}


static bool
_bson_as_json_visit_maxkey (const bson_iter_t *iter,
                            const char        *key,
                            void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "{ \"$maxKey\" : 1 }");

   return false;
}




static bool
_bson_as_json_visit_before (const bson_iter_t *iter,
                            const char        *key,
                            void              *data)
{
   bson_json_state_t *state = data;
   char *escaped;

   if (state->count) {
      bson_string_append (state->str, ", ");
   }

   if (state->keys) {
      escaped = bson_utf8_escape_for_json (key, -1);
      if (escaped) {
         bson_string_append (state->str, "\"");
         bson_string_append (state->str, escaped);
         bson_string_append (state->str, "\" : ");
         bson_free (escaped);
      } else {
         return true;
      }
   }

   state->count++;

   return false;
}


static bool
_bson_as_json_visit_code (const bson_iter_t *iter,
                          const char        *key,
                          size_t             v_code_len,
                          const char        *v_code,
                          void              *data)
{
   bson_json_state_t *state = data;
   char *escaped;

   escaped = bson_utf8_escape_for_json (v_code, v_code_len);

   if (escaped) {
      bson_string_append (state->str, "\"");
      bson_string_append (state->str, escaped);
      bson_string_append (state->str, "\"");
      bson_free (escaped);
      return false;
   }

   return true;
}


static bool
_bson_as_json_visit_symbol (const bson_iter_t *iter,
                            const char        *key,
                            size_t             v_symbol_len,
                            const char        *v_symbol,
                            void              *data)
{
   bson_json_state_t *state = data;

   bson_string_append (state->str, "\"");
   bson_string_append (state->str, v_symbol);
   bson_string_append (state->str, "\"");

   return false;
}


static bool
_bson_as_json_visit_codewscope (const bson_iter_t *iter,
                                const char        *key,
                                size_t             v_code_len,
                                const char        *v_code,
                                const bson_t      *v_scope,
                                void              *data)
{
   bson_json_state_t *state = data;
   char *escaped;

   escaped = bson_utf8_escape_for_json (v_code, v_code_len);

   if (escaped) {
      bson_string_append (state->str, "\"");
      bson_string_append (state->str, escaped);
      bson_string_append (state->str, "\"");
      bson_free (escaped);
      return false;
   }

   return true;
}


static const bson_visitor_t bson_as_json_visitors = {
   _bson_as_json_visit_before,
   NULL, /* visit_after */
   NULL, /* visit_corrupt */
   _bson_as_json_visit_double,
   _bson_as_json_visit_utf8,
   _bson_as_json_visit_document,
   _bson_as_json_visit_array,
   _bson_as_json_visit_binary,
   _bson_as_json_visit_undefined,
   _bson_as_json_visit_oid,
   _bson_as_json_visit_bool,
   _bson_as_json_visit_date_time,
   _bson_as_json_visit_null,
   _bson_as_json_visit_regex,
   _bson_as_json_visit_dbpointer,
   _bson_as_json_visit_code,
   _bson_as_json_visit_symbol,
   _bson_as_json_visit_codewscope,
   _bson_as_json_visit_int32,
   _bson_as_json_visit_timestamp,
   _bson_as_json_visit_int64,
   _bson_as_json_visit_maxkey,
   _bson_as_json_visit_minkey,
};


static bool
_bson_as_json_visit_document (const bson_iter_t *iter,
                              const char        *key,
                              const bson_t      *v_document,
                              void              *data)
{
   bson_json_state_t *state = data;
   bson_json_state_t child_state = { 0, true };
   bson_iter_t child;

   if (state->depth >= BSON_MAX_RECURSION) {
      bson_string_append (state->str, "{ ... }");
      return false;
   }

   if (bson_iter_init (&child, v_document)) {
      child_state.str = bson_string_new ("{ ");
      child_state.depth = state->depth + 1;
      bson_iter_visit_all (&child, &bson_as_json_visitors, &child_state);
      bson_string_append (child_state.str, " }");
      bson_string_append (state->str, child_state.str->str);
      bson_string_free (child_state.str, true);
   }

   return false;
}


static bool
_bson_as_json_visit_array (const bson_iter_t *iter,
                           const char        *key,
                           const bson_t      *v_array,
                           void              *data)
{
   bson_json_state_t *state = data;
   bson_json_state_t child_state = { 0, false };
   bson_iter_t child;

   if (state->depth >= BSON_MAX_RECURSION) {
      bson_string_append (state->str, "{ ... }");
      return false;
   }

   if (bson_iter_init (&child, v_array)) {
      child_state.str = bson_string_new ("[ ");
      child_state.depth = state->depth + 1;
      bson_iter_visit_all (&child, &bson_as_json_visitors, &child_state);
      bson_string_append (child_state.str, " ]");
      bson_string_append (state->str, child_state.str->str);
      bson_string_free (child_state.str, true);
   }

   return false;
}


char *
bson_as_json (const bson_t *bson,
              size_t       *length)
{
   bson_json_state_t state;
   bson_iter_t iter;

   BSON_ASSERT (bson);

   if (length) {
      *length = 0;
   }

   if (bson_empty0 (bson)) {
      if (length) {
         *length = 3;
      }

      return bson_strdup ("{ }");
   }

   if (!bson_iter_init (&iter, bson)) {
      return NULL;
   }

   state.count = 0;
   state.keys = true;
   state.str = bson_string_new ("{ ");
   state.depth = 0;

   if (bson_iter_visit_all (&iter, &bson_as_json_visitors, &state) ||
       iter.err_off) {
      /*
       * We were prematurely exited due to corruption or failed visitor.
       */
      bson_string_free (state.str, true);
      if (length) {
         *length = 0;
      }
      return NULL;
   }

   bson_string_append (state.str, " }");

   if (length) {
      *length = state.str->len;
   }

   return bson_string_free (state.str, false);
}


char *
bson_array_as_json (const bson_t *bson,
                    size_t       *length)
{
   bson_json_state_t state;
   bson_iter_t iter;

   BSON_ASSERT (bson);

   if (length) {
      *length = 0;
   }

   if (bson_empty0 (bson)) {
      if (length) {
         *length = 3;
      }

      return bson_strdup ("[ ]");
   }

   if (!bson_iter_init (&iter, bson)) {
      return NULL;
   }

   state.count = 0;
   state.keys = false;
   state.str = bson_string_new ("[ ");
   state.depth = 0;
   bson_iter_visit_all (&iter, &bson_as_json_visitors, &state);

   if (bson_iter_visit_all (&iter, &bson_as_json_visitors, &state) ||
       iter.err_off) {
      /*
       * We were prematurely exited due to corruption or failed visitor.
       */
      bson_string_free (state.str, true);
      if (length) {
         *length = 0;
      }
      return NULL;
   }

   bson_string_append (state.str, " ]");

   if (length) {
      *length = state.str->len;
   }

   return bson_string_free (state.str, false);
}


static bool
_bson_iter_validate_utf8 (const bson_iter_t *iter,
                          const char        *key,
                          size_t             v_utf8_len,
                          const char        *v_utf8,
                          void              *data)
{
   bson_validate_state_t *state = data;
   bool allow_null;

   if ((state->flags & BSON_VALIDATE_UTF8)) {
      allow_null = !!(state->flags & BSON_VALIDATE_UTF8_ALLOW_NULL);

      if (!bson_utf8_validate (v_utf8, v_utf8_len, allow_null)) {
         state->err_offset = iter->off;
         return true;
      }
   }

   if ((state->flags & BSON_VALIDATE_DOLLAR_KEYS)) {
      if (state->phase == BSON_VALIDATE_PHASE_LF_REF_UTF8) {
         state->phase = BSON_VALIDATE_PHASE_LF_ID_KEY;
      } else if (state->phase == BSON_VALIDATE_PHASE_LF_DB_UTF8) {
         state->phase = BSON_VALIDATE_PHASE_NOT_DBREF;
      }
   }

   return false;
}


static void
_bson_iter_validate_corrupt (const bson_iter_t *iter,
                             void              *data)
{
   bson_validate_state_t *state = data;

   state->err_offset = iter->err_off;
}


static bool
_bson_iter_validate_before (const bson_iter_t *iter,
                            const char        *key,
                            void              *data)
{
   bson_validate_state_t *state = data;

   if ((state->flags & BSON_VALIDATE_DOLLAR_KEYS)) {
      if (key[0] == '$') {
         if (state->phase == BSON_VALIDATE_PHASE_LF_REF_KEY &&
             strcmp (key, "$ref") == 0) {
            state->phase = BSON_VALIDATE_PHASE_LF_REF_UTF8;
         } else if (state->phase == BSON_VALIDATE_PHASE_LF_ID_KEY &&
                    strcmp (key, "$id") == 0) {
            state->phase = BSON_VALIDATE_PHASE_LF_DB_KEY;
         } else if (state->phase == BSON_VALIDATE_PHASE_LF_DB_KEY &&
                    strcmp (key, "$db") == 0) {
            state->phase = BSON_VALIDATE_PHASE_LF_DB_UTF8;
         } else {
            state->err_offset = iter->off;
            return true;
         }
      } else if (state->phase == BSON_VALIDATE_PHASE_LF_ID_KEY ||
                 state->phase == BSON_VALIDATE_PHASE_LF_REF_UTF8 ||
                 state->phase == BSON_VALIDATE_PHASE_LF_DB_UTF8) {
         state->err_offset = iter->off;
         return true;
      } else {
         state->phase = BSON_VALIDATE_PHASE_NOT_DBREF;
      }
   }

   if ((state->flags & BSON_VALIDATE_DOT_KEYS)) {
      if (strstr (key, ".")) {
         state->err_offset = iter->off;
         return true;
      }
   }

   return false;
}


static bool
_bson_iter_validate_codewscope (const bson_iter_t *iter,
                                const char        *key,
                                size_t             v_code_len,
                                const char        *v_code,
                                const bson_t      *v_scope,
                                void              *data)
{
   bson_validate_state_t *state = data;
   size_t offset;

   if (!bson_validate (v_scope, state->flags, &offset)) {
      state->err_offset = iter->off + offset;
      return false;
   }

   return true;
}


static bool
_bson_iter_validate_document (const bson_iter_t *iter,
                              const char        *key,
                              const bson_t      *v_document,
                              void              *data);


static const bson_visitor_t bson_validate_funcs = {
   _bson_iter_validate_before,
   NULL, /* visit_after */
   _bson_iter_validate_corrupt,
   NULL, /* visit_double */
   _bson_iter_validate_utf8,
   _bson_iter_validate_document,
   _bson_iter_validate_document, /* visit_array */
   NULL, /* visit_binary */
   NULL, /* visit_undefined */
   NULL, /* visit_oid */
   NULL, /* visit_bool */
   NULL, /* visit_date_time */
   NULL, /* visit_null */
   NULL, /* visit_regex */
   NULL, /* visit_dbpoint */
   NULL, /* visit_code */
   NULL, /* visit_symbol */
   _bson_iter_validate_codewscope,
};


static bool
_bson_iter_validate_document (const bson_iter_t *iter,
                              const char        *key,
                              const bson_t      *v_document,
                              void              *data)
{
   bson_validate_state_t *state = data;
   bson_iter_t child;
   bson_validate_phase_t phase = state->phase;

   if (!bson_iter_init (&child, v_document)) {
      state->err_offset = iter->off;
      return true;
   }

   if (state->phase == BSON_VALIDATE_PHASE_START) {
      state->phase = BSON_VALIDATE_PHASE_TOP;
   } else {
      state->phase = BSON_VALIDATE_PHASE_LF_REF_KEY;
   }

   bson_iter_visit_all (&child, &bson_validate_funcs, state);

   if (state->phase == BSON_VALIDATE_PHASE_LF_ID_KEY ||
       state->phase == BSON_VALIDATE_PHASE_LF_REF_UTF8 ||
       state->phase == BSON_VALIDATE_PHASE_LF_DB_UTF8) {
       state->err_offset = iter->off;
       return true;
   }

   state->phase = phase;

   return false;
}


bool
bson_validate (const bson_t         *bson,
               bson_validate_flags_t flags,
               size_t               *offset)
{
   bson_validate_state_t state = { flags, -1, BSON_VALIDATE_PHASE_START };
   bson_iter_t iter;

   if (!bson_iter_init (&iter, bson)) {
      state.err_offset = 0;
      goto failure;
   }

   _bson_iter_validate_document (&iter, NULL, bson, &state);

failure:

   if (offset) {
      *offset = state.err_offset;
   }

   return state.err_offset < 0;
}


bool
bson_concat (bson_t       *dst,
             const bson_t *src)
{
   BSON_ASSERT (dst);
   BSON_ASSERT (src);

   if (!bson_empty (src)) {
      return _bson_append (dst, 1, src->len - 5,
                           src->len - 5, _bson_data (src) + 4);
   }

   return true;
}

/*==============================================================*/
/* --- bson-clock.c */

/*
 *--------------------------------------------------------------------------
 *
 * bson_gettimeofday --
 *
 *       A wrapper around gettimeofday() with fallback support for Windows.
 *
 * Returns:
 *       0 if successful.
 *
 * Side effects:
 *       @tv is set.
 *
 *--------------------------------------------------------------------------
 */

int
bson_gettimeofday (struct timeval *tv) /* OUT */
{
#if defined(_WIN32)
# if defined(_MSC_VER)
#  define DELTA_EPOCH_IN_MICROSEC 11644473600000000Ui64
# else
#  define DELTA_EPOCH_IN_MICROSEC 11644473600000000ULL
# endif
  FILETIME ft;
  uint64_t tmp = 0;

   /*
    * The const value is shamelessy stolen from
    * http://www.boost.org/doc/libs/1_55_0/boost/chrono/detail/inlined/win/chrono.hpp
    *
    * File times are the number of 100 nanosecond intervals elapsed since
    * 12:00 am Jan 1, 1601 UTC.  I haven't check the math particularly hard
    *
    * ...  good luck
    */

   if (tv) {
      GetSystemTimeAsFileTime (&ft);

      /* pull out of the filetime into a 64 bit uint */
      tmp |= ft.dwHighDateTime;
      tmp <<= 32;
      tmp |= ft.dwLowDateTime;

      /* convert from 100's of nanosecs to microsecs */
      tmp /= 10;

      /* adjust to unix epoch */
      tmp -= DELTA_EPOCH_IN_MICROSEC;

      tv->tv_sec = (long)(tmp / 1000000UL);
      tv->tv_usec = (long)(tmp % 1000000UL);
   }

   return 0;
#else
   return gettimeofday (tv, NULL);
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_get_monotonic_time --
 *
 *       Returns the monotonic system time, if available. A best effort is
 *       made to use the monotonic clock. However, some systems may not
 *       support such a feature.
 *
 * Returns:
 *       The monotonic clock in microseconds.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_get_monotonic_time (void)
{
#if defined(BSON_HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
   struct timespec ts;
   clock_gettime (CLOCK_MONOTONIC, &ts);
   return ((ts.tv_sec * 1000000UL) + (ts.tv_nsec / 1000UL));
#elif defined(__APPLE__)
   static mach_timebase_info_data_t info = { 0 };
   static double ratio = 0.0;

   if (!info.denom) {
      // the value from mach_absolute_time () * info.numer / info.denom
      // is in nano seconds. So we have to divid by 1000.0 to get micro seconds
      mach_timebase_info (&info);
      ratio = (double)info.numer / (double)info.denom / 1000.0;
   }

   return mach_absolute_time () * ratio;
#elif defined(_WIN32)
   /* Despite it's name, this is in milliseconds! */
   int64_t ticks = GetTickCount64 ();
   return (ticks * 1000L);
#else
# warning "Monotonic clock is not yet supported on your platform."
   struct timeval tv;

   bson_gettimeofday (&tv);
   return (tv.tv_sec * 1000000UL) + tv.tv_usec;
#endif
}

/*==============================================================*/
/* --- bson-context.c */

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif


/*
 * Globals.
 */
static bson_context_t gContextDefault;


#if defined(__linux__)
static uint16_t
gettid (void)
{
   return syscall (SYS_gettid);
}
#endif


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_host --
 *
 *       Retrieves the first three bytes of MD5(hostname) and assigns them
 *       to the host portion of oid.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_host (bson_context_t *context,  /* IN */
                            bson_oid_t     *oid)      /* OUT */
{
   uint8_t *bytes = (uint8_t *)oid;
   uint8_t digest[16];
   bson_md5_t md5;
   char hostname[HOST_NAME_MAX];

   BSON_ASSERT (context);
   BSON_ASSERT (oid);

   gethostname (hostname, sizeof hostname);
   hostname[HOST_NAME_MAX - 1] = '\0';

   bson_md5_init (&md5);
   bson_md5_append (&md5, (const uint8_t *)hostname, (uint32_t)strlen (hostname));
   bson_md5_finish (&md5, &digest[0]);

   bytes[4] = digest[0];
   bytes[5] = digest[1];
   bytes[6] = digest[2];
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_host_cached --
 *
 *       Fetch the cached copy of the MD5(hostname).
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_host_cached (bson_context_t *context, /* IN */
                                   bson_oid_t     *oid)     /* OUT */
{
   BSON_ASSERT (context);
   BSON_ASSERT (oid);

   oid->bytes[4] = context->md5[0];
   oid->bytes[5] = context->md5[1];
   oid->bytes[6] = context->md5[2];
}


static BSON_INLINE uint16_t
_bson_getpid (void)
{
   uint16_t pid;
#ifdef BSON_OS_WIN32
   DWORD real_pid;

   real_pid = GetCurrentProcessId ();
   pid = (real_pid & 0xFFFF) ^ ((real_pid >> 16) & 0xFFFF);
#else
   pid = getpid ();
#endif

   return pid;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_pid --
 *
 *       Initialize the pid field of @oid.
 *
 *       The pid field is 2 bytes, big-endian for memcmp().
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_pid (bson_context_t *context, /* IN */
                           bson_oid_t     *oid)     /* OUT */
{
   uint16_t pid = _bson_getpid ();
   uint8_t *bytes = (uint8_t *)&pid;

   BSON_ASSERT (context);
   BSON_ASSERT (oid);

   pid = BSON_UINT16_TO_BE (pid);

   oid->bytes[7] = bytes[0];
   oid->bytes[8] = bytes[1];
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_pid_cached --
 *
 *       Fetch the cached copy of the current pid.
 *       This helps avoid multiple calls to getpid() which is slower
 *       on some systems.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_pid_cached (bson_context_t *context, /* IN */
                                  bson_oid_t     *oid)     /* OUT */
{
   oid->bytes[7] = context->pidbe[0];
   oid->bytes[8] = context->pidbe[1];
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_seq32 --
 *
 *       32-bit sequence generator, non-thread-safe version.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_seq32 (bson_context_t *context, /* IN */
                             bson_oid_t     *oid)     /* OUT */
{
   uint32_t seq = context->seq32++;

   seq = BSON_UINT32_TO_BE (seq);
   memcpy (&oid->bytes[9], ((uint8_t *)&seq) + 1, 3);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_seq32_threadsafe --
 *
 *       Thread-safe version of 32-bit sequence generator.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_seq32_threadsafe (bson_context_t *context, /* IN */
                                        bson_oid_t     *oid)     /* OUT */
{
   int32_t seq = bson_atomic_int_add (&context->seq32, 1);

   seq = BSON_UINT32_TO_BE (seq);
   memcpy (&oid->bytes[9], ((uint8_t *)&seq) + 1, 3);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_seq64 --
 *
 *       64-bit oid sequence generator, non-thread-safe version.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_seq64 (bson_context_t *context, /* IN */
                             bson_oid_t     *oid)     /* OUT */
{
   uint64_t seq;

   BSON_ASSERT (context);
   BSON_ASSERT (oid);

   seq = BSON_UINT64_TO_BE (context->seq64++);
   memcpy (&oid->bytes[4], &seq, sizeof (seq));
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_get_oid_seq64_threadsafe --
 *
 *       Thread-safe 64-bit sequence generator.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_get_oid_seq64_threadsafe (bson_context_t *context, /* IN */
                                        bson_oid_t     *oid)     /* OUT */
{
   int64_t seq = bson_atomic_int64_add (&context->seq64, 1);

   seq = BSON_UINT64_TO_BE (seq);
   memcpy (&oid->bytes[4], &seq, sizeof (seq));
}


static void
_bson_context_init (bson_context_t *context,    /* IN */
                    bson_context_flags_t flags) /* IN */
{
   struct timeval tv;
   uint16_t pid;
   unsigned int seed[3];
   unsigned int real_seed;
   bson_oid_t oid;

   context->flags = flags;
   context->oid_get_host = _bson_context_get_oid_host_cached;
   context->oid_get_pid = _bson_context_get_oid_pid_cached;
   context->oid_get_seq32 = _bson_context_get_oid_seq32;
   context->oid_get_seq64 = _bson_context_get_oid_seq64;

   /*
    * Generate a seed for our the random starting position of our increment
    * bytes. We mask off the last nibble so that the last digit of the OID will
    * start at zero. Just to be nice.
    *
    * The seed itself is made up of the current time in seconds, milliseconds,
    * and pid xored together. I welcome better solutions if at all necessary.
    */
   bson_gettimeofday (&tv);
   seed[0] = (unsigned int)tv.tv_sec;
   seed[1] = (unsigned int)tv.tv_usec;
   seed[2] = _bson_getpid ();
   real_seed = seed[0] ^ seed[1] ^ seed[2];

#ifdef BSON_OS_WIN32
   /* ms's runtime is multithreaded by default, so no rand_r */
   srand(real_seed);
   context->seq32 = rand() & 0x007FFFF0;
#else
   context->seq32 = rand_r (&real_seed) & 0x007FFFF0;
#endif

   if ((flags & BSON_CONTEXT_DISABLE_HOST_CACHE)) {
      context->oid_get_host = _bson_context_get_oid_host;
   } else {
      _bson_context_get_oid_host (context, &oid);
      context->md5[0] = oid.bytes[4];
      context->md5[1] = oid.bytes[5];
      context->md5[2] = oid.bytes[6];
   }

   if ((flags & BSON_CONTEXT_THREAD_SAFE)) {
      context->oid_get_seq32 = _bson_context_get_oid_seq32_threadsafe;
      context->oid_get_seq64 = _bson_context_get_oid_seq64_threadsafe;
   }

   if ((flags & BSON_CONTEXT_DISABLE_PID_CACHE)) {
      context->oid_get_pid = _bson_context_get_oid_pid;
   } else {
      pid = BSON_UINT16_TO_BE (_bson_getpid());
#if defined(__linux__)

      if ((flags & BSON_CONTEXT_USE_TASK_ID)) {
         int32_t tid;

         if ((tid = gettid ())) {
            pid = BSON_UINT16_TO_BE (tid);
         }
      }

#endif
      memcpy (&context->pidbe[0], &pid, 2);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_context_new --
 *
 *       Initializes a new context with the flags specified.
 *
 *       In most cases, you want to call this with @flags set to
 *       BSON_CONTEXT_NONE.
 *
 *       If you are running on Linux, %BSON_CONTEXT_USE_TASK_ID can result
 *       in a healthy speedup for multi-threaded scenarios.
 *
 *       If you absolutely must have a single context for your application
 *       and use more than one thread, then %BSON_CONTEXT_THREAD_SAFE should
 *       be bitwise-or'd with your flags. This requires synchronization
 *       between threads.
 *
 *       If you expect your hostname to change often, you may consider
 *       specifying %BSON_CONTEXT_DISABLE_HOST_CACHE so that gethostname()
 *       is called for every OID generated. This is much slower.
 *
 *       If you expect your pid to change without notice, such as from an
 *       unexpected call to fork(), then specify
 *       %BSON_CONTEXT_DISABLE_PID_CACHE.
 *
 * Returns:
 *       A newly allocated bson_context_t that should be freed with
 *       bson_context_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_context_t *
bson_context_new (bson_context_flags_t flags)
{
   bson_context_t *context;

   context = bson_malloc0 (sizeof *context);
   _bson_context_init (context, flags);

   return context;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_context_destroy --
 *
 *       Cleans up a bson_context_t and releases any associated resources.
 *       This should be called when you are done using @context.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_context_destroy (bson_context_t *context)  /* IN */
{
   if (context != &gContextDefault) {
      memset (context, 0, sizeof *context);
      bson_free (context);
   }
}


static
BSON_ONCE_FUN(_bson_context_init_default)
{
   _bson_context_init (&gContextDefault,
                       (BSON_CONTEXT_THREAD_SAFE |
                        BSON_CONTEXT_DISABLE_PID_CACHE));
   BSON_ONCE_RETURN;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_context_get_default --
 *
 *       Fetches the default, thread-safe implementation of #bson_context_t.
 *       If you need faster generation, it is recommended you create your
 *       own #bson_context_t with bson_context_new().
 *
 * Returns:
 *       A shared instance to the default #bson_context_t. This should not
 *       be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_context_t *
bson_context_get_default (void)
{
   static bson_once_t once = BSON_ONCE_INIT;

   bson_once (&once, _bson_context_init_default);

   return &gContextDefault;
}

/*==============================================================*/
/* --- bson-error.c */

/*
 *--------------------------------------------------------------------------
 *
 * bson_set_error --
 *
 *       Initializes @error using the parameters specified.
 *
 *       @domain is an application specific error domain which should
 *       describe which module initiated the error. Think of this as the
 *       exception type.
 *
 *       @code is the @domain specific error code.
 *
 *       @format is used to generate the format string. It uses vsnprintf()
 *       internally so the format should match what you would use there.
 *
 * Parameters:
 *       @error: A #bson_error_t.
 *       @domain: The error domain.
 *       @code: The error code.
 *       @format: A printf style format string.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @error is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_set_error (bson_error_t *error,  /* OUT */
                uint32_t      domain, /* IN */
                uint32_t      code,   /* IN */
                const char   *format, /* IN */
                ...)                  /* IN */
{
   va_list args;

   if (error) {
      error->domain = domain;
      error->code = code;

      va_start (args, format);
      bson_vsnprintf (error->message, sizeof error->message, format, args);
      va_end (args);

      error->message[sizeof error->message - 1] = '\0';
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strerror_r --
 *
 *       This is a reentrant safe macro for strerror.
 *
 *       The resulting string may be stored in @buf.
 *
 * Returns:
 *       A pointer to a static string or @buf.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_strerror_r (int     err_code,  /* IN */
                 char   *buf,       /* IN */
                 size_t  buflen)    /* IN */
{
   static const char *unknown_msg = "Unknown error";
   char *ret = NULL;

#if defined(__GNUC__) && defined(_GNU_SOURCE)
   ret = strerror_r (err_code, buf, buflen);
#elif defined(_WIN32)
   if (strerror_s (buf, buflen, err_code) != 0) {
      ret = buf;
   }
#else /* XSI strerror_r */
   if (strerror_r (err_code, buf, buflen) == 0) {
      ret = buf;
   }
#endif

   if (!ret) {
      bson_strncpy (buf, unknown_msg, buflen);
      ret = buf;
   }

   return ret;
}


/*==============================================================*/
/* --- bson-iso8601.c */

static bool
get_tok (const char  *terminals,
         const char **ptr,
         int32_t     *remaining,
         const char **out,
         int32_t     *out_len)
{
   const char *terminal;
   bool found_terminal = false;

   if (!*remaining) {
      *out = "";
      *out_len = 0;
   }

   *out = *ptr;
   *out_len = -1;

   for (; *remaining && !found_terminal;
        (*ptr)++, (*remaining)--, (*out_len)++) {
      for (terminal = terminals; *terminal; terminal++) {
         if (**ptr == *terminal) {
            found_terminal = true;
            break;
         }
      }
   }

   if (!found_terminal) {
      (*out_len)++;
   }

   return found_terminal;
}

static bool
digits_only (const char *str,
             int32_t     len)
{
   int i;

   for (i = 0; i < len; i++) {
      if (!isdigit(str[i])) {
         return false;
      }
   }

   return true;
}

static bool
parse_num (const char *str,
           int32_t     len,
           int32_t     digits,
           int32_t     min,
           int32_t     max,
           int32_t    *out)
{
   int i;
   int magnitude = 1;
   int32_t value = 0;

   if ((digits >= 0 && len != digits) || !digits_only (str, len)) {
      return false;
   }

   for (i = 1; i <= len; i++, magnitude *= 10) {
      value += (str[len - i] - '0') * magnitude;
   }

   if (value < min || value > max) {
      return false;
   }

   *out = value;

   return true;
}

bool
_bson_iso8601_date_parse (const char *str,
                          int32_t     len,
                          int64_t    *out)
{
   const char *ptr;
   int32_t remaining = len;

   const char *year_ptr;
   const char *month_ptr;
   const char *day_ptr;
   const char *hour_ptr;
   const char *min_ptr;
   const char *sec_ptr;
   const char *millis_ptr;
   const char *tz_ptr;

   int32_t year_len = 0;
   int32_t month_len = 0;
   int32_t day_len = 0;
   int32_t hour_len = 0;
   int32_t min_len = 0;
   int32_t sec_len = 0;
   int32_t millis_len = 0;
   int32_t tz_len = 0;

   int32_t year;
   int32_t month;
   int32_t day;
   int32_t hour;
   int32_t min;
   int32_t sec = 0;
   int64_t millis = 0;
   int32_t tz_adjustment = 0;

#ifdef BSON_OS_WIN32
   SYSTEMTIME win_sys_time;
   FILETIME win_file_time;
   int64_t win_time_offset;
   int64_t win_epoch_difference;
#else
   struct tm posix_date = { 0 };
#endif

   ptr = str;

   /* we have to match at least yyyy-mm-ddThh:mm[:+-Z] */
   if (!(get_tok ("-", &ptr, &remaining, &year_ptr,
                  &year_len) &&
         get_tok ("-", &ptr, &remaining, &month_ptr,
                  &month_len) &&
         get_tok ("T", &ptr, &remaining, &day_ptr,
                  &day_len) &&
         get_tok (":", &ptr, &remaining, &hour_ptr,
                  &hour_len) &&
         get_tok (":+-Z", &ptr, &remaining, &min_ptr, &min_len))) {
      return false;
   }

   /* if the minute has a ':' at the end look for seconds */
   if (min_ptr[min_len] == ':') {
      if (remaining < 2) {
         return false;
      }

      get_tok (".+-Z", &ptr, &remaining, &sec_ptr, &sec_len);

      if (!sec_len) {
         return false;
      }
   }

   /* if we had a second and it is followed by a '.' look for milliseconds */
   if (sec_len && sec_ptr[sec_len] == '.') {
      if (remaining < 2) {
         return false;
      }

      get_tok ("+-Z", &ptr, &remaining, &millis_ptr, &millis_len);

      if (!millis_len) {
         return false;
      }
   }

   /* backtrack by 1 to put ptr on the timezone */
   ptr--;
   remaining++;

   get_tok ("", &ptr, &remaining, &tz_ptr, &tz_len);

   /* we want to include the last few hours in 1969 for timezones translate
    * across 1970 GMT.  We'll check in timegm later on to make sure we're post
    * 1970 */
   if (!parse_num (year_ptr, year_len, 4, 1969, 9999, &year)) {
      return false;
   }

   /* values are as in struct tm */
   year -= 1900;

   if (!parse_num (month_ptr, month_len, 2, 1, 12, &month)) {
      return false;
   }

   /* values are as in struct tm */
   month -= 1;

   if (!parse_num (day_ptr, day_len, 2, 1, 31, &day)) {
      return false;
   }

   if (!parse_num (hour_ptr, hour_len, 2, 0, 23, &hour)) {
      return false;
   }

   if (!parse_num (min_ptr, min_len, 2, 0, 59, &min)) {
      return false;
   }

   if (sec_len && !parse_num (sec_ptr, sec_len, 2, 0, 60, &sec)) {
      return false;
   }

   if (tz_len > 0) {
      if (tz_ptr[0] == 'Z' && tz_len == 1) {
         /* valid */
      } else if (tz_ptr[0] == '+' || tz_ptr[0] == '-') {
         int32_t tz_hour;
         int32_t tz_min;

         if (tz_len != 5 || !digits_only (tz_ptr + 1, 4)) {
            return false;
         }

         if (!parse_num (tz_ptr + 1, 2, -1, -23, 23, &tz_hour)) {
            return false;
         }

         if (!parse_num (tz_ptr + 3, 2, -1, 0, 59, &tz_min)) {
            return false;
         }

         /* we inflect the meaning of a 'positive' timezone.  Those are hours
          * we have to substract, and vice versa */
         tz_adjustment =
            (tz_ptr[0] == '-' ? 1 : -1) * ((tz_min * 60) + (tz_hour * 60 * 60));

         if (!(tz_adjustment > -86400 && tz_adjustment < 86400)) {
            return false;
         }
      } else {
         return false;
      }
   }

   if (millis_len > 0) {
      int i;
      int magnitude;
      millis = 0;

      if (millis_len > 3 || !digits_only (millis_ptr, millis_len)) {
         return false;
      }

      for (i = 1, magnitude = 1; i <= millis_len; i++, magnitude *= 10) {
         millis += (millis_ptr[millis_len - i] - '0') * magnitude;
      }

      if (millis_len == 1) {
         millis *= 100;
      } else if (millis_len == 2) {
         millis *= 10;
      }

      if (millis < 0 || millis > 1000) {
         return false;
      }
   }

#ifdef BSON_OS_WIN32
   win_sys_time.wMilliseconds = millis;
   win_sys_time.wSecond = sec;
   win_sys_time.wMinute = min;
   win_sys_time.wHour = hour;
   win_sys_time.wDay = day;
   win_sys_time.wDayOfWeek = -1;  /* ignored */
   win_sys_time.wMonth = month + 1;
   win_sys_time.wYear = year + 1900;

   /* the wDayOfWeek member of SYSTEMTIME is ignored by this function */
   if (SystemTimeToFileTime (&win_sys_time, &win_file_time) == 0) {
      return 0;
   }

   /* The Windows FILETIME structure contains two parts of a 64-bit value representing the
    * number of 100-nanosecond intervals since January 1, 1601
    */
   win_time_offset =
      (((uint64_t)win_file_time.dwHighDateTime) << 32) |
      win_file_time.dwLowDateTime;

   /* There are 11644473600 seconds between the unix epoch and the windows epoch
    * 100-nanoseconds = milliseconds * 10000
    */
   win_epoch_difference = 11644473600000 * 10000;

   /* removes the diff between 1970 and 1601 */
   win_time_offset -= win_epoch_difference;

   /* 1 milliseconds = 1000000 nanoseconds = 10000 100-nanosecond intervals */
   millis = win_time_offset / 10000;
#else
   posix_date.tm_sec = sec;
   posix_date.tm_min = min;
   posix_date.tm_hour = hour;
   posix_date.tm_mday = day;
   posix_date.tm_mon = month;
   posix_date.tm_year = year;
   posix_date.tm_wday = 0;
   posix_date.tm_yday = 0;

   millis = (1000 * ((uint64_t)_bson_timegm (&posix_date))) + millis;

#endif

   millis += tz_adjustment * 1000;

   if (millis < 0) {
      return false;
   }

   *out = millis;

   return true;
}

/*==============================================================*/
/* --- bson-iter.c */

#define ITER_TYPE(i) ((bson_type_t) *((i)->raw + (i)->type))


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_init --
 *
 *       Initializes @iter to be used to iterate @bson.
 *
 * Returns:
 *       true if bson_iter_t was initialized. otherwise false.
 *
 * Side effects:
 *       @iter is initialized.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_init (bson_iter_t  *iter, /* OUT */
                const bson_t *bson) /* IN */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (bson);

   if (BSON_UNLIKELY (bson->len < 5)) {
      memset (iter, 0, sizeof *iter);
      return false;
   }

   iter->raw = bson_get_data (bson);
   iter->len = bson->len;
   iter->off = 0;
   iter->type = 0;
   iter->key = 0;
   iter->d1 = 0;
   iter->d2 = 0;
   iter->d3 = 0;
   iter->d4 = 0;
   iter->next_off = 4;
   iter->err_off = 0;

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_recurse --
 *
 *       Creates a new sub-iter looking at the document or array that @iter
 *       is currently pointing at.
 *
 * Returns:
 *       true if successful and @child was initialized.
 *
 * Side effects:
 *       @child is initialized.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_recurse (const bson_iter_t *iter,  /* IN */
                   bson_iter_t       *child) /* OUT */
{
   const uint8_t *data = NULL;
   uint32_t len = 0;

   BSON_ASSERT (iter);
   BSON_ASSERT (child);

   if (ITER_TYPE (iter) == BSON_TYPE_DOCUMENT) {
      bson_iter_document (iter, &len, &data);
   } else if (ITER_TYPE (iter) == BSON_TYPE_ARRAY) {
      bson_iter_array (iter, &len, &data);
   } else {
      return false;
   }

   child->raw = data;
   child->len = len;
   child->off = 0;
   child->type = 0;
   child->key = 0;
   child->d1 = 0;
   child->d2 = 0;
   child->d3 = 0;
   child->d4 = 0;
   child->next_off = 4;
   child->err_off = 0;

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_init_find --
 *
 *       Initializes a #bson_iter_t and moves the iter to the first field
 *       matching @key.
 *
 * Returns:
 *       true if the field named @key was found; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_init_find (bson_iter_t  *iter, /* INOUT */
                     const bson_t *bson, /* IN */
                     const char   *key)  /* IN */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   return bson_iter_init (iter, bson) && bson_iter_find (iter, key);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_init_find_case --
 *
 *       A case-insensitive version of bson_iter_init_find().
 *
 * Returns:
 *       true if the field was found and @iter is observing that field.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_init_find_case (bson_iter_t  *iter, /* INOUT */
                          const bson_t *bson, /* IN */
                          const char   *key)  /* IN */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (bson);
   BSON_ASSERT (key);

   return bson_iter_init (iter, bson) && bson_iter_find_case (iter, key);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_iter_find_with_len --
 *
 *       Internal helper for finding an exact key.
 *
 * Returns:
 *       true if the field @key was found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_bson_iter_find_with_len (bson_iter_t *iter,   /* INOUT */
                          const char  *key,    /* IN */
                          int          keylen) /* IN */
{
   const char *ikey;

   if (keylen == 0) {
      return false;
   }

   if (keylen < 0) {
      keylen = (int)strlen (key);
   }

   while (bson_iter_next (iter)) {
      ikey = bson_iter_key (iter);

      if ((0 == strncmp (key, ikey, keylen)) && (ikey [keylen] == '\0')) {
         return true;
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_find --
 *
 *       Searches through @iter starting from the current position for a key
 *       matching @key. This is a case-sensitive search meaning "KEY" and
 *       "key" would NOT match.
 *
 * Returns:
 *       true if @key is found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_find (bson_iter_t *iter, /* INOUT */
                const char  *key)  /* IN */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (key);

   return _bson_iter_find_with_len (iter, key, -1);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_find_case --
 *
 *       Searches through @iter starting from the current position for a key
 *       matching @key. This is a case-insensitive search meaning "KEY" and
 *       "key" would match.
 *
 * Returns:
 *       true if @key is found.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_find_case (bson_iter_t *iter, /* INOUT */
                     const char  *key)  /* IN */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (key);

   while (bson_iter_next (iter)) {
#ifdef BSON_OS_WIN32
      if (!_stricmp(key, bson_iter_key (iter))) {
#else
      if (!strcasecmp (key, bson_iter_key (iter))) {
#endif
         return true;
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_find_descendant --
 *
 *       Locates a descendant using the "parent.child.key" notation. This
 *       operates similar to bson_iter_find() except that it can recurse
 *       into children documents using the dot notation.
 *
 * Returns:
 *       true if the descendant was found and @descendant was initialized.
 *
 * Side effects:
 *       @descendant may be initialized.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_find_descendant (bson_iter_t *iter,       /* INOUT */
                           const char  *dotkey,     /* IN */
                           bson_iter_t *descendant) /* OUT */
{
   bson_iter_t tmp;
   const char *dot;
   size_t sublen;

   BSON_ASSERT (iter);
   BSON_ASSERT (dotkey);
   BSON_ASSERT (descendant);

   if ((dot = strchr (dotkey, '.'))) {
      sublen = dot - dotkey;
   } else {
      sublen = strlen (dotkey);
   }

   if (_bson_iter_find_with_len (iter, dotkey, (int)sublen)) {
      if (!dot) {
         *descendant = *iter;
         return true;
      }

      if (BSON_ITER_HOLDS_DOCUMENT (iter) || BSON_ITER_HOLDS_ARRAY (iter)) {
         if (bson_iter_recurse (iter, &tmp)) {
            return bson_iter_find_descendant (&tmp, dot + 1, descendant);
         }
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_key --
 *
 *       Retrieves the key of the current field. The resulting key is valid
 *       while @iter is valid.
 *
 * Returns:
 *       A string that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_key (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   return bson_iter_key_unsafe (iter);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_type --
 *
 *       Retrieves the type of the current field.  It may be useful to check
 *       the type using the BSON_ITER_HOLDS_*() macros.
 *
 * Returns:
 *       A bson_type_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_type_t
bson_iter_type (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (iter->raw);
   BSON_ASSERT (iter->len);

   return bson_iter_type_unsafe (iter);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_next --
 *
 *       Advances @iter to the next field of the underlying BSON document.
 *       If all fields have been exhausted, then %false is returned.
 *
 *       It is a programming error to use @iter after this function has
 *       returned false.
 *
 * Returns:
 *       true if the iter was advanced to the next record.
 *       otherwise false and @iter should be considered invalid.
 *
 * Side effects:
 *       @iter may be invalidated.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_next (bson_iter_t *iter) /* INOUT */
{
   const uint8_t *data;
   uint32_t o;
   unsigned int len;

   BSON_ASSERT (iter);

   if (!iter->raw) {
      return false;
   }

   data = iter->raw;
   len = iter->len;

   iter->off = iter->next_off;
   iter->type = iter->off;
   iter->key = iter->off + 1;
   iter->d1 = 0;
   iter->d2 = 0;
   iter->d3 = 0;
   iter->d4 = 0;

   for (o = iter->off + 1; o < len; o++) {
      if (!data [o]) {
         iter->d1 = ++o;
         goto fill_data_fields;
      }
   }

   goto mark_invalid;

fill_data_fields:

   switch (ITER_TYPE (iter)) {
   case BSON_TYPE_DATE_TIME:
   case BSON_TYPE_DOUBLE:
   case BSON_TYPE_INT64:
   case BSON_TYPE_TIMESTAMP:
      iter->next_off = o + 8;
      break;
   case BSON_TYPE_CODE:
   case BSON_TYPE_SYMBOL:
   case BSON_TYPE_UTF8:
      {
         uint32_t l;

         if ((o + 4) >= len) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         memcpy (&l, iter->raw + iter->d1, sizeof (l));
         l = BSON_UINT32_FROM_LE (l);

         if (l > (len - (o + 4))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->next_off = o + 4 + l;

         /*
          * Make sure the string length includes the NUL byte.
          */
         if (BSON_UNLIKELY ((l == 0) || (iter->next_off >= len))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         /*
          * Make sure the last byte is a NUL byte.
          */
         if (BSON_UNLIKELY ((iter->raw + iter->d2)[l - 1] != '\0')) {
            iter->err_off = o + 4 + l - 1;
            goto mark_invalid;
         }
      }
      break;
   case BSON_TYPE_BINARY:
      {
         bson_subtype_t subtype;
         uint32_t l;

         if (o >= (len - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         iter->d3 = o + 5;

         memcpy (&l, iter->raw + iter->d1, sizeof (l));
         l = BSON_UINT32_FROM_LE (l);

         if (l >= (len - o)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         subtype = *(iter->raw + iter->d2);

         if (subtype == BSON_SUBTYPE_BINARY_DEPRECATED) {
            if (l < 4) {
               iter->err_off = o;
               goto mark_invalid;
            }
         }

         iter->next_off = o + 5 + l;
      }
      break;
   case BSON_TYPE_ARRAY:
   case BSON_TYPE_DOCUMENT:
      {
         uint32_t l;

         if (o >= (len - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         memcpy (&l, iter->raw + iter->d1, sizeof (l));
         l = BSON_UINT32_FROM_LE (l);

         if ((l > len) || (l > (len - o))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->next_off = o + l;
      }
      break;
   case BSON_TYPE_OID:
      iter->next_off = o + 12;
      break;
   case BSON_TYPE_BOOL:
      iter->next_off = o + 1;
      break;
   case BSON_TYPE_REGEX:
      {
         bool eor = false;
         bool eoo = false;

         for (; o < len; o++) {
            if (!data [o]) {
               iter->d2 = ++o;
               eor = true;
               break;
            }
         }

         if (!eor) {
            iter->err_off = iter->next_off;
            goto mark_invalid;
         }

         for (; o < len; o++) {
            if (!data [o]) {
               eoo = true;
               break;
            }
         }

         if (!eoo) {
            iter->err_off = iter->next_off;
            goto mark_invalid;
         }

         iter->next_off = o + 1;
      }
      break;
   case BSON_TYPE_DBPOINTER:
      {
         uint32_t l;

         if (o >= (len - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         memcpy (&l, iter->raw + iter->d1, sizeof (l));
         l = BSON_UINT32_FROM_LE (l);

         if ((l > len) || (l > (len - o))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d3 = o + 4 + l;
         iter->next_off = o + 4 + l + 12;
      }
      break;
   case BSON_TYPE_CODEWSCOPE:
      {
         uint32_t l;
         uint32_t doclen;

         if ((len < 19) || (o >= (len - 14))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->d2 = o + 4;
         iter->d3 = o + 8;

         memcpy (&l, iter->raw + iter->d1, sizeof (l));
         l = BSON_UINT32_FROM_LE (l);

         if ((l < 14) || (l >= (len - o))) {
            iter->err_off = o;
            goto mark_invalid;
         }

         iter->next_off = o + l;

         if (iter->next_off >= len) {
            iter->err_off = o;
            goto mark_invalid;
         }

         memcpy (&l, iter->raw + iter->d2, sizeof (l));
         l = BSON_UINT32_FROM_LE (l);

         if (l >= (len - o - 4 - 4)) {
            iter->err_off = o;
            goto mark_invalid;
         }

         if ((o + 4 + 4 + l + 4) >= iter->next_off) {
            iter->err_off = o + 4;
            goto mark_invalid;
         }

         iter->d4 = o + 4 + 4 + l;
         memcpy (&doclen, iter->raw + iter->d4, sizeof (doclen));
         doclen = BSON_UINT32_FROM_LE (doclen);

         if ((o + 4 + 4 + l + doclen) != iter->next_off) {
            iter->err_off = o + 4 + 4 + l;
            goto mark_invalid;
         }
      }
      break;
   case BSON_TYPE_INT32:
      iter->next_off = o + 4;
      break;
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
      iter->d1 = -1;
      iter->next_off = o;
      break;
   case BSON_TYPE_EOD:
   default:
      iter->err_off = o;
      goto mark_invalid;
   }

   /*
    * Check to see if any of the field locations would overflow the
    * current BSON buffer. If so, set the error location to the offset
    * of where the field starts.
    */
   if (iter->next_off >= len) {
      iter->err_off = o;
      goto mark_invalid;
   }

   iter->err_off = 0;

   return true;

mark_invalid:
   iter->raw = NULL;
   iter->len = 0;
   iter->next_off = 0;

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_binary --
 *
 *       Retrieves the BSON_TYPE_BINARY field. The subtype is stored in
 *       @subtype.  The length of @binary in bytes is stored in @binary_len.
 *
 *       @binary should not be modified or freed and is only valid while
 *       @iter's bson_t is valid and unmodified.
 *
 * Parameters:
 *       @iter: A bson_iter_t
 *       @subtype: A location for the binary subtype.
 *       @binary_len: A location for the length of @binary.
 *       @binary: A location for a pointer to the binary data.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_binary (const bson_iter_t  *iter,        /* IN */
                  bson_subtype_t     *subtype,     /* OUT */
                  uint32_t           *binary_len,  /* OUT */
                  const uint8_t     **binary)      /* OUT */
{
   bson_subtype_t backup;

   BSON_ASSERT (iter);
   BSON_ASSERT (!binary || binary_len);

   if (ITER_TYPE (iter) == BSON_TYPE_BINARY) {
      if (!subtype) {
         subtype = &backup;
      }

      *subtype = (bson_subtype_t) *(iter->raw + iter->d2);

      if (binary) {
         memcpy (binary_len, (iter->raw + iter->d1), sizeof (*binary_len));
         *binary_len = BSON_UINT32_FROM_LE (*binary_len);
         *binary = iter->raw + iter->d3;

         if (*subtype == BSON_SUBTYPE_BINARY_DEPRECATED) {
            *binary_len -= sizeof (int32_t);
            *binary += sizeof (int32_t);
         }
      }

      return;
   }

   if (binary) {
      *binary = NULL;
   }

   if (binary_len) {
      *binary_len = 0;
   }

   if (subtype) {
      *subtype = BSON_SUBTYPE_BINARY;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_bool --
 *
 *       Retrieves the current field of type BSON_TYPE_BOOL.
 *
 * Returns:
 *       true or false, dependent on bson document.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_bool (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_BOOL) {
      return bson_iter_bool_unsafe (iter);
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_as_bool --
 *
 *       If @iter is on a boolean field, returns the boolean. If it is on a
 *       non-boolean field such as int32, int64, or double, it will convert
 *       the value to a boolean.
 *
 *       Zero is false, and non-zero is true.
 *
 * Returns:
 *       true or false, dependent on field type.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_as_bool (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   switch ((int)ITER_TYPE (iter)) {
   case BSON_TYPE_BOOL:
      return bson_iter_bool (iter);
   case BSON_TYPE_DOUBLE:
      return !(bson_iter_double (iter) == 0.0);
   case BSON_TYPE_INT64:
      return !(bson_iter_int64 (iter) == 0);
   case BSON_TYPE_INT32:
      return !(bson_iter_int32 (iter) == 0);
   case BSON_TYPE_UTF8:
      return true;
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
      return false;
   default:
      return true;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_double --
 *
 *       Retrieves the current field of type BSON_TYPE_DOUBLE.
 *
 * Returns:
 *       A double.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

double
bson_iter_double (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DOUBLE) {
      return bson_iter_double_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_int32 --
 *
 *       Retrieves the value of the field of type BSON_TYPE_INT32.
 *
 * Returns:
 *       A 32-bit signed integer.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int32_t
bson_iter_int32 (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_INT32) {
      return bson_iter_int32_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_int64 --
 *
 *       Retrieves a 64-bit signed integer for the current BSON_TYPE_INT64
 *       field.
 *
 * Returns:
 *       A 64-bit signed integer.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_iter_int64 (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_INT64) {
      return bson_iter_int64_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_as_int64 --
 *
 *       If @iter is not an int64 field, it will try to convert the value to
 *       an int64. Such field types include:
 *
 *        - bool
 *        - double
 *        - int32
 *
 * Returns:
 *       An int64_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_iter_as_int64 (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   switch ((int)ITER_TYPE (iter)) {
   case BSON_TYPE_BOOL:
      return (int64_t)bson_iter_bool (iter);
   case BSON_TYPE_DOUBLE:
      return (int64_t)bson_iter_double (iter);
   case BSON_TYPE_INT64:
      return bson_iter_int64 (iter);
   case BSON_TYPE_INT32:
      return (int64_t)bson_iter_int32 (iter);
   default:
      return 0;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_oid --
 *
 *       Retrieves the current field of type %BSON_TYPE_OID. The result is
 *       valid while @iter is valid.
 *
 * Returns:
 *       A bson_oid_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_oid_t *
bson_iter_oid (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_OID) {
      return bson_iter_oid_unsafe (iter);
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_regex --
 *
 *       Fetches the current field from the iter which should be of type
 *       BSON_TYPE_REGEX.
 *
 * Returns:
 *       Regex from @iter. This should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_regex (const bson_iter_t *iter,    /* IN */
                 const char       **options) /* IN */
{
   const char *ret = NULL;
   const char *ret_options = NULL;

   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_REGEX) {
      ret = (const char *)(iter->raw + iter->d1);
      ret_options = (const char *)(iter->raw + iter->d2);
   }

   if (options) {
      *options = ret_options;
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_utf8 --
 *
 *       Retrieves the current field of type %BSON_TYPE_UTF8 as a UTF-8
 *       encoded string.
 *
 * Parameters:
 *       @iter: A bson_iter_t.
 *       @length: A location for the length of the string.
 *
 * Returns:
 *       A string that should not be modified or freed.
 *
 * Side effects:
 *       @length will be set to the result strings length if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_utf8 (const bson_iter_t *iter,   /* IN */
                uint32_t          *length) /* OUT */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_UTF8) {
      if (length) {
         *length = bson_iter_utf8_len_unsafe (iter);
      }

      return (const char *)(iter->raw + iter->d2);
   }

   if (length) {
      *length = 0;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_dup_utf8 --
 *
 *       Copies the current UTF-8 element into a newly allocated string. The
 *       string should be freed using bson_free() when the caller is
 *       finished with it.
 *
 * Returns:
 *       A newly allocated char* that should be freed with bson_free().
 *
 * Side effects:
 *       @length will be set to the result strings length if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_iter_dup_utf8 (const bson_iter_t *iter,   /* IN */
                    uint32_t          *length) /* OUT */
{
   uint32_t local_length = 0;
   const char *str;
   char *ret = NULL;

   BSON_ASSERT (iter);

   if ((str = bson_iter_utf8 (iter, &local_length))) {
      ret = bson_malloc0 (local_length + 1);
      memcpy (ret, str, local_length);
      ret[local_length] = '\0';
   }

   if (length) {
      *length = local_length;
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_code --
 *
 *       Retrieves the current field of type %BSON_TYPE_CODE. The length of
 *       the resulting string is stored in @length.
 *
 * Parameters:
 *       @iter: A bson_iter_t.
 *       @length: A location for the code length.
 *
 * Returns:
 *       A NUL-terminated string containing the code which should not be
 *       modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_code (const bson_iter_t *iter,   /* IN */
                uint32_t          *length) /* OUT */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_CODE) {
      if (length) {
         *length = bson_iter_utf8_len_unsafe (iter);
      }

      return (const char *)(iter->raw + iter->d2);
   }

   if (length) {
      *length = 0;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_codewscope --
 *
 *       Similar to bson_iter_code() but with a scope associated encoded as
 *       a BSON document. @scope should not be modified or freed. It is
 *       valid while @iter is valid.
 *
 * Parameters:
 *       @iter: A #bson_iter_t.
 *       @length: A location for the length of resulting string.
 *       @scope_len: A location for the length of @scope.
 *       @scope: A location for the scope encoded as BSON.
 *
 * Returns:
 *       A NUL-terminated string that should not be modified or freed.
 *
 * Side effects:
 *       @length is set to the resulting string length in bytes.
 *       @scope_len is set to the length of @scope in bytes.
 *       @scope is set to the scope documents buffer which can be
 *       turned into a bson document with bson_init_static().
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_codewscope (const bson_iter_t  *iter,      /* IN */
                      uint32_t           *length,    /* OUT */
                      uint32_t           *scope_len, /* OUT */
                      const uint8_t     **scope)     /* OUT */
{
   uint32_t len;

   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_CODEWSCOPE) {
      if (length) {
         memcpy (&len, iter->raw + iter->d2, sizeof (len));
         *length = BSON_UINT32_FROM_LE (len) - 1;
      }

      memcpy (&len, iter->raw + iter->d4, sizeof (len));
      *scope_len = BSON_UINT32_FROM_LE (len);
      *scope = iter->raw + iter->d4;
      return (const char *)(iter->raw + iter->d3);
   }

   if (length) {
      *length = 0;
   }

   if (scope_len) {
      *scope_len = 0;
   }

   if (scope) {
      *scope = NULL;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_dbpointer --
 *
 *       Retrieves a BSON_TYPE_DBPOINTER field. @collection_len will be set
 *       to the length of the collection name. The collection name will be
 *       placed into @collection. The oid will be placed into @oid.
 *
 *       @collection and @oid should not be modified.
 *
 * Parameters:
 *       @iter: A #bson_iter_t.
 *       @collection_len: A location for the length of @collection.
 *       @collection: A location for the collection name.
 *       @oid: A location for the oid.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @collection_len is set to the length of @collection in bytes
 *       excluding the null byte.
 *       @collection is set to the collection name, including a terminating
 *       null byte.
 *       @oid is initialized with the oid.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_dbpointer (const bson_iter_t  *iter,           /* IN */
                     uint32_t           *collection_len, /* OUT */
                     const char        **collection,     /* OUT */
                     const bson_oid_t  **oid)            /* OUT */
{
   BSON_ASSERT (iter);

   if (collection) {
      *collection = NULL;
   }

   if (oid) {
      *oid = NULL;
   }

   if (ITER_TYPE (iter) == BSON_TYPE_DBPOINTER) {
      if (collection_len) {
         memcpy (collection_len, (iter->raw + iter->d1), sizeof (*collection_len));
         *collection_len = BSON_UINT32_FROM_LE (*collection_len);

         if ((*collection_len) > 0) {
            (*collection_len)--;
         }
      }

      if (collection) {
         *collection = (const char *)(iter->raw + iter->d2);
      }

      if (oid) {
         *oid = (const bson_oid_t *)(iter->raw + iter->d3);
      }
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_symbol --
 *
 *       Retrieves the symbol of the current field of type BSON_TYPE_SYMBOL.
 *
 * Parameters:
 *       @iter: A bson_iter_t.
 *       @length: A location for the length of the symbol.
 *
 * Returns:
 *       A string containing the symbol as UTF-8. The value should not be
 *       modified or freed.
 *
 * Side effects:
 *       @length is set to the resulting strings length in bytes,
 *       excluding the null byte.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_iter_symbol (const bson_iter_t *iter,   /* IN */
                  uint32_t          *length) /* OUT */
{
   const char *ret = NULL;
   uint32_t ret_length = 0;

   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_SYMBOL) {
      ret = (const char *)(iter->raw + iter->d2);
      ret_length = bson_iter_utf8_len_unsafe (iter);
   }

   if (length) {
      *length = ret_length;
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_date_time --
 *
 *       Fetches the number of milliseconds elapsed since the UNIX epoch.
 *       This value can be negative as times before 1970 are valid.
 *
 * Returns:
 *       A signed 64-bit integer containing the number of milliseconds.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int64_t
bson_iter_date_time (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DATE_TIME) {
      return bson_iter_int64_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_time_t --
 *
 *       Retrieves the current field of type BSON_TYPE_DATE_TIME as a
 *       time_t.
 *
 * Returns:
 *       A #time_t of the number of seconds since UNIX epoch in UTC.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

time_t
bson_iter_time_t (const bson_iter_t *iter) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DATE_TIME) {
      return bson_iter_time_t_unsafe (iter);
   }

   return 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_timestamp --
 *
 *       Fetches the current field if it is a BSON_TYPE_TIMESTAMP.
 *
 * Parameters:
 *       @iter: A #bson_iter_t.
 *       @timestamp: a location for the timestamp.
 *       @increment: A location for the increment.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @timestamp is initialized.
 *       @increment is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_timestamp (const bson_iter_t *iter,      /* IN */
                     uint32_t          *timestamp, /* OUT */
                     uint32_t          *increment) /* OUT */
{
   uint64_t encoded;
   uint32_t ret_timestamp = 0;
   uint32_t ret_increment = 0;

   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_TIMESTAMP) {
      memcpy (&encoded, iter->raw + iter->d1, sizeof (encoded));
      encoded = BSON_UINT64_FROM_LE (encoded);
      ret_timestamp = (encoded >> 32) & 0xFFFFFFFF;
      ret_increment = encoded & 0xFFFFFFFF;
   }

   if (timestamp) {
      *timestamp = ret_timestamp;
   }

   if (increment) {
      *increment = ret_increment;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_timeval --
 *
 *       Retrieves the current field of type BSON_TYPE_DATE_TIME and stores
 *       it into the struct timeval provided. tv->tv_sec is set to the
 *       number of seconds since the UNIX epoch in UTC.
 *
 *       Since BSON_TYPE_DATE_TIME does not support fractions of a second,
 *       tv->tv_usec will always be set to zero.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @tv is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_timeval (const bson_iter_t *iter,  /* IN */
                   struct timeval    *tv)    /* OUT */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DATE_TIME) {
      bson_iter_timeval_unsafe (iter, tv);
      return;
   }

   memset (tv, 0, sizeof *tv);
}


/**
 * bson_iter_document:
 * @iter: a bson_iter_t.
 * @document_len: A location for the document length.
 * @document: A location for a pointer to the document buffer.
 *
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_document --
 *
 *       Retrieves the data to the document BSON structure and stores the
 *       length of the document buffer in @document_len and the document
 *       buffer in @document.
 *
 *       If you would like to iterate over the child contents, you might
 *       consider creating a bson_t on the stack such as the following. It
 *       allows you to call functions taking a const bson_t* only.
 *
 *          bson_t b;
 *          uint32_t len;
 *          const uint8_t *data;
 *
 *          bson_iter_document(iter, &len, &data);
 *
 *          if (bson_init_static (&b, data, len)) {
 *             ...
 *          }
 *
 *       There is no need to cleanup the bson_t structure as no data can be
 *       modified in the process of its use (as it is static/const).
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @document_len is initialized.
 *       @document is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_document (const bson_iter_t  *iter,         /* IN */
                    uint32_t           *document_len, /* OUT */
                    const uint8_t     **document)     /* OUT */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (document_len);
   BSON_ASSERT (document);

   *document = NULL;
   *document_len = 0;

   if (ITER_TYPE (iter) == BSON_TYPE_DOCUMENT) {
      memcpy (document_len, (iter->raw + iter->d1), sizeof (*document_len));
      *document_len = BSON_UINT32_FROM_LE (*document_len);
      *document = (iter->raw + iter->d1);
   }
}


/**
 * bson_iter_array:
 * @iter: a #bson_iter_t.
 * @array_len: A location for the array length.
 * @array: A location for a pointer to the array buffer.
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_array --
 *
 *       Retrieves the data to the array BSON structure and stores the
 *       length of the array buffer in @array_len and the array buffer in
 *       @array.
 *
 *       If you would like to iterate over the child contents, you might
 *       consider creating a bson_t on the stack such as the following. It
 *       allows you to call functions taking a const bson_t* only.
 *
 *          bson_t b;
 *          uint32_t len;
 *          const uint8_t *data;
 *
 *          bson_iter_array (iter, &len, &data);
 *
 *          if (bson_init_static (&b, data, len)) {
 *             ...
 *          }
 *
 *       There is no need to cleanup the #bson_t structure as no data can be
 *       modified in the process of its use.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @array_len is initialized.
 *       @array is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_array (const bson_iter_t  *iter,      /* IN */
                 uint32_t           *array_len, /* OUT */
                 const uint8_t     **array)     /* OUT */
{
   BSON_ASSERT (iter);
   BSON_ASSERT (array_len);
   BSON_ASSERT (array);

   *array = NULL;
   *array_len = 0;

   if (ITER_TYPE (iter) == BSON_TYPE_ARRAY) {
      memcpy (array_len, (iter->raw + iter->d1), sizeof (*array_len));
      *array_len = BSON_UINT32_FROM_LE (*array_len);
      *array = (iter->raw + iter->d1);
   }
}


#define VISIT_FIELD(name) visitor->visit_##name && visitor->visit_##name
#define VISIT_AFTER VISIT_FIELD (after)
#define VISIT_BEFORE VISIT_FIELD (before)
#define VISIT_CORRUPT if (visitor->visit_corrupt) visitor->visit_corrupt
#define VISIT_DOUBLE VISIT_FIELD (double)
#define VISIT_UTF8 VISIT_FIELD (utf8)
#define VISIT_DOCUMENT VISIT_FIELD (document)
#define VISIT_ARRAY VISIT_FIELD (array)
#define VISIT_BINARY VISIT_FIELD (binary)
#define VISIT_UNDEFINED VISIT_FIELD (undefined)
#define VISIT_OID VISIT_FIELD (oid)
#define VISIT_BOOL VISIT_FIELD (bool)
#define VISIT_DATE_TIME VISIT_FIELD (date_time)
#define VISIT_NULL VISIT_FIELD (null)
#define VISIT_REGEX VISIT_FIELD (regex)
#define VISIT_DBPOINTER VISIT_FIELD (dbpointer)
#define VISIT_CODE VISIT_FIELD (code)
#define VISIT_SYMBOL VISIT_FIELD (symbol)
#define VISIT_CODEWSCOPE VISIT_FIELD (codewscope)
#define VISIT_INT32 VISIT_FIELD (int32)
#define VISIT_TIMESTAMP VISIT_FIELD (timestamp)
#define VISIT_INT64 VISIT_FIELD (int64)
#define VISIT_MAXKEY VISIT_FIELD (maxkey)
#define VISIT_MINKEY VISIT_FIELD (minkey)


/**
 * bson_iter_visit_all:
 * @iter: A #bson_iter_t.
 * @visitor: A #bson_visitor_t containing the visitors.
 * @data: User data for @visitor data parameters.
 *
 *
 * Returns: true if the visitor was pre-maturely ended; otherwise false.
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_visit_all --
 *
 *       Visits all fields forward from the current position of @iter. For
 *       each field found a function in @visitor will be called. Typically
 *       you will use this immediately after initializing a bson_iter_t.
 *
 *          bson_iter_init (&iter, b);
 *          bson_iter_visit_all (&iter, my_visitor, NULL);
 *
 *       @iter will no longer be valid after this function has executed and
 *       will need to be reinitialized if intending to reuse.
 *
 * Returns:
 *       true if successfully visited all fields or callback requested
 *       early termination, otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_iter_visit_all (bson_iter_t          *iter,    /* INOUT */
                     const bson_visitor_t *visitor, /* IN */
                     void                 *data)    /* IN */
{
   const char *key;

   BSON_ASSERT (iter);
   BSON_ASSERT (visitor);

   while (bson_iter_next (iter)) {
      key = bson_iter_key_unsafe (iter);

      if (*key && !bson_utf8_validate (key, strlen (key), false)) {
         iter->err_off = iter->off;
         return true;
      }

      if (VISIT_BEFORE (iter, key, data)) {
         return true;
      }

      switch (bson_iter_type (iter)) {
      case BSON_TYPE_DOUBLE:

         if (VISIT_DOUBLE (iter, key, bson_iter_double (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_UTF8:
         {
            uint32_t utf8_len;
            const char *utf8;

            utf8 = bson_iter_utf8 (iter, &utf8_len);

            if (!bson_utf8_validate (utf8, utf8_len, true)) {
               iter->err_off = iter->off;
               return true;
            }

            if (VISIT_UTF8 (iter, key, utf8_len, utf8, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_DOCUMENT:
         {
            const uint8_t *docbuf = NULL;
            uint32_t doclen = 0;
            bson_t b;

            bson_iter_document (iter, &doclen, &docbuf);

            if (bson_init_static (&b, docbuf, doclen) &&
                VISIT_DOCUMENT (iter, key, &b, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_ARRAY:
         {
            const uint8_t *docbuf = NULL;
            uint32_t doclen = 0;
            bson_t b;

            bson_iter_array (iter, &doclen, &docbuf);

            if (bson_init_static (&b, docbuf, doclen)
                && VISIT_ARRAY (iter, key, &b, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_BINARY:
         {
            const uint8_t *binary = NULL;
            bson_subtype_t subtype = BSON_SUBTYPE_BINARY;
            uint32_t binary_len = 0;

            bson_iter_binary (iter, &subtype, &binary_len, &binary);

            if (VISIT_BINARY (iter, key, subtype, binary_len, binary, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_UNDEFINED:

         if (VISIT_UNDEFINED (iter, key, data)) {
            return true;
         }

         break;
      case BSON_TYPE_OID:

         if (VISIT_OID (iter, key, bson_iter_oid (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_BOOL:

         if (VISIT_BOOL (iter, key, bson_iter_bool (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_DATE_TIME:

         if (VISIT_DATE_TIME (iter, key, bson_iter_date_time (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_NULL:

         if (VISIT_NULL (iter, key, data)) {
            return true;
         }

         break;
      case BSON_TYPE_REGEX:
         {
            const char *regex = NULL;
            const char *options = NULL;
            regex = bson_iter_regex (iter, &options);

            if (VISIT_REGEX (iter, key, regex, options, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_DBPOINTER:
         {
            uint32_t collection_len = 0;
            const char *collection = NULL;
            const bson_oid_t *oid = NULL;

            bson_iter_dbpointer (iter, &collection_len, &collection, &oid);

            if (VISIT_DBPOINTER (iter, key, collection_len, collection, oid,
                                 data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_CODE:
         {
            uint32_t code_len;
            const char *code;

            code = bson_iter_code (iter, &code_len);

            if (VISIT_CODE (iter, key, code_len, code, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_SYMBOL:
         {
            uint32_t symbol_len;
            const char *symbol;

            symbol = bson_iter_symbol (iter, &symbol_len);

            if (VISIT_SYMBOL (iter, key, symbol_len, symbol, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_CODEWSCOPE:
         {
            uint32_t length = 0;
            const char *code;
            const uint8_t *docbuf = NULL;
            uint32_t doclen = 0;
            bson_t b;

            code = bson_iter_codewscope (iter, &length, &doclen, &docbuf);

            if (bson_init_static (&b, docbuf, doclen) &&
                VISIT_CODEWSCOPE (iter, key, length, code, &b, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_INT32:

         if (VISIT_INT32 (iter, key, bson_iter_int32 (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_TIMESTAMP:
         {
            uint32_t timestamp;
            uint32_t increment;
            bson_iter_timestamp (iter, &timestamp, &increment);

            if (VISIT_TIMESTAMP (iter, key, timestamp, increment, data)) {
               return true;
            }
         }
         break;
      case BSON_TYPE_INT64:

         if (VISIT_INT64 (iter, key, bson_iter_int64 (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_MAXKEY:

         if (VISIT_MAXKEY (iter, bson_iter_key_unsafe (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_MINKEY:

         if (VISIT_MINKEY (iter, bson_iter_key_unsafe (iter), data)) {
            return true;
         }

         break;
      case BSON_TYPE_EOD:
      default:
         break;
      }

      if (VISIT_AFTER (iter, bson_iter_key_unsafe (iter), data)) {
         return true;
      }
   }

   if (iter->err_off) {
      VISIT_CORRUPT (iter, data);
   }

#undef VISIT_FIELD

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_bool --
 *
 *       Overwrites the current BSON_TYPE_BOOLEAN field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_bool (bson_iter_t *iter,  /* IN */
                          bool         value) /* IN */
{
   BSON_ASSERT (iter);
   value = !!value;

   if (ITER_TYPE (iter) == BSON_TYPE_BOOL) {
      memcpy ((void *)(iter->raw + iter->d1), &value, 1);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_int32 --
 *
 *       Overwrites the current BSON_TYPE_INT32 field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_int32 (bson_iter_t *iter,  /* IN */
                           int32_t      value) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_INT32) {
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
      value = BSON_UINT32_TO_LE (value);
#endif
      memcpy ((void *)(iter->raw + iter->d1), &value, sizeof (value));
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_int64 --
 *
 *       Overwrites the current BSON_TYPE_INT64 field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_int64 (bson_iter_t *iter,   /* IN */
                           int64_t      value)  /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_INT64) {
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
      value = BSON_UINT64_TO_LE (value);
#endif
      memcpy ((void *)(iter->raw + iter->d1), &value, sizeof (value));
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_overwrite_double --
 *
 *       Overwrites the current BSON_TYPE_DOUBLE field with a new value.
 *       This is performed in-place and therefore no keys are moved.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_iter_overwrite_double (bson_iter_t *iter,  /* IN */
                            double       value) /* IN */
{
   BSON_ASSERT (iter);

   if (ITER_TYPE (iter) == BSON_TYPE_DOUBLE) {
      value = BSON_DOUBLE_TO_LE (value);
      memcpy ((void *)(iter->raw + iter->d1), &value, sizeof (value));
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_iter_value --
 *
 *       Retrieves a bson_value_t containing the boxed value of the current
 *       element. The result of this function valid until the state of
 *       iter has been changed (through the use of bson_iter_next()).
 *
 * Returns:
 *       A bson_value_t that should not be modified or freed. If you need
 *       to hold on to the value, use bson_value_copy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_value_t *
bson_iter_value (bson_iter_t *iter) /* IN */
{
   bson_value_t *value;

   BSON_ASSERT (iter);

   value = &iter->value;
   value->value_type = ITER_TYPE (iter);

   switch (value->value_type) {
   case BSON_TYPE_DOUBLE:
      value->value.v_double = bson_iter_double (iter);
      break;
   case BSON_TYPE_UTF8:
      value->value.v_utf8.str =
         (char *)bson_iter_utf8 (iter, &value->value.v_utf8.len);
      break;
   case BSON_TYPE_DOCUMENT:
      bson_iter_document (iter,
                          &value->value.v_doc.data_len,
                          (const uint8_t **)&value->value.v_doc.data);
      break;
   case BSON_TYPE_ARRAY:
      bson_iter_array (iter,
                       &value->value.v_doc.data_len,
                       (const uint8_t **)&value->value.v_doc.data);
      break;
   case BSON_TYPE_BINARY:
      bson_iter_binary (iter,
                        &value->value.v_binary.subtype,
                        &value->value.v_binary.data_len,
                        (const uint8_t **)&value->value.v_binary.data);
      break;
   case BSON_TYPE_OID:
      bson_oid_copy (bson_iter_oid (iter), &value->value.v_oid);
      break;
   case BSON_TYPE_BOOL:
      value->value.v_bool = bson_iter_bool (iter);
      break;
   case BSON_TYPE_DATE_TIME:
      value->value.v_datetime = bson_iter_date_time (iter);
      break;
   case BSON_TYPE_REGEX:
      value->value.v_regex.regex = (char *)bson_iter_regex (
            iter,
            (const char **)&value->value.v_regex.options);
      break;
   case BSON_TYPE_DBPOINTER: {
      const bson_oid_t *oid;

      bson_iter_dbpointer (iter,
                           &value->value.v_dbpointer.collection_len,
                           (const char **)&value->value.v_dbpointer.collection,
                           &oid);
      bson_oid_copy (oid, &value->value.v_dbpointer.oid);
      break;
   }
   case BSON_TYPE_CODE:
      value->value.v_code.code =
         (char *)bson_iter_code (
            iter,
            &value->value.v_code.code_len);
      break;
   case BSON_TYPE_SYMBOL:
      value->value.v_symbol.symbol =
         (char *)bson_iter_symbol (
            iter,
            &value->value.v_symbol.len);
      break;
   case BSON_TYPE_CODEWSCOPE:
      value->value.v_codewscope.code =
         (char *)bson_iter_codewscope (
            iter,
            &value->value.v_codewscope.code_len,
            &value->value.v_codewscope.scope_len,
            (const uint8_t **)&value->value.v_codewscope.scope_data);
      break;
   case BSON_TYPE_INT32:
      value->value.v_int32 = bson_iter_int32 (iter);
      break;
   case BSON_TYPE_TIMESTAMP:
      bson_iter_timestamp (iter,
                           &value->value.v_timestamp.timestamp,
                           &value->value.v_timestamp.increment);
      break;
   case BSON_TYPE_INT64:
      value->value.v_int64 = bson_iter_int64 (iter);
      break;
   case BSON_TYPE_NULL:
   case BSON_TYPE_UNDEFINED:
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
      break;
   case BSON_TYPE_EOD:
   default:
      return NULL;
   }

   return value;
}

/*==============================================================*/
/* --- bson-json.c */

#define STACK_MAX 100
#define BSON_JSON_DEFAULT_BUF_SIZE (1 << 14)


typedef enum
{
   BSON_JSON_REGULAR,
   BSON_JSON_DONE,
   BSON_JSON_ERROR,
   BSON_JSON_IN_START_MAP,
   BSON_JSON_IN_BSON_TYPE,
   BSON_JSON_IN_BSON_TYPE_DATE_NUMBERLONG,
   BSON_JSON_IN_BSON_TYPE_DATE_ENDMAP,
   BSON_JSON_IN_BSON_TYPE_TIMESTAMP_STARTMAP,
   BSON_JSON_IN_BSON_TYPE_TIMESTAMP_VALUES,
   BSON_JSON_IN_BSON_TYPE_TIMESTAMP_ENDMAP,
} bson_json_read_state_t;


typedef enum
{
   BSON_JSON_LF_REGEX,
   BSON_JSON_LF_OPTIONS,
   BSON_JSON_LF_OID,
   BSON_JSON_LF_BINARY,
   BSON_JSON_LF_TYPE,
   BSON_JSON_LF_DATE,
   BSON_JSON_LF_TIMESTAMP_T,
   BSON_JSON_LF_TIMESTAMP_I,
   BSON_JSON_LF_UNDEFINED,
   BSON_JSON_LF_MINKEY,
   BSON_JSON_LF_MAXKEY,
   BSON_JSON_LF_INT64,
} bson_json_read_bson_state_t;


typedef struct
{
   uint8_t *buf;
   size_t   n_bytes;
   size_t   len;
} bson_json_buf_t;


typedef struct
{
   int    i;
   bool   is_array;
   bson_t bson;
} bson_json_stack_frame_t;


typedef union
{
   struct {
      bool has_regex;
      bool has_options;
   } regex;
   struct {
      bool       has_oid;
      bson_oid_t oid;
   } oid;
   struct {
      bool           has_binary;
      bool           has_subtype;
      bson_subtype_t type;
   } binary;
   struct {
      bool    has_date;
      int64_t date;
   } date;
   struct {
      bool     has_t;
      bool     has_i;
      uint32_t t;
      uint32_t i;
   } timestamp;
   struct {
      bool has_undefined;
   } undefined;
   struct {
      bool has_minkey;
   } minkey;
   struct {
      bool has_maxkey;
   } maxkey;
   struct {
      int64_t value;
   } v_int64;
} bson_json_bson_data_t;


typedef struct
{
   bson_t                      *bson;
   bson_json_stack_frame_t      stack[STACK_MAX];
   int                          n;
   const char                  *key;
   bson_json_buf_t              key_buf;
   bson_json_read_state_t       read_state;
   bson_json_read_bson_state_t  bson_state;
   bson_type_t                  bson_type;
   bson_json_buf_t              bson_type_buf [3];
   bson_json_bson_data_t        bson_type_data;
   bool                         known_bson_type;
} bson_json_reader_bson_t;


typedef struct
{
   void                 *data;
   bson_json_reader_cb   cb;
   bson_json_destroy_cb  dcb;
   uint8_t              *buf;
   size_t                buf_size;
   size_t                bytes_read;
   size_t                bytes_parsed;
   bool                  all_whitespace;
} bson_json_reader_producer_t;


struct _bson_json_reader_t
{
   bson_json_reader_producer_t  producer;
   bson_json_reader_bson_t      bson;
   yajl_handle                  yh;
   bson_error_t                *error;
};


typedef struct
{
   int fd;
   bool do_close;
} bson_json_reader_handle_fd_t;


static void *
bson_yajl_malloc_func (void   *ctx,
                       size_t  sz)
{
   return bson_malloc (sz);
}


static void
bson_yajl_free_func (void *ctx,
                     void *ptr)
{
   bson_free (ptr);
}


static void *
bson_yajl_realloc_func (void   *ctx,
                        void   *ptr,
                        size_t  sz)
{
   return bson_realloc (ptr, sz);
}


static yajl_alloc_funcs gYajlAllocFuncs = {
   bson_yajl_malloc_func,
   bson_yajl_realloc_func,
   bson_yajl_free_func,
};


static void
_noop (void)
{
}

#define STACK_ELE(_delta, _name) (bson->stack[(_delta) + bson->n]._name)
#define STACK_BSON(_delta) \
      (((_delta) + bson->n) == 0 ? bson->bson : &STACK_ELE (_delta, bson))
#define STACK_BSON_PARENT STACK_BSON (-1)
#define STACK_BSON_CHILD STACK_BSON (0)
#define STACK_I STACK_ELE (0, i)
#define STACK_IS_ARRAY STACK_ELE (0, is_array)
#define STACK_PUSH_ARRAY(statement) \
   do { \
      if (bson->n >= (STACK_MAX - 1)) { return 0; } \
      bson->n++; \
      STACK_I = 0; \
      STACK_IS_ARRAY = 1; \
      if (bson->n != 0) { \
         statement; \
      } \
   } while (0)
#define STACK_PUSH_DOC(statement) \
   do { \
      if (bson->n >= (STACK_MAX - 1)) { return 0; } \
      bson->n++; \
      STACK_IS_ARRAY = 0; \
      if (bson->n != 0) { \
         statement; \
      } \
   } while (0)
#define STACK_POP_ARRAY(statement) \
   do { \
      if (!STACK_IS_ARRAY) { return 0; } \
      if (bson->n < 0) { return 0; } \
      if (bson->n > 0) { \
         statement; \
      } \
      bson->n--; \
   } while (0)
#define STACK_POP_DOC(statement) \
   do { \
      if (STACK_IS_ARRAY) { return 0; } \
      if (bson->n < 0) { return 0; } \
      if (bson->n > 0) { \
         statement; \
      } \
      bson->n--; \
   } while (0)
#define BASIC_YAJL_CB_PREAMBLE \
   const char *key; \
   size_t len; \
   bson_json_reader_t *reader = (bson_json_reader_t *)_ctx; \
   bson_json_reader_bson_t *bson = &reader->bson; \
   _bson_json_read_fixup_key (bson); \
   key = bson->key; \
   len = bson->key_buf.len;
#define BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL(_type) \
   if (bson->read_state != BSON_JSON_REGULAR) { \
      _bson_json_read_set_error (reader, "Invalid read of %s in state %d", \
                                 (_type), bson->read_state); \
      return 0; \
   } else if (! key) { \
      _bson_json_read_set_error (reader, "Invalid read of %s without key in state %d", \
                                 (_type), bson->read_state); \
      return 0; \
   }
#define HANDLE_OPTION(_key, _type, _state) \
   (len == strlen (_key) && strncmp ((const char *)val, (_key), len) == 0) { \
      if (bson->known_bson_type && bson->bson_type != (_type)) { \
         _bson_json_read_set_error (reader, \
                                    "Invalid key %s.  Looking for values for %d", \
                                    (_key), bson->bson_type); \
         return 0; \
      } \
      bson->bson_type = (_type); \
      bson->bson_state = (_state); \
   }

static bool
_bson_json_all_whitespace (const char *utf8)
{
   bool all_whitespace = true;

   if (utf8) {
      for (; *utf8; utf8 = bson_utf8_next_char (utf8)) {
         if (!isspace (bson_utf8_get_char (utf8))) {
            all_whitespace = false;
            break;
         }
      }
   }

   return all_whitespace;
}

static void
_bson_json_read_set_error (bson_json_reader_t *reader,
                           const char         *fmt,
                           ...)
   BSON_GNUC_PRINTF (2, 3);


static void
_bson_json_read_set_error (bson_json_reader_t *reader, /* IN */
                           const char         *fmt,    /* IN */
                           ...)
{
   va_list ap;

   if (reader->error) {
      reader->error->domain = BSON_ERROR_JSON;
      reader->error->code = BSON_JSON_ERROR_READ_INVALID_PARAM;
      va_start (ap, fmt);
      bson_vsnprintf (reader->error->message, sizeof reader->error->message,
                      fmt, ap);
      va_end (ap);
      reader->error->message [sizeof reader->error->message - 1] = '\0';
   }

   reader->bson.read_state = BSON_JSON_ERROR;
}


static void
_bson_json_buf_ensure (bson_json_buf_t *buf, /* IN */
                       size_t           len) /* IN */
{
   if (buf->n_bytes < len) {
      bson_free (buf->buf);

      buf->n_bytes = bson_next_power_of_two (len);
      buf->buf = bson_malloc (buf->n_bytes);
   }
}


static void
_bson_json_read_fixup_key (bson_json_reader_bson_t *bson) /* IN */
{
   if (bson->n >= 0 && STACK_IS_ARRAY) {
      _bson_json_buf_ensure (&bson->key_buf, 12);
      bson->key_buf.len = bson_uint32_to_string (STACK_I, &bson->key,
                                                 (char *)bson->key_buf.buf, 12);
      STACK_I++;
   }
}


static void
_bson_json_buf_set (bson_json_buf_t *buf,            /* IN */
                    const void       *from,          /* IN */
                    size_t            len,           /* IN */
                    bool              trailing_null) /* IN */
{
   if (trailing_null) {
      _bson_json_buf_ensure (buf, len + 1);
   } else {
      _bson_json_buf_ensure (buf, len);
   }

   memcpy (buf->buf, from, len);

   if (trailing_null) {
      buf->buf[len] = '\0';
   }

   buf->len = len;
}


static int
_bson_json_read_null (void *_ctx)
{
   BASIC_YAJL_CB_PREAMBLE;
   BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL ("null");

   bson_append_null (STACK_BSON_CHILD, key, (int)len);

   return 1;
}


static int
_bson_json_read_boolean (void *_ctx, /* IN */
                         int   val)  /* IN */
{
   BASIC_YAJL_CB_PREAMBLE;

   if (bson->read_state == BSON_JSON_IN_BSON_TYPE && bson->bson_state ==
       BSON_JSON_LF_UNDEFINED) {
      bson->bson_type_data.undefined.has_undefined = true;
      return 1;
   }

   BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL ("boolean");

   bson_append_bool (STACK_BSON_CHILD, key, (int)len, val);

   return 1;
}


static int
_bson_json_read_integer (void    *_ctx, /* IN */
                         int64_t  val)  /* IN */
{
   bson_json_read_state_t rs;
   bson_json_read_bson_state_t bs;

   BASIC_YAJL_CB_PREAMBLE;

   rs = bson->read_state;
   bs = bson->bson_state;

   if (rs == BSON_JSON_REGULAR) {
      BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL ("integer");

      if (val <= INT32_MAX) {
         bson_append_int32 (STACK_BSON_CHILD, key, (int)len, (int)val);
      } else {
         bson_append_int64 (STACK_BSON_CHILD, key, (int)len, val);
      }
   } else if (rs == BSON_JSON_IN_BSON_TYPE || rs ==
              BSON_JSON_IN_BSON_TYPE_TIMESTAMP_VALUES) {
      switch (bs) {
      case BSON_JSON_LF_DATE:
         bson->bson_type_data.date.has_date = true;
         bson->bson_type_data.date.date = val;
         break;
      case BSON_JSON_LF_TIMESTAMP_T:
         bson->bson_type_data.timestamp.has_t = true;
         bson->bson_type_data.timestamp.t = (uint32_t)val;
         break;
      case BSON_JSON_LF_TIMESTAMP_I:
         bson->bson_type_data.timestamp.has_i = true;
         bson->bson_type_data.timestamp.i = (uint32_t)val;
         break;
      case BSON_JSON_LF_MINKEY:
         bson->bson_type_data.minkey.has_minkey = true;
         break;
      case BSON_JSON_LF_MAXKEY:
         bson->bson_type_data.maxkey.has_maxkey = true;
         break;
      case BSON_JSON_LF_REGEX:
      case BSON_JSON_LF_OPTIONS:
      case BSON_JSON_LF_OID:
      case BSON_JSON_LF_BINARY:
      case BSON_JSON_LF_TYPE:
      case BSON_JSON_LF_UNDEFINED:
      case BSON_JSON_LF_INT64:
      default:
         _bson_json_read_set_error (reader,
                                    "Invalid special type for integer read %d",
                                    bs);
         return 0;
      }
   } else {
      _bson_json_read_set_error (reader, "Invalid state for integer read %d",
                                 rs);
      return 0;
   }

   return 1;
}


static int
_bson_json_read_double (void   *_ctx, /* IN */
                        double  val)  /* IN */
{
   BASIC_YAJL_CB_PREAMBLE;
   BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL ("double");

   bson_append_double (STACK_BSON_CHILD, key, (int)len, val);

   return 1;
}


static int
_bson_json_read_string (void                *_ctx, /* IN */
                        const unsigned char *val,  /* IN */
                        size_t               vlen) /* IN */
{
   bson_json_read_state_t rs;
   bson_json_read_bson_state_t bs;

   BASIC_YAJL_CB_PREAMBLE;

   rs = bson->read_state;
   bs = bson->bson_state;

   if (rs == BSON_JSON_REGULAR) {
      BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL ("string");
      bson_append_utf8 (STACK_BSON_CHILD, key, (int)len, (const char *)val, (int)vlen);
   } else if (rs == BSON_JSON_IN_BSON_TYPE || rs ==
              BSON_JSON_IN_BSON_TYPE_TIMESTAMP_VALUES || rs == BSON_JSON_IN_BSON_TYPE_DATE_NUMBERLONG) {
      const char *val_w_null;
      _bson_json_buf_set (&bson->bson_type_buf[2], val, vlen, true);
      val_w_null = (const char *)bson->bson_type_buf[2].buf;

      switch (bs) {
      case BSON_JSON_LF_REGEX:
         bson->bson_type_data.regex.has_regex = true;
         _bson_json_buf_set (&bson->bson_type_buf[0], val, vlen, true);
         break;
      case BSON_JSON_LF_OPTIONS:
         bson->bson_type_data.regex.has_options = true;
         _bson_json_buf_set (&bson->bson_type_buf[1], val, vlen, true);
         break;
      case BSON_JSON_LF_OID:

         if (vlen != 24) {
            goto BAD_PARSE;
         }

         bson->bson_type_data.oid.has_oid = true;
         bson_oid_init_from_string (&bson->bson_type_data.oid.oid, val_w_null);
         break;
      case BSON_JSON_LF_TYPE:
         bson->bson_type_data.binary.has_subtype = true;

#ifdef _MSC_VER
# define SSCANF sscanf_s
#else
# define SSCANF sscanf
#endif

         if (SSCANF (val_w_null, "%02x",
                     &bson->bson_type_data.binary.type) != 1) {
            goto BAD_PARSE;
         }

#undef SSCANF

         break;
      case BSON_JSON_LF_BINARY: {
            /* TODO: error handling for pton */
            int binary_len;
            bson->bson_type_data.binary.has_binary = true;
            binary_len = b64_pton (val_w_null, NULL, 0);
            _bson_json_buf_ensure (&bson->bson_type_buf[0], binary_len + 1);
            b64_pton ((char *)bson->bson_type_buf[2].buf,
                      bson->bson_type_buf[0].buf, binary_len + 1);
            bson->bson_type_buf[0].len = binary_len;
            break;
         }
      case BSON_JSON_LF_INT64:
         {
            int64_t v64;
            char *endptr = NULL;

            errno = 0;
            v64 = bson_ascii_strtoll ((const char *)val, &endptr, 10);

            if (((v64 == INT64_MIN) || (v64 == INT64_MAX)) && (errno == ERANGE)) {
               goto BAD_PARSE;
            }

            if (endptr != ((const char *)val + vlen)) {
               goto BAD_PARSE;
            }

            if (bson->read_state == BSON_JSON_IN_BSON_TYPE) {
                bson->bson_type_data.v_int64.value = v64;
            } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_DATE_NUMBERLONG) {
                bson->bson_type_data.date.has_date = true;
                bson->bson_type_data.date.date = v64;
            } else {
                goto BAD_PARSE;
            }
         }
         break;
      case BSON_JSON_LF_DATE:
         {
            int64_t v64;

            if (!_bson_iso8601_date_parse ((char *)val, (int)vlen, &v64)) {
               _bson_json_read_set_error (reader, "Could not parse \"%s\" as a date",
                                          val_w_null);
               return 0;
            } else {
               bson->bson_type_data.date.has_date = true;
               bson->bson_type_data.date.date = v64;
            }
         }
         break;
      case BSON_JSON_LF_TIMESTAMP_T:
      case BSON_JSON_LF_TIMESTAMP_I:
      case BSON_JSON_LF_UNDEFINED:
      case BSON_JSON_LF_MINKEY:
      case BSON_JSON_LF_MAXKEY:
      default:
         goto BAD_PARSE;
      }

      return 1;

   BAD_PARSE:
      _bson_json_read_set_error (reader,
                                 "Invalid input string %s, looking for %d",
                                 val_w_null, bs);
      return 0;
   } else {
      _bson_json_read_set_error (reader, "Invalid state to look for string %d",
                                 rs);
      return 0;
   }

   return 1;
}


static int
_bson_json_read_start_map (void *_ctx) /* IN */
{
   BASIC_YAJL_CB_PREAMBLE;

   if (bson->read_state == BSON_JSON_IN_BSON_TYPE && bson->bson_state == BSON_JSON_LF_DATE) {
      bson->read_state = BSON_JSON_IN_BSON_TYPE_DATE_NUMBERLONG;
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_TIMESTAMP_STARTMAP) {
      bson->read_state = BSON_JSON_IN_BSON_TYPE_TIMESTAMP_VALUES;
   } else {
      bson->read_state = BSON_JSON_IN_START_MAP;
   }

   /* silence some warnings */
   (void)len;
   (void)key;

   return 1;
}


static bool
_is_known_key (const char *key, size_t len)
{
   bool ret;

#define IS_KEY(k) (len == strlen(k) && (0 == memcmp (k, key, len)))

   ret = (IS_KEY ("$regex") ||
          IS_KEY ("$options") ||
          IS_KEY ("$oid") ||
          IS_KEY ("$binary") ||
          IS_KEY ("$type") ||
          IS_KEY ("$date") ||
          IS_KEY ("$undefined") ||
          IS_KEY ("$maxKey") ||
          IS_KEY ("$minKey") ||
          IS_KEY ("$timestamp") ||
          IS_KEY ("$numberLong"));

#undef IS_KEY

   return ret;
}


static int
_bson_json_read_map_key (void          *_ctx, /* IN */
                         const uint8_t *val,  /* IN */
                         size_t         len)  /* IN */
{
   bson_json_reader_t *reader = (bson_json_reader_t *)_ctx;
   bson_json_reader_bson_t *bson = &reader->bson;

   if (bson->read_state == BSON_JSON_IN_START_MAP) {
      if (len > 0 && val[0] == '$' && _is_known_key ((const char *)val, len)) {
         bson->read_state = BSON_JSON_IN_BSON_TYPE;
         bson->bson_type = (bson_type_t) 0;
         memset (&bson->bson_type_data, 0, sizeof bson->bson_type_data);
      } else {
         bson->read_state = BSON_JSON_REGULAR;
         STACK_PUSH_DOC (bson_append_document_begin (STACK_BSON_PARENT,
                                                     bson->key,
                                                     (int)bson->key_buf.len,
                                                     STACK_BSON_CHILD));
      }
   }

   if (bson->read_state == BSON_JSON_IN_BSON_TYPE) {
      if HANDLE_OPTION ("$regex", BSON_TYPE_REGEX, BSON_JSON_LF_REGEX) else
      if HANDLE_OPTION ("$options", BSON_TYPE_REGEX, BSON_JSON_LF_OPTIONS) else
      if HANDLE_OPTION ("$oid", BSON_TYPE_OID, BSON_JSON_LF_OID) else
      if HANDLE_OPTION ("$binary", BSON_TYPE_BINARY, BSON_JSON_LF_BINARY) else
      if HANDLE_OPTION ("$type", BSON_TYPE_BINARY, BSON_JSON_LF_TYPE) else
      if HANDLE_OPTION ("$date", BSON_TYPE_DATE_TIME, BSON_JSON_LF_DATE) else
      if HANDLE_OPTION ("$undefined", BSON_TYPE_UNDEFINED,
                        BSON_JSON_LF_UNDEFINED) else
      if HANDLE_OPTION ("$minKey", BSON_TYPE_MINKEY, BSON_JSON_LF_MINKEY) else
      if HANDLE_OPTION ("$maxKey", BSON_TYPE_MAXKEY, BSON_JSON_LF_MAXKEY) else
      if HANDLE_OPTION ("$numberLong", BSON_TYPE_INT64, BSON_JSON_LF_INT64) else
      if (len == strlen ("$timestamp") &&
          memcmp (val, "$timestamp", len) == 0) {
         bson->bson_type = BSON_TYPE_TIMESTAMP;
         bson->read_state = BSON_JSON_IN_BSON_TYPE_TIMESTAMP_STARTMAP;
      } else {
         _bson_json_read_set_error (reader,
                                    "Invalid key %s.  Looking for values for %d",
                                    val, bson->bson_type);
         return 0;
      }
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_DATE_NUMBERLONG) {
      if HANDLE_OPTION ("$numberLong", BSON_TYPE_DATE_TIME, BSON_JSON_LF_INT64) else {
         _bson_json_read_set_error (reader,
                                    "Invalid key %s.  Looking for values for %d",
                                    val, bson->bson_type);
         return 0;
      }
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_TIMESTAMP_VALUES) {
      if HANDLE_OPTION ("t", BSON_TYPE_TIMESTAMP, BSON_JSON_LF_TIMESTAMP_T) else
      if HANDLE_OPTION ("i", BSON_TYPE_TIMESTAMP,
                        BSON_JSON_LF_TIMESTAMP_I) else {
         _bson_json_read_set_error (reader,
                                    "Invalid key %s.  Looking for values for %d",
                                    val, bson->bson_type);
         return 0;
      }
   } else {
      _bson_json_buf_set (&bson->key_buf, val, len, true);
      bson->key = (const char *)bson->key_buf.buf;
   }

   return 1;
}


static int
_bson_json_read_append_binary (bson_json_reader_t      *reader, /* IN */
                               bson_json_reader_bson_t *bson)   /* IN */
{
   if (!bson->bson_type_data.binary.has_binary) {
      _bson_json_read_set_error (reader,
                                 "Missing $binary after $type in BSON_TYPE_BINARY");
   } else if (!bson->bson_type_data.binary.has_subtype) {
      _bson_json_read_set_error (reader,
                                 "Missing $type after $binary in BSON_TYPE_BINARY");
   } else {
      return bson_append_binary (STACK_BSON_CHILD, bson->key, (int)bson->key_buf.len,
                                 bson->bson_type_data.binary.type,
                                 bson->bson_type_buf[0].buf,
                                 (uint32_t)bson->bson_type_buf[0].len);
   }

   return 0;
}


static int
_bson_json_read_append_regex (bson_json_reader_t      *reader, /* IN */
                              bson_json_reader_bson_t *bson)   /* IN */
{
   char *regex = NULL;
   char *options = NULL;

   if (!bson->bson_type_data.regex.has_regex) {
      _bson_json_read_set_error (reader,
                                 "Missing $regex after $options in BSON_TYPE_REGEX");
      return 0;
   }

   regex = (char *)bson->bson_type_buf[0].buf;

   if (bson->bson_type_data.regex.has_options) {
      options = (char *)bson->bson_type_buf[1].buf;
   }

   return bson_append_regex (STACK_BSON_CHILD, bson->key, (int)bson->key_buf.len,
                             regex, options);
}


static int
_bson_json_read_append_oid (bson_json_reader_t      *reader, /* IN */
                            bson_json_reader_bson_t *bson)   /* IN */
{
   return bson_append_oid (STACK_BSON_CHILD, bson->key, (int)bson->key_buf.len,
                           &bson->bson_type_data.oid.oid);
}


static int
_bson_json_read_append_date_time (bson_json_reader_t      *reader, /* IN */
                                  bson_json_reader_bson_t *bson)   /* IN */
{
   return bson_append_date_time (STACK_BSON_CHILD, bson->key, (int)bson->key_buf.len,
                                 bson->bson_type_data.date.date);
}


static int
_bson_json_read_append_timestamp (bson_json_reader_t      *reader, /* IN */
                                  bson_json_reader_bson_t *bson)   /* IN */
{
   if (!bson->bson_type_data.timestamp.has_t) {
      _bson_json_read_set_error (reader,
                                 "Missing t after $timestamp in BSON_TYPE_TIMESTAMP");
      return 0;
   }

   if (!bson->bson_type_data.timestamp.has_i) {
      _bson_json_read_set_error (reader,
                                 "Missing i after $timestamp in BSON_TYPE_TIMESTAMP");
      return 0;
   }

   return bson_append_timestamp (STACK_BSON_CHILD, bson->key, (int)bson->key_buf.len,
                                 bson->bson_type_data.timestamp.t,
                                 bson->bson_type_data.timestamp.i);
}


static int
_bson_json_read_end_map (void *_ctx) /* IN */
{
   bson_json_reader_t *reader = (bson_json_reader_t *)_ctx;
   bson_json_reader_bson_t *bson = &reader->bson;

   if (bson->read_state == BSON_JSON_IN_START_MAP) {
      bson->read_state = BSON_JSON_REGULAR;
      STACK_PUSH_DOC (bson_append_document_begin (STACK_BSON_PARENT, bson->key,
                                                  (int)bson->key_buf.len,
                                                  STACK_BSON_CHILD));
   }

   if (bson->read_state == BSON_JSON_IN_BSON_TYPE) {
      bson->read_state = BSON_JSON_REGULAR;
      switch (bson->bson_type) {
      case BSON_TYPE_REGEX:
         return _bson_json_read_append_regex (reader, bson);
      case BSON_TYPE_OID:
         return _bson_json_read_append_oid (reader, bson);
      case BSON_TYPE_BINARY:
         return _bson_json_read_append_binary (reader, bson);
      case BSON_TYPE_DATE_TIME:
         return _bson_json_read_append_date_time (reader, bson);
      case BSON_TYPE_UNDEFINED:
         return bson_append_undefined (STACK_BSON_CHILD, bson->key,
                                       (int)bson->key_buf.len);
      case BSON_TYPE_MINKEY:
         return bson_append_minkey (STACK_BSON_CHILD, bson->key,
                                    (int)bson->key_buf.len);
      case BSON_TYPE_MAXKEY:
         return bson_append_maxkey (STACK_BSON_CHILD, bson->key,
                                    (int)bson->key_buf.len);
      case BSON_TYPE_INT64:
         return bson_append_int64 (STACK_BSON_CHILD, bson->key,
                                   (int)bson->key_buf.len,
                                   bson->bson_type_data.v_int64.value);
      case BSON_TYPE_EOD:
      case BSON_TYPE_DOUBLE:
      case BSON_TYPE_UTF8:
      case BSON_TYPE_DOCUMENT:
      case BSON_TYPE_ARRAY:
      case BSON_TYPE_BOOL:
      case BSON_TYPE_NULL:
      case BSON_TYPE_CODE:
      case BSON_TYPE_SYMBOL:
      case BSON_TYPE_CODEWSCOPE:
      case BSON_TYPE_INT32:
      case BSON_TYPE_TIMESTAMP:
      case BSON_TYPE_DBPOINTER:
      default:
         _bson_json_read_set_error (reader, "Unknown type %d", bson->bson_type);
         return 0;
         break;
      }
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_TIMESTAMP_VALUES) {
      bson->read_state = BSON_JSON_IN_BSON_TYPE_TIMESTAMP_ENDMAP;

      return _bson_json_read_append_timestamp (reader, bson);
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_TIMESTAMP_ENDMAP) {
      bson->read_state = BSON_JSON_REGULAR;
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_DATE_NUMBERLONG) {
      bson->read_state = BSON_JSON_IN_BSON_TYPE_DATE_ENDMAP;

      return _bson_json_read_append_date_time(reader, bson);
   } else if (bson->read_state == BSON_JSON_IN_BSON_TYPE_DATE_ENDMAP) {
      bson->read_state = BSON_JSON_REGULAR;
   } else if (bson->read_state == BSON_JSON_REGULAR) {
      STACK_POP_DOC (bson_append_document_end (STACK_BSON_PARENT,
                                               STACK_BSON_CHILD));

      if (bson->n == -1) {
         bson->read_state = BSON_JSON_DONE;
         return 0;
      }
   } else {
      _bson_json_read_set_error (reader, "Invalid state %d", bson->read_state);
      return 0;
   }

   return 1;
}


static int
_bson_json_read_start_array (void *_ctx) /* IN */
{
   const char *key;
   size_t len;
   bson_json_reader_t *reader = (bson_json_reader_t *)_ctx;
   bson_json_reader_bson_t *bson = &reader->bson;

   if (bson->n < 0) {
      STACK_PUSH_ARRAY (_noop ());
   } else {
      _bson_json_read_fixup_key (bson);
      key = bson->key;
      len = bson->key_buf.len;

      BASIC_YAJL_CB_BAIL_IF_NOT_NORMAL ("[");

      STACK_PUSH_ARRAY (bson_append_array_begin (STACK_BSON_PARENT, key, (int)len,
                                                 STACK_BSON_CHILD));
   }

   return 1;
}


static int
_bson_json_read_end_array (void *_ctx) /* IN */
{
   bson_json_reader_t *reader = (bson_json_reader_t *)_ctx;
   bson_json_reader_bson_t *bson = &reader->bson;

   if (bson->read_state != BSON_JSON_REGULAR) {
      _bson_json_read_set_error (reader, "Invalid read of %s in state %d",
                                 "]", bson->read_state);
      return 0;
   }

   STACK_POP_ARRAY (bson_append_array_end (STACK_BSON_PARENT,
                                           STACK_BSON_CHILD));
   if (bson->n == -1) {
      bson->read_state = BSON_JSON_DONE;
      return 0;
   }

   return 1;
}


static yajl_callbacks read_cbs = {
   _bson_json_read_null,
   _bson_json_read_boolean,
   _bson_json_read_integer,
   _bson_json_read_double,
   NULL,
   _bson_json_read_string,
   _bson_json_read_start_map,
   _bson_json_read_map_key,
   _bson_json_read_end_map,
   _bson_json_read_start_array,
   _bson_json_read_end_array
};


static int
_bson_json_read_parse_error (bson_json_reader_t *reader, /* IN */
                             yajl_status         ys,     /* IN */
                             bson_error_t       *error)  /* OUT */
{
   unsigned char *str;
   int r;
   yajl_handle yh = reader->yh;
   bson_json_reader_bson_t *bson = &reader->bson;
   bson_json_reader_producer_t *p = &reader->producer;

   if (ys == yajl_status_client_canceled) {
      if (bson->read_state == BSON_JSON_DONE) {
         r = 1;
      } else {
         r = -1;
      }
   } else if (p->all_whitespace) {
      r = 0;
   } else {
      if (error) {
         str = yajl_get_error (yh, 1, p->buf + p->bytes_parsed,
                               p->bytes_read - p->bytes_parsed);
         bson_set_error (error,
                         BSON_ERROR_JSON,
                         BSON_JSON_ERROR_READ_CORRUPT_JS,
                         "%s", str);
         yajl_free_error (yh, str);
      }

      r = -1;
   }

   p->bytes_parsed += yajl_get_bytes_consumed (yh);

   yh->stateStack.used = 0;
   yajl_bs_push (yh->stateStack, yajl_state_start);

   return r;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_json_reader_read --
 *
 *       Read the next json document from @reader and write its value
 *       into @bson. @bson will be allocated as part of this process.
 *
 *       @bson MUST be initialized before calling this function as it
 *       will not be initialized automatically. The reasoning for this
 *       is so that you can chain together bson_json_reader_t with
 *       other components like bson_writer_t.
 *
 * Returns:
 *       1 if successful and data was read.
 *       0 if successful and no data was read.
 *       -1 if there was an error and @error is set.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

int
bson_json_reader_read (bson_json_reader_t *reader, /* IN */
                       bson_t             *bson,   /* IN */
                       bson_error_t       *error)  /* OUT */
{
   bson_json_reader_producer_t *p;
   yajl_status ys;
   yajl_handle yh;
   ssize_t r;
   bool read_something = false;
   int ret = 0;

   BSON_ASSERT (reader);
   BSON_ASSERT (bson);

   p = &reader->producer;
   yh = reader->yh;

   reader->bson.bson = bson;
   reader->bson.n = -1;
   reader->bson.read_state = BSON_JSON_REGULAR;
   reader->error = error;
   reader->producer.all_whitespace = true;

   for (;; ) {
      if (!read_something &&
          p->bytes_parsed &&
          (p->bytes_read > p->bytes_parsed)) {
         r = p->bytes_read - p->bytes_parsed;
      } else {
         r = p->cb (p->data, p->buf, p->buf_size - 1);

         if (r > 0) {
            p->bytes_read = r;
            p->bytes_parsed = 0;
            p->buf[r] = '\0';
         }
      }

      if (r < 0) {
         if (error) {
            bson_set_error (error,
                            BSON_ERROR_JSON,
                            BSON_JSON_ERROR_READ_CB_FAILURE,
                            "reader cb failed");
         }
         ret = -1;
         goto cleanup;
      } else if (r == 0) {
         break;
      } else {
         read_something = true;

         if (p->all_whitespace) {
            p->all_whitespace = _bson_json_all_whitespace (
               (char *)(p->buf + p->bytes_parsed));
         }

         ys = yajl_parse (yh, p->buf + p->bytes_parsed, r);

         if (ys != yajl_status_ok) {
            ret = _bson_json_read_parse_error (reader, ys, error);
            goto cleanup;
         }
      }
   }

   if (read_something) {
      ys = yajl_complete_parse (yh);

      if (ys != yajl_status_ok) {
         ret = _bson_json_read_parse_error (reader, ys, error);
         goto cleanup;
      }
   }

cleanup:

   return ret;
}


bson_json_reader_t *
bson_json_reader_new (void                 *data,           /* IN */
                      bson_json_reader_cb   cb,             /* IN */
                      bson_json_destroy_cb  dcb,            /* IN */
                      bool                  allow_multiple, /* IN */
                      size_t                buf_size)       /* IN */
{
   bson_json_reader_t *r;
   bson_json_reader_producer_t *p;

   r = bson_malloc0 (sizeof *r);

   p = &r->producer;

   p->data = data;
   p->cb = cb;
   p->dcb = dcb;
   p->buf_size = buf_size ? buf_size : BSON_JSON_DEFAULT_BUF_SIZE;
   p->buf = bson_malloc (p->buf_size);

   r->yh = yajl_alloc (&read_cbs, &gYajlAllocFuncs, r);

   yajl_config (r->yh,
                yajl_dont_validate_strings |
                (allow_multiple ?  yajl_allow_multiple_values : 0)
                , 1);

   return r;
}


void
bson_json_reader_destroy (bson_json_reader_t *reader) /* IN */
{
   int i;
   bson_json_reader_producer_t *p = &reader->producer;
   bson_json_reader_bson_t *b = &reader->bson;

   if (reader->producer.dcb) {
      reader->producer.dcb (reader->producer.data);
   }

   bson_free (p->buf);
   bson_free (b->key_buf.buf);

   for (i = 0; i < 3; i++) {
      bson_free (b->bson_type_buf[i].buf);
   }

   yajl_free (reader->yh);

   bson_free (reader);
}


typedef struct
{
   const uint8_t *data;
   size_t         len;
   size_t         bytes_parsed;
} bson_json_data_reader_t;


static ssize_t
_bson_json_data_reader_cb (void    *_ctx,
                           uint8_t *buf,
                           size_t   len)
{
   size_t bytes;
   bson_json_data_reader_t *ctx = (bson_json_data_reader_t *)_ctx;

   if (!ctx->data) {
      return -1;
   }

   bytes = BSON_MIN (len, ctx->len - ctx->bytes_parsed);

   memcpy (buf, ctx->data + ctx->bytes_parsed, bytes);

   ctx->bytes_parsed += bytes;

   return bytes;
}


bson_json_reader_t *
bson_json_data_reader_new (bool   allow_multiple, /* IN */
                           size_t size)           /* IN */
{
   bson_json_data_reader_t *dr = bson_malloc0 (sizeof *dr);

   return bson_json_reader_new (dr, &_bson_json_data_reader_cb, &bson_free,
                                allow_multiple, size);
}


void
bson_json_data_reader_ingest (bson_json_reader_t *reader, /* IN */
                              const uint8_t      *data,   /* IN */
                              size_t              len)    /* IN */
{
   bson_json_data_reader_t *ctx =
      (bson_json_data_reader_t *)reader->producer.data;

   ctx->data = data;
   ctx->len = len;
   ctx->bytes_parsed = 0;
}


bson_t *
bson_new_from_json (const uint8_t *data,  /* IN */
                    ssize_t        len,   /* IN */
                    bson_error_t  *error) /* OUT */
{
   bson_json_reader_t *reader;
   bson_t *bson;
   int r;

   BSON_ASSERT (data);

   if (len < 0) {
      len = (ssize_t)strlen ((const char *)data);
   }

   bson = bson_new ();
   reader = bson_json_data_reader_new (false, BSON_JSON_DEFAULT_BUF_SIZE);
   bson_json_data_reader_ingest (reader, data, len);
   r = bson_json_reader_read (reader, bson, error);
   bson_json_reader_destroy (reader);

   if (r != 1) {
      bson_destroy (bson);
      return NULL;
   }

   return bson;
}


bool
bson_init_from_json (bson_t       *bson,  /* OUT */
                     const char   *data,  /* IN */
                     ssize_t       len,   /* IN */
                     bson_error_t *error) /* OUT */
{
   bson_json_reader_t *reader;
   int r;

   BSON_ASSERT (bson);
   BSON_ASSERT (data);

   if (len < 0) {
      len = strlen (data);
   }

   bson_init (bson);

   reader = bson_json_data_reader_new (false, BSON_JSON_DEFAULT_BUF_SIZE);
   bson_json_data_reader_ingest (reader, (const uint8_t *)data, len);
   r = bson_json_reader_read (reader, bson, error);
   bson_json_reader_destroy (reader);

   if (r != 1) {
      bson_destroy (bson);
      return false;
   }

   return true;
}


static void
_bson_json_reader_handle_fd_destroy (void *handle) /* IN */
{
   bson_json_reader_handle_fd_t *fd = handle;

   if (fd) {
      if ((fd->fd != -1) && fd->do_close) {
#ifdef _WIN32
		 _close(fd->fd);
#else
         close (fd->fd);
#endif
      }
      bson_free (fd);
   }
}


static ssize_t
_bson_json_reader_handle_fd_read (void    *handle, /* IN */
                                  uint8_t *buf,    /* IN */
                                  size_t   len)   /* IN */
{
   bson_json_reader_handle_fd_t *fd = handle;
   ssize_t ret = -1;

   if (fd && (fd->fd != -1)) {
   again:
#ifdef BSON_OS_WIN32
      ret = _read (fd->fd, buf, (unsigned int)len);
#else
      ret = read (fd->fd, buf, len);
#endif
      if ((ret == -1) && (errno == EAGAIN)) {
         goto again;
      }
   }

   return ret;
}


bson_json_reader_t *
bson_json_reader_new_from_fd (int fd,                /* IN */
                              bool close_on_destroy) /* IN */
{
   bson_json_reader_handle_fd_t *handle;

   BSON_ASSERT (fd != -1);

   handle = bson_malloc0 (sizeof *handle);
   handle->fd = fd;
   handle->do_close = close_on_destroy;

   return bson_json_reader_new (handle,
                                _bson_json_reader_handle_fd_read,
                                _bson_json_reader_handle_fd_destroy,
                                true,
                                BSON_JSON_DEFAULT_BUF_SIZE);
}


bson_json_reader_t *
bson_json_reader_new_from_file (const char   *path,  /* IN */
                                bson_error_t *error) /* OUT */
{
   char errmsg_buf[BSON_ERROR_BUFFER_SIZE];
   char *errmsg;
   int fd = -1;

   BSON_ASSERT (path);

#ifdef BSON_OS_WIN32
   _sopen_s (&fd, path, (_O_RDONLY | _O_BINARY), _SH_DENYNO, _S_IREAD);
#else
   fd = open (path, O_RDONLY);
#endif

   if (fd == -1) {
      errmsg = bson_strerror_r (errno, errmsg_buf, sizeof errmsg_buf);
      bson_set_error (error,
                      BSON_ERROR_READER,
                      BSON_ERROR_READER_BADFD,
                      "%s", errmsg);
      return NULL;
   }

   return bson_json_reader_new_from_fd (fd, true);
}

/*==============================================================*/
/* --- bson-keys.c */

static const char * gUint32Strs[] = {
   "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
   "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
   "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34",
   "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45",
   "46", "47", "48", "49", "50", "51", "52", "53", "54", "55", "56",
   "57", "58", "59", "60", "61", "62", "63", "64", "65", "66", "67",
   "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78",
   "79", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
   "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "100",
   "101", "102", "103", "104", "105", "106", "107", "108", "109", "110",
   "111", "112", "113", "114", "115", "116", "117", "118", "119", "120",
   "121", "122", "123", "124", "125", "126", "127", "128", "129", "130",
   "131", "132", "133", "134", "135", "136", "137", "138", "139", "140",
   "141", "142", "143", "144", "145", "146", "147", "148", "149", "150",
   "151", "152", "153", "154", "155", "156", "157", "158", "159", "160",
   "161", "162", "163", "164", "165", "166", "167", "168", "169", "170",
   "171", "172", "173", "174", "175", "176", "177", "178", "179", "180",
   "181", "182", "183", "184", "185", "186", "187", "188", "189", "190",
   "191", "192", "193", "194", "195", "196", "197", "198", "199", "200",
   "201", "202", "203", "204", "205", "206", "207", "208", "209", "210",
   "211", "212", "213", "214", "215", "216", "217", "218", "219", "220",
   "221", "222", "223", "224", "225", "226", "227", "228", "229", "230",
   "231", "232", "233", "234", "235", "236", "237", "238", "239", "240",
   "241", "242", "243", "244", "245", "246", "247", "248", "249", "250",
   "251", "252", "253", "254", "255", "256", "257", "258", "259", "260",
   "261", "262", "263", "264", "265", "266", "267", "268", "269", "270",
   "271", "272", "273", "274", "275", "276", "277", "278", "279", "280",
   "281", "282", "283", "284", "285", "286", "287", "288", "289", "290",
   "291", "292", "293", "294", "295", "296", "297", "298", "299", "300",
   "301", "302", "303", "304", "305", "306", "307", "308", "309", "310",
   "311", "312", "313", "314", "315", "316", "317", "318", "319", "320",
   "321", "322", "323", "324", "325", "326", "327", "328", "329", "330",
   "331", "332", "333", "334", "335", "336", "337", "338", "339", "340",
   "341", "342", "343", "344", "345", "346", "347", "348", "349", "350",
   "351", "352", "353", "354", "355", "356", "357", "358", "359", "360",
   "361", "362", "363", "364", "365", "366", "367", "368", "369", "370",
   "371", "372", "373", "374", "375", "376", "377", "378", "379", "380",
   "381", "382", "383", "384", "385", "386", "387", "388", "389", "390",
   "391", "392", "393", "394", "395", "396", "397", "398", "399", "400",
   "401", "402", "403", "404", "405", "406", "407", "408", "409", "410",
   "411", "412", "413", "414", "415", "416", "417", "418", "419", "420",
   "421", "422", "423", "424", "425", "426", "427", "428", "429", "430",
   "431", "432", "433", "434", "435", "436", "437", "438", "439", "440",
   "441", "442", "443", "444", "445", "446", "447", "448", "449", "450",
   "451", "452", "453", "454", "455", "456", "457", "458", "459", "460",
   "461", "462", "463", "464", "465", "466", "467", "468", "469", "470",
   "471", "472", "473", "474", "475", "476", "477", "478", "479", "480",
   "481", "482", "483", "484", "485", "486", "487", "488", "489", "490",
   "491", "492", "493", "494", "495", "496", "497", "498", "499", "500",
   "501", "502", "503", "504", "505", "506", "507", "508", "509", "510",
   "511", "512", "513", "514", "515", "516", "517", "518", "519", "520",
   "521", "522", "523", "524", "525", "526", "527", "528", "529", "530",
   "531", "532", "533", "534", "535", "536", "537", "538", "539", "540",
   "541", "542", "543", "544", "545", "546", "547", "548", "549", "550",
   "551", "552", "553", "554", "555", "556", "557", "558", "559", "560",
   "561", "562", "563", "564", "565", "566", "567", "568", "569", "570",
   "571", "572", "573", "574", "575", "576", "577", "578", "579", "580",
   "581", "582", "583", "584", "585", "586", "587", "588", "589", "590",
   "591", "592", "593", "594", "595", "596", "597", "598", "599", "600",
   "601", "602", "603", "604", "605", "606", "607", "608", "609", "610",
   "611", "612", "613", "614", "615", "616", "617", "618", "619", "620",
   "621", "622", "623", "624", "625", "626", "627", "628", "629", "630",
   "631", "632", "633", "634", "635", "636", "637", "638", "639", "640",
   "641", "642", "643", "644", "645", "646", "647", "648", "649", "650",
   "651", "652", "653", "654", "655", "656", "657", "658", "659", "660",
   "661", "662", "663", "664", "665", "666", "667", "668", "669", "670",
   "671", "672", "673", "674", "675", "676", "677", "678", "679", "680",
   "681", "682", "683", "684", "685", "686", "687", "688", "689", "690",
   "691", "692", "693", "694", "695", "696", "697", "698", "699", "700",
   "701", "702", "703", "704", "705", "706", "707", "708", "709", "710",
   "711", "712", "713", "714", "715", "716", "717", "718", "719", "720",
   "721", "722", "723", "724", "725", "726", "727", "728", "729", "730",
   "731", "732", "733", "734", "735", "736", "737", "738", "739", "740",
   "741", "742", "743", "744", "745", "746", "747", "748", "749", "750",
   "751", "752", "753", "754", "755", "756", "757", "758", "759", "760",
   "761", "762", "763", "764", "765", "766", "767", "768", "769", "770",
   "771", "772", "773", "774", "775", "776", "777", "778", "779", "780",
   "781", "782", "783", "784", "785", "786", "787", "788", "789", "790",
   "791", "792", "793", "794", "795", "796", "797", "798", "799", "800",
   "801", "802", "803", "804", "805", "806", "807", "808", "809", "810",
   "811", "812", "813", "814", "815", "816", "817", "818", "819", "820",
   "821", "822", "823", "824", "825", "826", "827", "828", "829", "830",
   "831", "832", "833", "834", "835", "836", "837", "838", "839", "840",
   "841", "842", "843", "844", "845", "846", "847", "848", "849", "850",
   "851", "852", "853", "854", "855", "856", "857", "858", "859", "860",
   "861", "862", "863", "864", "865", "866", "867", "868", "869", "870",
   "871", "872", "873", "874", "875", "876", "877", "878", "879", "880",
   "881", "882", "883", "884", "885", "886", "887", "888", "889", "890",
   "891", "892", "893", "894", "895", "896", "897", "898", "899", "900",
   "901", "902", "903", "904", "905", "906", "907", "908", "909", "910",
   "911", "912", "913", "914", "915", "916", "917", "918", "919", "920",
   "921", "922", "923", "924", "925", "926", "927", "928", "929", "930",
   "931", "932", "933", "934", "935", "936", "937", "938", "939", "940",
   "941", "942", "943", "944", "945", "946", "947", "948", "949", "950",
   "951", "952", "953", "954", "955", "956", "957", "958", "959", "960",
   "961", "962", "963", "964", "965", "966", "967", "968", "969", "970",
   "971", "972", "973", "974", "975", "976", "977", "978", "979", "980",
   "981", "982", "983", "984", "985", "986", "987", "988", "989", "990",
   "991", "992", "993", "994", "995", "996", "997", "998", "999"
};


/*
 *--------------------------------------------------------------------------
 *
 * bson_uint32_to_string --
 *
 *       Converts @value to a string.
 *
 *       If @value is from 0 to 1000, it will use a constant string in the
 *       data section of the library.
 *
 *       If not, a string will be formatted using @str and snprintf(). This
 *       is much slower, of course and therefore we try to optimize it out.
 *
 *       @strptr will always be set. It will either point to @str or a
 *       constant string. You will want to use this as your key.
 *
 * Parameters:
 *       @value: A #uint32_t to convert to string.
 *       @strptr: (out): A pointer to the resulting string.
 *       @str: (out): Storage for a string made with snprintf.
 *       @size: Size of @str.
 *
 * Returns:
 *       The number of bytes in the resulting string.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

size_t
bson_uint32_to_string (uint32_t     value,  /* IN */
                       const char **strptr, /* OUT */
                       char        *str,    /* OUT */
                       size_t       size)   /* IN */
{
   if (value < 1000) {
      *strptr = gUint32Strs[value];

      if (value < 10) {
         return 1;
      } else if (value < 100) {
         return 2;
      } else {
         return 3;
      }
   }

   *strptr = str;

   return bson_snprintf (str, size, "%u", value);
}

/*==============================================================*/
/* --- bson-md5.c */

/*
  Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */
/* $Id$ */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
    http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
    either statically or dynamically; added missing #include <string.h>
    in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
    type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
    unsigned in ANSI C, signed in traditional"; made test program
    self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

/*
 * The following MD5 implementation has been modified to use types as
 * specified in libbson.
 */

#undef BYTE_ORDER   /* 1 = big-endian, -1 = little-endian, 0 = unknown */
#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN
#  define BYTE_ORDER 1
#else
#  define BYTE_ORDER -1
#endif

#define T_MASK ((uint32_t)~0)
#define T1 /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2 /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5 /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8 /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)


static void
bson_md5_process (bson_md5_t     *md5,
                  const uint8_t *data)
{
   uint32_t a = md5->abcd[0];
   uint32_t b = md5->abcd[1];
   uint32_t c = md5->abcd[2];
   uint32_t d = md5->abcd[3];
   uint32_t t;

#if BYTE_ORDER > 0
    /* Define storage only for big-endian CPUs. */
    uint32_t X[16];
#else
    /* Define storage for little-endian or both types of CPUs. */
    uint32_t xbuf[16];
    const uint32_t *X;
#endif

    {
#if BYTE_ORDER == 0
        /*
         * Determine dynamically whether this is a big-endian or
         * little-endian machine, since we can use a more efficient
         * algorithm on the latter.
         */
        static const int w = 1;

        if (*((const uint8_t *)&w)) /* dynamic little-endian */
#endif
#if BYTE_ORDER <= 0     /* little-endian */
        {
            /*
             * On little-endian machines, we can process properly aligned
             * data without copying it.
             */
            if (!((data - (const uint8_t *)0) & 3)) {
                /* data are properly aligned */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
#endif
                X = (const uint32_t *)data;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
            }
            else {
                /* not aligned */
                memcpy(xbuf, data, sizeof (xbuf));
                X = xbuf;
            }
        }
#endif
#if BYTE_ORDER == 0
        else            /* dynamic big-endian */
#endif
#if BYTE_ORDER >= 0     /* big-endian */
        {
            /*
             * On big-endian machines, we must arrange the bytes in the
             * right order.
             */
            const uint8_t *xp = data;
            int i;

#  if BYTE_ORDER == 0
            X = xbuf;       /* (dynamic only) */
#  else
#    define xbuf X      /* (static only) */
#  endif
            for (i = 0; i < 16; ++i, xp += 4)
                xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
        }
#endif
    }

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* Round 1. */
    /* Let [abcd k s i] denote the operation
       a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
    t = a + F(b,c,d) + X[k] + Ti;\
    a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  7,  T1);
    SET(d, a, b, c,  1, 12,  T2);
    SET(c, d, a, b,  2, 17,  T3);
    SET(b, c, d, a,  3, 22,  T4);
    SET(a, b, c, d,  4,  7,  T5);
    SET(d, a, b, c,  5, 12,  T6);
    SET(c, d, a, b,  6, 17,  T7);
    SET(b, c, d, a,  7, 22,  T8);
    SET(a, b, c, d,  8,  7,  T9);
    SET(d, a, b, c,  9, 12, T10);
    SET(c, d, a, b, 10, 17, T11);
    SET(b, c, d, a, 11, 22, T12);
    SET(a, b, c, d, 12,  7, T13);
    SET(d, a, b, c, 13, 12, T14);
    SET(c, d, a, b, 14, 17, T15);
    SET(b, c, d, a, 15, 22, T16);
#undef SET

    /* Round 2. */
    /* Let [abcd k s i] denote the operation
         a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
    t = a + G(b,c,d) + X[k] + Ti;\
    a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  1,  5, T17);
    SET(d, a, b, c,  6,  9, T18);
    SET(c, d, a, b, 11, 14, T19);
    SET(b, c, d, a,  0, 20, T20);
    SET(a, b, c, d,  5,  5, T21);
    SET(d, a, b, c, 10,  9, T22);
    SET(c, d, a, b, 15, 14, T23);
    SET(b, c, d, a,  4, 20, T24);
    SET(a, b, c, d,  9,  5, T25);
    SET(d, a, b, c, 14,  9, T26);
    SET(c, d, a, b,  3, 14, T27);
    SET(b, c, d, a,  8, 20, T28);
    SET(a, b, c, d, 13,  5, T29);
    SET(d, a, b, c,  2,  9, T30);
    SET(c, d, a, b,  7, 14, T31);
    SET(b, c, d, a, 12, 20, T32);
#undef SET

    /* Round 3. */
    /* Let [abcd k s t] denote the operation
         a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
    t = a + H(b,c,d) + X[k] + Ti;\
    a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  5,  4, T33);
    SET(d, a, b, c,  8, 11, T34);
    SET(c, d, a, b, 11, 16, T35);
    SET(b, c, d, a, 14, 23, T36);
    SET(a, b, c, d,  1,  4, T37);
    SET(d, a, b, c,  4, 11, T38);
    SET(c, d, a, b,  7, 16, T39);
    SET(b, c, d, a, 10, 23, T40);
    SET(a, b, c, d, 13,  4, T41);
    SET(d, a, b, c,  0, 11, T42);
    SET(c, d, a, b,  3, 16, T43);
    SET(b, c, d, a,  6, 23, T44);
    SET(a, b, c, d,  9,  4, T45);
    SET(d, a, b, c, 12, 11, T46);
    SET(c, d, a, b, 15, 16, T47);
    SET(b, c, d, a,  2, 23, T48);
#undef SET

    /* Round 4. */
    /* Let [abcd k s t] denote the operation
         a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
    t = a + I(b,c,d) + X[k] + Ti;\
    a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  6, T49);
    SET(d, a, b, c,  7, 10, T50);
    SET(c, d, a, b, 14, 15, T51);
    SET(b, c, d, a,  5, 21, T52);
    SET(a, b, c, d, 12,  6, T53);
    SET(d, a, b, c,  3, 10, T54);
    SET(c, d, a, b, 10, 15, T55);
    SET(b, c, d, a,  1, 21, T56);
    SET(a, b, c, d,  8,  6, T57);
    SET(d, a, b, c, 15, 10, T58);
    SET(c, d, a, b,  6, 15, T59);
    SET(b, c, d, a, 13, 21, T60);
    SET(a, b, c, d,  4,  6, T61);
    SET(d, a, b, c, 11, 10, T62);
    SET(c, d, a, b,  2, 15, T63);
    SET(b, c, d, a,  9, 21, T64);
#undef SET

    /* Then perform the following additions. (That is increment each
       of the four registers by the value it had before this block
       was started.) */
    md5->abcd[0] += a;
    md5->abcd[1] += b;
    md5->abcd[2] += c;
    md5->abcd[3] += d;
}

void
bson_md5_init (bson_md5_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void
bson_md5_append (bson_md5_t         *pms,
                 const uint8_t *data,
                 uint32_t       nbytes)
{
    const uint8_t *p = data;
    int left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    uint32_t nbits = (uint32_t)(nbytes << 3);

    if (nbytes <= 0)
        return;

    /* Update the message length. */
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
        pms->count[1]++;

    /* Process an initial partial block. */
    if (offset) {
        int copy = (offset + (int)nbytes > 64 ? 64 - offset : (int)nbytes);

        memcpy(pms->buf + offset, p, copy);
        if (offset + copy < 64)
            return;
        p += copy;
        left -= copy;
        bson_md5_process(pms, pms->buf);
    }

    /* Process full blocks. */
    for (; left >= 64; p += 64, left -= 64)
        bson_md5_process(pms, p);

    /* Process a final partial block. */
    if (left)
        memcpy(pms->buf, p, left);
}

void
bson_md5_finish (bson_md5_t   *pms,
                 uint8_t  digest[16])
{
    static const uint8_t pad[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint8_t data[8];
    int i;

    /* Save the length before padding. */
    for (i = 0; i < 8; ++i)
        data[i] = (uint8_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    /* Pad to 56 bytes mod 64. */
    bson_md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    /* Append the length. */
    bson_md5_append(pms, data, sizeof (data));
    for (i = 0; i < 16; ++i)
        digest[i] = (uint8_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

/*==============================================================*/
/* --- bson-memory.c */

static bson_mem_vtable_t gMemVtable = {
   malloc,
   calloc,
#ifdef __APPLE__
   reallocf,
#else
   realloc,
#endif
   free,
};


/*
 *--------------------------------------------------------------------------
 *
 * bson_malloc --
 *
 *       Allocates @num_bytes of memory and returns a pointer to it.  If
 *       malloc failed to allocate the memory, abort() is called.
 *
 *       Libbson does not try to handle OOM conditions as it is beyond the
 *       scope of this library to handle so appropriately.
 *
 * Parameters:
 *       @num_bytes: The number of bytes to allocate.
 *
 * Returns:
 *       A pointer if successful; otherwise abort() is called and this
 *       function will never return.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void *
bson_malloc (size_t num_bytes) /* IN */
{
   void *mem;

   if (!(mem = gMemVtable.malloc (num_bytes))) {
      abort ();
   }

   return mem;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_malloc0 --
 *
 *       Like bson_malloc() except the memory is zeroed first. This is
 *       similar to calloc() except that abort() is called in case of
 *       failure to allocate memory.
 *
 * Parameters:
 *       @num_bytes: The number of bytes to allocate.
 *
 * Returns:
 *       A pointer if successful; otherwise abort() is called and this
 *       function will never return.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void *
bson_malloc0 (size_t num_bytes) /* IN */
{
   void *mem = NULL;

   if (BSON_LIKELY (num_bytes)) {
      if (BSON_UNLIKELY (!(mem = gMemVtable.calloc (1, num_bytes)))) {
         abort ();
      }
   }

   return mem;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_realloc --
 *
 *       This function behaves similar to realloc() except that if there is
 *       a failure abort() is called.
 *
 * Parameters:
 *       @mem: The memory to realloc, or NULL.
 *       @num_bytes: The size of the new allocation or 0 to free.
 *
 * Returns:
 *       The new allocation if successful; otherwise abort() is called and
 *       this function never returns.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void *
bson_realloc (void   *mem,        /* IN */
              size_t  num_bytes)  /* IN */
{
   /*
    * Not all platforms are guaranteed to free() the memory if a call to
    * realloc() with a size of zero occurs. Windows, Linux, and FreeBSD do,
    * however, OS X does not.
    */
   if (BSON_UNLIKELY (num_bytes == 0)) {
      gMemVtable.free (mem);
      return NULL;
   }

   mem = gMemVtable.realloc (mem, num_bytes);

   if (BSON_UNLIKELY (!mem)) {
      abort ();
   }

   return mem;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_realloc_ctx --
 *
 *       This wraps bson_realloc and provides a compatible api for similar
 *       functions with a context
 *
 * Parameters:
 *       @mem: The memory to realloc, or NULL.
 *       @num_bytes: The size of the new allocation or 0 to free.
 *       @ctx: Ignored
 *
 * Returns:
 *       The new allocation if successful; otherwise abort() is called and
 *       this function never returns.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */


void *
bson_realloc_ctx (void   *mem,        /* IN */
                  size_t  num_bytes,  /* IN */
                  void   *ctx)        /* IN */
{
   return bson_realloc (mem, num_bytes);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_free --
 *
 *       Frees @mem using the underlying allocator.
 *
 *       Currently, this only calls free() directly, but that is subject to
 *       change.
 *
 * Parameters:
 *       @mem: An allocation to free.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_free (void *mem) /* IN */
{
   gMemVtable.free (mem);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_zero_free --
 *
 *       Frees @mem using the underlying allocator. @size bytes of @mem will
 *       be zeroed before freeing the memory. This is useful in scenarios
 *       where @mem contains passwords or other sensitive information.
 *
 * Parameters:
 *       @mem: An allocation to free.
 *       @size: The number of bytes in @mem.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_zero_free (void  *mem,  /* IN */
                size_t size) /* IN */
{
   if (BSON_LIKELY (mem)) {
      memset (mem, 0, size);
      gMemVtable.free (mem);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_mem_set_vtable --
 *
 *       This function will change our allocationt vtable.
 *
 *       It is imperitive that this is called at the beginning of the
 *       process before any memory has been allocated by the default
 *       allocator.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_mem_set_vtable (const bson_mem_vtable_t *vtable)
{
   BSON_ASSERT (vtable);

   if (!vtable->malloc ||
       !vtable->calloc ||
       !vtable->realloc ||
       !vtable->free) {
      fprintf (stderr, "Failure to install BSON vtable, "
                       "missing functions.\n");
      return;
   }

   gMemVtable = *vtable;
}

void
bson_mem_restore_vtable (void)
{
   bson_mem_vtable_t vtable = {
      malloc,
      calloc,
#ifdef __APPLE__
      reallocf,
#else
      realloc,
#endif
      free,
   };

   bson_mem_set_vtable(&vtable);
}


/*==============================================================*/
/* --- bson-oid.c */

/*
 * This table contains an array of two character pairs for every possible
 * uint8_t. It is used as a lookup table when encoding a bson_oid_t
 * to hex formatted ASCII. Performing two characters at a time roughly
 * reduces the number of operations by one-half.
 */
static const uint16_t gHexCharPairs[] = {
#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN
   12336, 12337, 12338, 12339, 12340, 12341, 12342, 12343, 12344, 12345,
   12385, 12386, 12387, 12388, 12389, 12390, 12592, 12593, 12594, 12595,
   12596, 12597, 12598, 12599, 12600, 12601, 12641, 12642, 12643, 12644,
   12645, 12646, 12848, 12849, 12850, 12851, 12852, 12853, 12854, 12855,
   12856, 12857, 12897, 12898, 12899, 12900, 12901, 12902, 13104, 13105,
   13106, 13107, 13108, 13109, 13110, 13111, 13112, 13113, 13153, 13154,
   13155, 13156, 13157, 13158, 13360, 13361, 13362, 13363, 13364, 13365,
   13366, 13367, 13368, 13369, 13409, 13410, 13411, 13412, 13413, 13414,
   13616, 13617, 13618, 13619, 13620, 13621, 13622, 13623, 13624, 13625,
   13665, 13666, 13667, 13668, 13669, 13670, 13872, 13873, 13874, 13875,
   13876, 13877, 13878, 13879, 13880, 13881, 13921, 13922, 13923, 13924,
   13925, 13926, 14128, 14129, 14130, 14131, 14132, 14133, 14134, 14135,
   14136, 14137, 14177, 14178, 14179, 14180, 14181, 14182, 14384, 14385,
   14386, 14387, 14388, 14389, 14390, 14391, 14392, 14393, 14433, 14434,
   14435, 14436, 14437, 14438, 14640, 14641, 14642, 14643, 14644, 14645,
   14646, 14647, 14648, 14649, 14689, 14690, 14691, 14692, 14693, 14694,
   24880, 24881, 24882, 24883, 24884, 24885, 24886, 24887, 24888, 24889,
   24929, 24930, 24931, 24932, 24933, 24934, 25136, 25137, 25138, 25139,
   25140, 25141, 25142, 25143, 25144, 25145, 25185, 25186, 25187, 25188,
   25189, 25190, 25392, 25393, 25394, 25395, 25396, 25397, 25398, 25399,
   25400, 25401, 25441, 25442, 25443, 25444, 25445, 25446, 25648, 25649,
   25650, 25651, 25652, 25653, 25654, 25655, 25656, 25657, 25697, 25698,
   25699, 25700, 25701, 25702, 25904, 25905, 25906, 25907, 25908, 25909,
   25910, 25911, 25912, 25913, 25953, 25954, 25955, 25956, 25957, 25958,
   26160, 26161, 26162, 26163, 26164, 26165, 26166, 26167, 26168, 26169,
   26209, 26210, 26211, 26212, 26213, 26214
#else
   12336, 12592, 12848, 13104, 13360, 13616, 13872, 14128, 14384, 14640,
   24880, 25136, 25392, 25648, 25904, 26160, 12337, 12593, 12849, 13105,
   13361, 13617, 13873, 14129, 14385, 14641, 24881, 25137, 25393, 25649,
   25905, 26161, 12338, 12594, 12850, 13106, 13362, 13618, 13874, 14130,
   14386, 14642, 24882, 25138, 25394, 25650, 25906, 26162, 12339, 12595,
   12851, 13107, 13363, 13619, 13875, 14131, 14387, 14643, 24883, 25139,
   25395, 25651, 25907, 26163, 12340, 12596, 12852, 13108, 13364, 13620,
   13876, 14132, 14388, 14644, 24884, 25140, 25396, 25652, 25908, 26164,
   12341, 12597, 12853, 13109, 13365, 13621, 13877, 14133, 14389, 14645,
   24885, 25141, 25397, 25653, 25909, 26165, 12342, 12598, 12854, 13110,
   13366, 13622, 13878, 14134, 14390, 14646, 24886, 25142, 25398, 25654,
   25910, 26166, 12343, 12599, 12855, 13111, 13367, 13623, 13879, 14135,
   14391, 14647, 24887, 25143, 25399, 25655, 25911, 26167, 12344, 12600,
   12856, 13112, 13368, 13624, 13880, 14136, 14392, 14648, 24888, 25144,
   25400, 25656, 25912, 26168, 12345, 12601, 12857, 13113, 13369, 13625,
   13881, 14137, 14393, 14649, 24889, 25145, 25401, 25657, 25913, 26169,
   12385, 12641, 12897, 13153, 13409, 13665, 13921, 14177, 14433, 14689,
   24929, 25185, 25441, 25697, 25953, 26209, 12386, 12642, 12898, 13154,
   13410, 13666, 13922, 14178, 14434, 14690, 24930, 25186, 25442, 25698,
   25954, 26210, 12387, 12643, 12899, 13155, 13411, 13667, 13923, 14179,
   14435, 14691, 24931, 25187, 25443, 25699, 25955, 26211, 12388, 12644,
   12900, 13156, 13412, 13668, 13924, 14180, 14436, 14692, 24932, 25188,
   25444, 25700, 25956, 26212, 12389, 12645, 12901, 13157, 13413, 13669,
   13925, 14181, 14437, 14693, 24933, 25189, 25445, 25701, 25957, 26213,
   12390, 12646, 12902, 13158, 13414, 13670, 13926, 14182, 14438, 14694,
   24934, 25190, 25446, 25702, 25958, 26214
#endif
};


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_init_sequence --
 *
 *       Initializes @oid with the next oid in the sequence. The first 4
 *       bytes contain the current time and the following 8 contain a 64-bit
 *       integer in big-endian format.
 *
 *       The bson_oid_t generated by this function is not guaranteed to be
 *       globally unique. Only unique within this context. It is however,
 *       guaranteed to be sequential.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_oid_init_sequence (bson_oid_t     *oid,     /* OUT */
                        bson_context_t *context) /* IN */
{
   uint32_t now = (uint32_t)(time (NULL));

   if (!context) {
      context = bson_context_get_default ();
   }

   now = BSON_UINT32_TO_BE (now);

   memcpy (&oid->bytes[0], &now, sizeof (now));
   context->oid_get_seq64 (context, oid);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_init --
 *
 *       Generates bytes for a new bson_oid_t and stores them in @oid. The
 *       bytes will be generated according to the specification and includes
 *       the current time, first 3 bytes of MD5(hostname), pid (or tid), and
 *       monotonic counter.
 *
 *       The bson_oid_t generated by this function is not guaranteed to be
 *       globally unique. Only unique within this context. It is however,
 *       guaranteed to be sequential.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_oid_init (bson_oid_t     *oid,     /* OUT */
               bson_context_t *context) /* IN */
{
   uint32_t now = (uint32_t)(time (NULL));

   BSON_ASSERT (oid);

   if (!context) {
      context = bson_context_get_default ();
   }

   now = BSON_UINT32_TO_BE (now);
   memcpy (&oid->bytes[0], &now, sizeof (now));

   context->oid_get_host (context, oid);
   context->oid_get_pid (context, oid);
   context->oid_get_seq32 (context, oid);
}


/**
 * bson_oid_init_from_data:
 * @oid: A bson_oid_t to initialize.
 * @bytes: A 12-byte buffer to copy into @oid.
 *
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_init_from_data --
 *
 *       Initializes an @oid from @data. @data MUST be a buffer of at least
 *       12 bytes. This method is analagous to memcpy()'ing data into @oid.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_oid_init_from_data (bson_oid_t    *oid,  /* OUT */
                         const uint8_t *data) /* IN */
{
   BSON_ASSERT (oid);
   BSON_ASSERT (data);

   memcpy (oid, data, 12);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_init_from_string --
 *
 *       Parses @str containing hex formatted bytes of an object id and
 *       places the bytes in @oid.
 *
 * Parameters:
 *       @oid: A bson_oid_t
 *       @str: A string containing at least 24 characters.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
bson_oid_init_from_string (bson_oid_t *oid, /* OUT */
                           const char *str) /* IN */
{
   BSON_ASSERT (oid);
   BSON_ASSERT (str);

   bson_oid_init_from_string_unsafe (oid, str);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_get_time_t --
 *
 *       Fetches the time for which @oid was created.
 *
 * Returns:
 *       A time_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

time_t
bson_oid_get_time_t (const bson_oid_t *oid) /* IN */
{
   BSON_ASSERT (oid);

   return bson_oid_get_time_t_unsafe (oid);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_to_string --
 *
 *       Formats a bson_oid_t into a string. @str must contain enough bytes
 *       for the resulting string which is 25 bytes with a terminating
 *       NUL-byte.
 *
 * Parameters:
 *       @oid: A bson_oid_t.
 *       @str: A location to store the resulting string.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_oid_to_string
   (const bson_oid_t *oid,                                   /* IN */
    char              str[BSON_ENSURE_ARRAY_PARAM_SIZE(25)]) /* OUT */
{
#if !defined(__i386__) && !defined(__x86_64__)
   BSON_ASSERT (oid);
   BSON_ASSERT (str);

   bson_snprintf (str, 25,
                  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                  oid->bytes[0],
                  oid->bytes[1],
                  oid->bytes[2],
                  oid->bytes[3],
                  oid->bytes[4],
                  oid->bytes[5],
                  oid->bytes[6],
                  oid->bytes[7],
                  oid->bytes[8],
                  oid->bytes[9],
                  oid->bytes[10],
                  oid->bytes[11]);
#else
   uint16_t *dst;
   uint8_t *id = (uint8_t *)oid;

   BSON_ASSERT (oid);
   BSON_ASSERT (str);

   dst = (uint16_t *)(void *)str;
   dst[0] = gHexCharPairs[id[0]];
   dst[1] = gHexCharPairs[id[1]];
   dst[2] = gHexCharPairs[id[2]];
   dst[3] = gHexCharPairs[id[3]];
   dst[4] = gHexCharPairs[id[4]];
   dst[5] = gHexCharPairs[id[5]];
   dst[6] = gHexCharPairs[id[6]];
   dst[7] = gHexCharPairs[id[7]];
   dst[8] = gHexCharPairs[id[8]];
   dst[9] = gHexCharPairs[id[9]];
   dst[10] = gHexCharPairs[id[10]];
   dst[11] = gHexCharPairs[id[11]];
   str[24] = '\0';
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_hash --
 *
 *       Hashes the bytes of the provided bson_oid_t using DJB hash.  This
 *       allows bson_oid_t to be used as keys in a hash table.
 *
 * Returns:
 *       A hash value corresponding to @oid.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
bson_oid_hash (const bson_oid_t *oid) /* IN */
{
   BSON_ASSERT (oid);

   return bson_oid_hash_unsafe (oid);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_compare --
 *
 *       A qsort() style compare function that will return less than zero if
 *       @oid1 is less than @oid2, zero if they are the same, and greater
 *       than zero if @oid2 is greater than @oid1.
 *
 * Returns:
 *       A qsort() style compare integer.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
bson_oid_compare (const bson_oid_t *oid1, /* IN */
                  const bson_oid_t *oid2) /* IN */
{
   BSON_ASSERT (oid1);
   BSON_ASSERT (oid2);

   return bson_oid_compare_unsafe (oid1, oid2);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_equal --
 *
 *       Compares for equality of @oid1 and @oid2. If they are equal, then
 *       true is returned, otherwise false.
 *
 * Returns:
 *       A boolean indicating the equality of @oid1 and @oid2.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_oid_equal (const bson_oid_t *oid1, /* IN */
                const bson_oid_t *oid2) /* IN */
{
   BSON_ASSERT (oid1);
   BSON_ASSERT (oid2);

   return bson_oid_equal_unsafe (oid1, oid2);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_copy --
 *
 *       Copies the contents of @src to @dst.
 *
 * Parameters:
 *       @src: A bson_oid_t to copy from.
 *       @dst: A bson_oid_t to copy to.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @dst will contain a copy of the data in @src.
 *
 *--------------------------------------------------------------------------
 */

void
bson_oid_copy (const bson_oid_t *src, /* IN */
               bson_oid_t       *dst) /* OUT */
{
   BSON_ASSERT (src);
   BSON_ASSERT (dst);

   bson_oid_copy_unsafe (src, dst);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_oid_is_valid --
 *
 *       Validates that @str is a valid OID string. @length MUST be 24, but
 *       is provided as a parameter to simplify calling code.
 *
 * Parameters:
 *       @str: A string to validate.
 *       @length: The length of @str.
 *
 * Returns:
 *       true if @str can be passed to bson_oid_init_from_string().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_oid_is_valid (const char *str,    /* IN */
                   size_t      length) /* IN */
{
   size_t i;

   BSON_ASSERT (str);

   if ((length == 25) && (str [24] == '\0')) {
      length = 24;
   }

   if (length == 24) {
      for (i = 0; i < length; i++) {
         switch (str[i]) {
         case '0': case '1': case '2': case '3': case '4': case '5': case '6':
         case '7': case '8': case '9': case 'a': case 'b': case 'c': case 'd':
         case 'e': case 'f':
            break;
         default:
            return false;
         }
      }
      return true;
   }

   return false;
}

/*==============================================================*/
/* --- bson-reader.c */

typedef enum
{
   BSON_READER_HANDLE = 1,
   BSON_READER_DATA = 2,
} bson_reader_type_t;


typedef struct
{
   bson_reader_type_t         type;
   void                      *handle;
   bool                       done   : 1;
   bool                       failed : 1;
   size_t                     end;
   size_t                     len;
   size_t                     offset;
   size_t                     bytes_read;
   bson_t                     inline_bson;
   uint8_t                   *data;
   bson_reader_read_func_t    read_func;
   bson_reader_destroy_func_t destroy_func;
} bson_reader_handle_t;


typedef struct
{
   int fd;
   bool do_close;
} bson_reader_handle_fd_t;


typedef struct
{
   bson_reader_type_t type;
   const uint8_t     *data;
   size_t             length;
   size_t             offset;
   bson_t             inline_bson;
} bson_reader_data_t;


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_handle_fill_buffer --
 *
 *       Attempt to read as much as possible until the underlying buffer
 *       in @reader is filled or we have reached end-of-stream or
 *       read failure.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_reader_handle_fill_buffer (bson_reader_handle_t *reader) /* IN */
{
   ssize_t ret;

   /*
    * Handle first read specially.
    */
   if ((!reader->done) && (!reader->offset) && (!reader->end)) {
      ret = reader->read_func (reader->handle, &reader->data[0], reader->len);

      if (ret <= 0) {
         reader->done = true;
         return;
      }
      reader->bytes_read += ret;

      reader->end = ret;
      return;
   }

   /*
    * Move valid data to head.
    */
   memmove (&reader->data[0],
            &reader->data[reader->offset],
            reader->end - reader->offset);
   reader->end = reader->end - reader->offset;
   reader->offset = 0;

   /*
    * Read in data to fill the buffer.
    */
   ret = reader->read_func (reader->handle,
                            &reader->data[reader->end],
                            reader->len - reader->end);

   if (ret <= 0) {
      reader->done = true;
      reader->failed = (ret < 0);
   } else {
      reader->bytes_read += ret;
      reader->end += ret;
   }

   BSON_ASSERT (reader->offset == 0);
   BSON_ASSERT (reader->end <= reader->len);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_new_from_handle --
 *
 *       Allocates and initializes a new bson_reader_t using the opaque
 *       handle provided.
 *
 * Parameters:
 *       @handle: an opaque handle to use to read data.
 *       @rf: a function to perform reads on @handle.
 *       @df: a function to release @handle, or NULL.
 *
 * Returns:
 *       A newly allocated bson_reader_t if successful, otherwise NULL.
 *       Free the successful result with bson_reader_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_reader_t *
bson_reader_new_from_handle (void                       *handle,
                             bson_reader_read_func_t     rf,
                             bson_reader_destroy_func_t  df)
{
   bson_reader_handle_t *real;

   BSON_ASSERT (handle);
   BSON_ASSERT (rf);

   real = bson_malloc0 (sizeof *real);
   real->type = BSON_READER_HANDLE;
   real->data = bson_malloc0 (1024);
   real->handle = handle;
   real->len = 1024;
   real->offset = 0;

   bson_reader_set_read_func ((bson_reader_t *)real, rf);

   if (df) {
      bson_reader_set_destroy_func ((bson_reader_t *)real, df);
   }

   _bson_reader_handle_fill_buffer (real);

   return (bson_reader_t *)real;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_handle_fd_destroy --
 *
 *       Cleanup allocations associated with state created in
 *       bson_reader_new_from_fd().
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_reader_handle_fd_destroy (void *handle) /* IN */
{
   bson_reader_handle_fd_t *fd = handle;

   if (fd) {
      if ((fd->fd != -1) && fd->do_close) {
#ifdef _WIN32
         _close (fd->fd);
#else
         close (fd->fd);
#endif
      }
      bson_free (fd);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_handle_fd_read --
 *
 *       Perform read on opaque handle created in
 *       bson_reader_new_from_fd().
 *
 *       The underlying file descriptor is read from the current position
 *       using the bson_reader_handle_fd_t allocated.
 *
 * Returns:
 *       -1 on failure.
 *       0 on end of stream.
 *       Greater than zero on success.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
_bson_reader_handle_fd_read (void   *handle, /* IN */
                             void   *buf,    /* IN */
                             size_t  len)    /* IN */
{
   bson_reader_handle_fd_t *fd = handle;
   ssize_t ret = -1;

   if (fd && (fd->fd != -1)) {
   again:
#ifdef BSON_OS_WIN32
      ret = _read (fd->fd, buf, (unsigned int)len);
#else
      ret = read (fd->fd, buf, len);
#endif
      if ((ret == -1) && (errno == EAGAIN)) {
         goto again;
      }
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_new_from_fd --
 *
 *       Create a new bson_reader_t using the file-descriptor provided.
 *
 * Parameters:
 *       @fd: a libc style file-descriptor.
 *       @close_on_destroy: if close() should be called on @fd when
 *          bson_reader_destroy() is called.
 *
 * Returns:
 *       A newly allocated bson_reader_t on success; otherwise NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_reader_t *
bson_reader_new_from_fd (int  fd,               /* IN */
                         bool close_on_destroy) /* IN */
{
   bson_reader_handle_fd_t *handle;

   BSON_ASSERT (fd != -1);

   handle = bson_malloc0 (sizeof *handle);
   handle->fd = fd;
   handle->do_close = close_on_destroy;

   return bson_reader_new_from_handle (handle,
                                       _bson_reader_handle_fd_read,
                                       _bson_reader_handle_fd_destroy);
}


/**
 * bson_reader_set_read_func:
 * @reader: A bson_reader_t.
 *
 * Note that @reader must be initialized by bson_reader_init_from_handle(), or data
 * will be destroyed.
 */
/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_set_read_func --
 *
 *       Set the read func to be provided for @reader.
 *
 *       You probably want to use bson_reader_new_from_handle() or
 *       bson_reader_new_from_fd() instead.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_reader_set_read_func (bson_reader_t          *reader, /* IN */
                           bson_reader_read_func_t func)   /* IN */
{
   bson_reader_handle_t *real = (bson_reader_handle_t *)reader;

   BSON_ASSERT (reader->type == BSON_READER_HANDLE);

   real->read_func = func;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_set_destroy_func --
 *
 *       Set the function to cleanup state when @reader is destroyed.
 *
 *       You probably want bson_reader_new_from_fd() or
 *       bson_reader_new_from_handle() instead.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_reader_set_destroy_func (bson_reader_t             *reader, /* IN */
                              bson_reader_destroy_func_t func)   /* IN */
{
   bson_reader_handle_t *real = (bson_reader_handle_t *)reader;

   BSON_ASSERT (reader->type == BSON_READER_HANDLE);

   real->destroy_func = func;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_handle_grow_buffer --
 *
 *       Grow the buffer to the next power of two.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_reader_handle_grow_buffer (bson_reader_handle_t *reader) /* IN */
{
   size_t size;

   size = reader->len * 2;
   reader->data = bson_realloc (reader->data, size);
   reader->len = size;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_handle_tell --
 *
 *       Tell the current position within the underlying file-descriptor.
 *
 * Returns:
 *       An off_t containing the current offset.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static off_t
_bson_reader_handle_tell (bson_reader_handle_t *reader) /* IN */
{
   off_t off;

   off = (off_t)reader->bytes_read;
   off -= (off_t)reader->end;
   off += (off_t)reader->offset;

   return off;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_handle_read --
 *
 *       Read the next chunk of data from the underlying file descriptor
 *       and return a bson_t which should not be modified.
 *
 *       There was a failure if NULL is returned and @reached_eof is
 *       not set to true.
 *
 * Returns:
 *       NULL on failure or end of stream.
 *
 * Side effects:
 *       @reached_eof is set if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

static const bson_t *
_bson_reader_handle_read (bson_reader_handle_t *reader,      /* IN */
                          bool                 *reached_eof) /* IN */
{
   int32_t blen;

   if (reached_eof) {
      *reached_eof = false;
   }

   while (!reader->done) {
      if ((reader->end - reader->offset) < 4) {
         _bson_reader_handle_fill_buffer (reader);
         continue;
      }

      memcpy (&blen, &reader->data[reader->offset], sizeof blen);
      blen = BSON_UINT32_FROM_LE (blen);

      if (blen < 5) {
         return NULL;
      }

      if (blen > (int32_t)(reader->end - reader->offset)) {
         if (blen > (int32_t)reader->len) {
            _bson_reader_handle_grow_buffer (reader);
         }

         _bson_reader_handle_fill_buffer (reader);
         continue;
      }

      if (!bson_init_static (&reader->inline_bson,
                             &reader->data[reader->offset],
                             (uint32_t)blen)) {
         return NULL;
      }

      reader->offset += blen;

      return &reader->inline_bson;
   }

   if (reached_eof) {
      *reached_eof = reader->done && !reader->failed;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_new_from_data --
 *
 *       Allocates and initializes a new bson_reader_t that reads the memory
 *       provided as a stream of BSON documents.
 *
 * Parameters:
 *       @data: A buffer to read BSON documents from.
 *       @length: The length of @data.
 *
 * Returns:
 *       A newly allocated bson_reader_t that should be freed with
 *       bson_reader_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_reader_t *
bson_reader_new_from_data (const uint8_t *data,   /* IN */
                           size_t         length) /* IN */
{
   bson_reader_data_t *real;

   BSON_ASSERT (data);

   real = (bson_reader_data_t*)bson_malloc0 (sizeof *real);
   real->type = BSON_READER_DATA;
   real->data = data;
   real->length = length;
   real->offset = 0;

   return (bson_reader_t *)real;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_data_read --
 *
 *       Read the next document from the underlying buffer.
 *
 * Returns:
 *       NULL on failure or end of stream.
 *       a bson_t which should not be modified.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static const bson_t *
_bson_reader_data_read (bson_reader_data_t *reader,      /* IN */
                        bool               *reached_eof) /* IN */
{
   int32_t blen;

   if (reached_eof) {
      *reached_eof = false;
   }

   if ((reader->offset + 4) < reader->length) {
      memcpy (&blen, &reader->data[reader->offset], sizeof blen);
      blen = BSON_UINT32_FROM_LE (blen);

      if (blen < 5) {
         return NULL;
      }

      if (blen > (int32_t)(reader->length - reader->offset)) {
         return NULL;
      }

      if (!bson_init_static (&reader->inline_bson,
                             &reader->data[reader->offset], (uint32_t)blen)) {
         return NULL;
      }

      reader->offset += blen;

      return &reader->inline_bson;
   }

   if (reached_eof) {
      *reached_eof = (reader->offset == reader->length);
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_reader_data_tell --
 *
 *       Tell the current position in the underlying buffer.
 *
 * Returns:
 *       An off_t of the current offset.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static off_t
_bson_reader_data_tell (bson_reader_data_t *reader) /* IN */
{
   return (off_t)reader->offset;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_destroy --
 *
 *       Release a bson_reader_t created with bson_reader_new_from_data(),
 *       bson_reader_new_from_fd(), or bson_reader_new_from_handle().
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_reader_destroy (bson_reader_t *reader) /* IN */
{
   BSON_ASSERT (reader);

   switch (reader->type) {
   case 0:
      break;
   case BSON_READER_HANDLE:
      {
         bson_reader_handle_t *handle = (bson_reader_handle_t *)reader;

         if (handle->destroy_func) {

            handle->destroy_func(handle->handle);
         }

         bson_free (handle->data);
      }
      break;
   case BSON_READER_DATA:
      break;
   default:
      fprintf (stderr, "No such reader type: %02x\n", reader->type);
      break;
   }

   reader->type = 0;

   bson_free (reader);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_read --
 *
 *       Reads the next bson_t in the underlying memory or storage.  The
 *       resulting bson_t should not be modified or freed. You may copy it
 *       and iterate over it.  Functions that take a const bson_t* are safe
 *       to use.
 *
 *       This structure does not survive calls to bson_reader_read() or
 *       bson_reader_destroy() as it uses memory allocated by the reader or
 *       underlying storage/memory.
 *
 *       If NULL is returned then @reached_eof will be set to true if the
 *       end of the file or buffer was reached. This indicates if there was
 *       an error parsing the document stream.
 *
 * Returns:
 *       A const bson_t that should not be modified or freed.
 *       NULL on failure or end of stream.
 *
 * Side effects:
 *       @reached_eof is set if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

const bson_t *
bson_reader_read (bson_reader_t *reader,      /* IN */
                  bool          *reached_eof) /* OUT */
{
   BSON_ASSERT (reader);

   switch (reader->type) {
   case BSON_READER_HANDLE:
      return _bson_reader_handle_read ((bson_reader_handle_t *)reader, reached_eof);

   case BSON_READER_DATA:
      return _bson_reader_data_read ((bson_reader_data_t *)reader, reached_eof);

   default:
      fprintf (stderr, "No such reader type: %02x\n", reader->type);
      break;
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_tell --
 *
 *       Return the current position in the underlying reader. This will
 *       always be at the beginning of a bson document or end of file.
 *
 * Returns:
 *       An off_t containing the current offset.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

off_t
bson_reader_tell (bson_reader_t *reader) /* IN */
{
   BSON_ASSERT (reader);

   switch (reader->type) {
   case BSON_READER_HANDLE:
      return _bson_reader_handle_tell ((bson_reader_handle_t *)reader);

   case BSON_READER_DATA:
      return _bson_reader_data_tell ((bson_reader_data_t *)reader);

   default:
      fprintf (stderr, "No such reader type: %02x\n", reader->type);
      return -1;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_new_from_file --
 *
 *       A convenience function to open a file containing sequential
 *       bson documents and read them using bson_reader_t.
 *
 * Returns:
 *       A new bson_reader_t if successful, otherwise NULL and
 *       @error is set. Free the non-NULL result with
 *       bson_reader_destroy().
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bson_reader_t *
bson_reader_new_from_file (const char   *path,  /* IN */
                           bson_error_t *error) /* OUT */
{
   char errmsg_buf[BSON_ERROR_BUFFER_SIZE];
   char *errmsg;
   int fd;

   BSON_ASSERT (path);

#ifdef BSON_OS_WIN32
   if (_sopen_s (&fd, path, (_O_RDONLY | _O_BINARY), _SH_DENYNO, 0) != 0) {
      fd = -1;
   }
#else
   fd = open (path, O_RDONLY);
#endif

   if (fd == -1) {
      errmsg = bson_strerror_r (errno, errmsg_buf, sizeof errmsg_buf);
      bson_set_error (error,
                      BSON_ERROR_READER,
                      BSON_ERROR_READER_BADFD,
                      "%s", errmsg);
      return NULL;
   }

   return bson_reader_new_from_fd (fd, true);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_reader_reset --
 *
 *       Restore the reader to its initial state. Valid only for readers
 *       created with bson_reader_new_from_data.
 *
 *--------------------------------------------------------------------------
 */

void
bson_reader_reset (bson_reader_t *reader)
{
   bson_reader_data_t *real = (bson_reader_data_t *) reader;

   if (real->type != BSON_READER_DATA) {
      fprintf (stderr, "Reader type cannot be reset\n");
      return;
   }

   real->offset = 0;
}

/*==============================================================*/
/* --- bson-string.c */

/*
 *--------------------------------------------------------------------------
 *
 * bson_string_new --
 *
 *       Create a new bson_string_t.
 *
 *       bson_string_t is a power-of-2 allocation growing string. Every
 *       time data is appended the next power of two size is chosen for
 *       the allocation. Pretty standard stuff.
 *
 *       It is UTF-8 aware through the use of bson_string_append_unichar().
 *       The proper UTF-8 character sequence will be used.
 *
 * Parameters:
 *       @str: a string to copy or NULL.
 *
 * Returns:
 *       A newly allocated bson_string_t that should be freed with
 *       bson_string_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_string_t *
bson_string_new (const char *str) /* IN */
{
   bson_string_t *ret;

   ret = bson_malloc0 (sizeof *ret);
   ret->len = str ? (int)strlen (str) : 0;
   ret->alloc = ret->len + 1;

   if (!bson_is_power_of_two (ret->alloc)) {
      ret->alloc = (uint32_t)bson_next_power_of_two ((size_t)ret->alloc);
   }

   BSON_ASSERT (ret->alloc >= 1);

   ret->str = bson_malloc (ret->alloc);

   if (str) {
      memcpy (ret->str, str, ret->len);
   }
   ret->str [ret->len] = '\0';

   ret->str [ret->len] = '\0';

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_string_free --
 *
 *       Free the bson_string_t @string and related allocations.
 *
 *       If @free_segment is false, then the strings buffer will be
 *       returned and is not freed. Otherwise, NULL is returned.
 *
 * Returns:
 *       The string->str if free_segment is false.
 *       Otherwise NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_string_free (bson_string_t *string,       /* IN */
                  bool           free_segment) /* IN */
{
   char *ret = NULL;

   BSON_ASSERT (string);

   if (!free_segment) {
      ret = string->str;
   } else {
      bson_free (string->str);
   }

   bson_free (string);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_string_append --
 *
 *       Append the UTF-8 string @str to @string.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_string_append (bson_string_t *string, /* IN */
                    const char    *str)    /* IN */
{
   uint32_t len;

   BSON_ASSERT (string);
   BSON_ASSERT (str);

   len = (uint32_t)strlen (str);

   if ((string->alloc - string->len - 1) < len) {
      string->alloc += len;
      if (!bson_is_power_of_two (string->alloc)) {
         string->alloc = (uint32_t)bson_next_power_of_two ((size_t)string->alloc);
      }
      string->str = bson_realloc (string->str, string->alloc);
   }

   memcpy (string->str + string->len, str, len);
   string->len += len;
   string->str [string->len] = '\0';
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_string_append_c --
 *
 *       Append the ASCII character @c to @string.
 *
 *       Do not use this if you are working with UTF-8 sequences,
 *       use bson_string_append_unichar().
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_string_append_c (bson_string_t *string, /* IN */
                      char           c)      /* IN */
{
   char cc[2];

   BSON_ASSERT (string);

   if (BSON_UNLIKELY (string->alloc == (string->len + 1))) {
      cc [0] = c;
      cc [1] = '\0';
      bson_string_append (string, cc);
      return;
   }

   string->str [string->len++] = c;
   string->str [string->len] = '\0';
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_string_append_unichar --
 *
 *       Append the bson_unichar_t @unichar to the string @string.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_string_append_unichar (bson_string_t  *string,  /* IN */
                            bson_unichar_t  unichar) /* IN */
{
   uint32_t len;
   char str [8];

   BSON_ASSERT (string);
   BSON_ASSERT (unichar);

   bson_utf8_from_unichar (unichar, str, &len);

   if (len <= 6) {
      str [len] = '\0';
      bson_string_append (string, str);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_string_append_printf --
 *
 *       Format a string according to @format and append it to @string.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_string_append_printf (bson_string_t *string,
                           const char    *format,
                           ...)
{
   va_list args;
   char *ret;

   BSON_ASSERT (string);
   BSON_ASSERT (format);

   va_start (args, format);
   ret = bson_strdupv_printf (format, args);
   va_end (args);
   bson_string_append (string, ret);
   bson_free (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_string_truncate --
 *
 *       Truncate the string @string to @len bytes.
 *
 *       The underlying memory will be released via realloc() down to
 *       the minimum required size specified by @len.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_string_truncate (bson_string_t *string, /* IN */
                      uint32_t       len)    /* IN */
{
   uint32_t alloc;

   BSON_ASSERT (string);
   BSON_ASSERT (len < INT_MAX);

   alloc = len + 1;

   if (alloc < 16) {
      alloc = 16;
   }

   if (!bson_is_power_of_two (alloc)) {
      alloc = (uint32_t)bson_next_power_of_two ((size_t)alloc);
   }

   string->str = bson_realloc (string->str, alloc);
   string->alloc = alloc;
   string->len = len;

   string->str [string->len] = '\0';
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strdup --
 *
 *       Portable strdup().
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_strdup (const char *str) /* IN */
{
   long len;
   char *out;

   if (!str) {
      return NULL;
   }

   len = (long)strlen (str);
   out = bson_malloc (len + 1);

   if (!out) {
      return NULL;
   }

   memcpy (out, str, len + 1);

   return out;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strdupv_printf --
 *
 *       Like bson_strdup_printf() but takes a va_list.
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_strdupv_printf (const char *format, /* IN */
                     va_list     args)   /* IN */
{
   va_list my_args;
   char *buf;
   int len = 32;
   int n;

   BSON_ASSERT (format);

   buf = bson_malloc0 (len);

   while (true) {
      va_copy (my_args, args);
      n = bson_vsnprintf (buf, len, format, my_args);
      va_end (my_args);

      if (n > -1 && n < len) {
         return buf;
      }

      if (n > -1) {
         len = n + 1;
      } else {
         len *= 2;
      }

      buf = bson_realloc (buf, len);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strdup_printf --
 *
 *       Convenience function that formats a string according to @format
 *       and returns a copy of it.
 *
 * Returns:
 *       A newly created string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_strdup_printf (const char *format, /* IN */
                    ...)                /* IN */
{
   va_list args;
   char *ret;

   BSON_ASSERT (format);

   va_start (args, format);
   ret = bson_strdupv_printf (format, args);
   va_end (args);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strndup --
 *
 *       A portable strndup().
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_strndup (const char *str,     /* IN */
              size_t      n_bytes) /* IN */
{
   char *ret;

   BSON_ASSERT (str);

   ret = bson_malloc (n_bytes + 1);
   memcpy (ret, str, n_bytes);
   ret[n_bytes] = '\0';

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strfreev --
 *
 *       Frees each string in a NULL terminated array of strings.
 *       This also frees the underlying array.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_strfreev (char **str) /* IN */
{
   int i;

   if (str) {
      for (i = 0; str [i]; i++)
         bson_free (str [i]);
      bson_free (str);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strnlen --
 *
 *       A portable strnlen().
 *
 * Returns:
 *       The length of @s up to @maxlen.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

size_t
bson_strnlen (const char *s,      /* IN */
              size_t      maxlen) /* IN */
{
#ifdef BSON_HAVE_STRNLEN
   return strnlen (s, maxlen);
#else
   size_t i;

   for (i = 0; i < maxlen; i++) {
      if (s [i] == '\0') {
         return i;
      }
   }

   return maxlen;
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_strncpy --
 *
 *       A portable strncpy.
 *
 *       Copies @src into @dst, which must be @size bytes or larger.
 *       The result is guaranteed to be \0 terminated.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_strncpy (char       *dst,  /* IN */
              const char *src,  /* IN */
              size_t      size) /* IN */
{
#ifdef _MSC_VER
   strncpy_s (dst, size, src, _TRUNCATE);
#else
   strncpy (dst, src, size);
   dst[size - 1] = '\0';
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_vsnprintf --
 *
 *       A portable vsnprintf.
 *
 *       If more than @size bytes are required (exluding the null byte),
 *       then @size bytes will be written to @string and the return value
 *       is the number of bytes required.
 *
 *       This function will always return a NULL terminated string.
 *
 * Returns:
 *       The number of bytes required for @format excluding the null byte.
 *
 * Side effects:
 *       @str is initialized with the formatted string.
 *
 *--------------------------------------------------------------------------
 */

int
bson_vsnprintf (char       *str,    /* IN */
                size_t      size,   /* IN */
                const char *format, /* IN */
                va_list     ap)     /* IN */
{
#ifdef BSON_OS_WIN32
   int r = -1;

   BSON_ASSERT (str);

   if (size != 0) {
       r = _vsnprintf_s (str, size, _TRUNCATE, format, ap);
   }

   if (r == -1) {
      r = _vscprintf (format, ap);
   }

   str [size - 1] = '\0';

   return r;
#else
   int r;

   r = vsnprintf (str, size, format, ap);
   str [size - 1] = '\0';
   return r;
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_snprintf --
 *
 *       A portable snprintf.
 *
 *       If @format requires more than @size bytes, then @size bytes are
 *       written and the result is the number of bytes required (excluding
 *       the null byte).
 *
 *       This function will always return a NULL terminated string.
 *
 * Returns:
 *       The number of bytes required for @format.
 *
 * Side effects:
 *       @str is initialized.
 *
 *--------------------------------------------------------------------------
 */

int
bson_snprintf (char       *str,    /* IN */
               size_t      size,   /* IN */
               const char *format, /* IN */
               ...)
{
   int r;
   va_list ap;

   BSON_ASSERT (str);

   va_start (ap, format);
   r = bson_vsnprintf (str, size, format, ap);
   va_end (ap);

   return r;
}


int64_t
bson_ascii_strtoll (const char  *s,
                    char       **e,
                    int          base)
{
    char *tok = (char *)s;
    char c;
    int64_t number = 0;
    int64_t sign = 1;

    if (!s) {
       errno = EINVAL;
       return 0;
    }

    c = *tok;

    while (isspace (c)) {
        c = *++tok;
    }

    if (!isdigit (c) && (c != '+') && (c != '-')) {
        *e = tok - 1;
        errno = EINVAL;
        return 0;
    }

    if (c == '-') {
        sign = -1;
        c = *++tok;
    }

    if (c == '+') {
        c = *++tok;
    }

    if (c == '0' && tok[1] != '\0' &&
       (isdigit (tok[1]) || tok[1] == 'x' || tok[1] == 'X')) {
        /* Hex, octal or binary -- maybe. */

        c = *++tok;

        if (c == 'x' || c == 'X') { /* Hex */
            if (base != 16) {
                *e = (char *)(s);
                errno = EINVAL;
                return 0;
            }

            c = *++tok;
            if (!isxdigit (c)) {
                *e = tok;
                errno = EINVAL;
                return 0;
            }
            do {
                number = (number << 4) + (c - '0');
                c = *(++tok);
            } while (isxdigit (c));
        }
        else { /* Octal */
            if (base != 8) {
                *e = (char *)(s);
                errno = EINVAL;
                return 0;
            }

            if (c < '0' || c >= '8') {
                *e = tok;
                errno = EINVAL;
                return 0;
            }
            do {
                number = (number << 3) + (c - '0');
                c = *(++tok);
            } while (('0' <= c) && (c < '8'));
        }

        while (c == 'l' || c == 'L' || c == 'u' || c == 'U') {
            c = *++tok;
        }
    }
    else {
        /* Decimal */
        if (base != 10) {
            *e = (char *)(s);
            errno = EINVAL;
            return 0;
        }

        do {
            number = (number * 10) + (c - '0');
            c = *(++tok);
        } while (isdigit (c));

        while (c == 'l' || c == 'L' || c == 'u' || c == 'U') {
            c = *(++tok);
        }
    }

    *e = tok;
    errno = 0;
    return (sign * number);
}


/*==============================================================*/
/* --- bson-utf8.c */

/*
 *--------------------------------------------------------------------------
 *
 * _bson_utf8_get_sequence --
 *
 *       Determine the sequence length of the first UTF-8 character in
 *       @utf8. The sequence length is stored in @seq_length and the mask
 *       for the first character is stored in @first_mask.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @seq_length is set.
 *       @first_mask is set.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_bson_utf8_get_sequence (const char *utf8,       /* IN */
                         uint8_t    *seq_length, /* OUT */
                         uint8_t    *first_mask) /* OUT */
{
   unsigned char c = *(const unsigned char *)utf8;
   uint8_t m;
   uint8_t n;

   /*
    * See the following[1] for a description of what the given multi-byte
    * sequences will be based on the bits set of the first byte. We also need
    * to mask the first byte based on that.  All subsequent bytes are masked
    * against 0x3F.
    *
    * [1] http://www.joelonsoftware.com/articles/Unicode.html
    */

   if ((c & 0x80) == 0) {
      n = 1;
      m = 0x7F;
   } else if ((c & 0xE0) == 0xC0) {
      n = 2;
      m = 0x1F;
   } else if ((c & 0xF0) == 0xE0) {
      n = 3;
      m = 0x0F;
   } else if ((c & 0xF8) == 0xF0) {
      n = 4;
      m = 0x07;
   } else if ((c & 0xFC) == 0xF8) {
      n = 5;
      m = 0x03;
   } else if ((c & 0xFE) == 0xFC) {
      n = 6;
      m = 0x01;
   } else {
      n = 0;
      m = 0;
   }

   *seq_length = n;
   *first_mask = m;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_validate --
 *
 *       Validates that @utf8 is a valid UTF-8 string.
 *
 *       If @allow_null is true, then \0 is allowed within @utf8_len bytes
 *       of @utf8.  Generally, this is bad practice since the main point of
 *       UTF-8 strings is that they can be used with strlen() and friends.
 *       However, some languages such as Python can send UTF-8 encoded
 *       strings with NUL's in them.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes.
 *       @allow_null: If \0 is allowed within @utf8, exclusing trailing \0.
 *
 * Returns:
 *       true if @utf8 is valid UTF-8. otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_utf8_validate (const char *utf8,       /* IN */
                    size_t      utf8_len,   /* IN */
                    bool        allow_null) /* IN */
{
   bson_unichar_t c;
   uint8_t first_mask;
   uint8_t seq_length;
   unsigned i;
   unsigned j;

   BSON_ASSERT (utf8);

   for (i = 0; i < utf8_len; i += seq_length) {
      _bson_utf8_get_sequence (&utf8[i], &seq_length, &first_mask);

      /*
       * Ensure we have a valid multi-byte sequence length.
       */
      if (!seq_length) {
         return false;
      }

      /*
       * Ensure we have enough bytes left.
       */
      if ((utf8_len - i) < seq_length) {
         return false;
      }

      /*
       * Also calculate the next char as a unichar so we can
       * check code ranges for non-shortest form.
       */
      c = utf8 [i] & first_mask;

      /*
       * Check the high-bits for each additional sequence byte.
       */
      for (j = i + 1; j < (i + seq_length); j++) {
         c = (c << 6) | (utf8 [j] & 0x3F);
         if ((utf8[j] & 0xC0) != 0x80) {
            return false;
         }
      }

      /*
       * Check for NULL bytes afterwards.
       *
       * Hint: if you want to optimize this function, starting here to do
       * this in the same pass as the data above would probably be a good
       * idea. You would add a branch into the inner loop, but save possibly
       * on cache-line bouncing on larger strings. Just a thought.
       */
      if (!allow_null) {
         for (j = 0; j < seq_length; j++) {
            if (((i + j) > utf8_len) || !utf8[i + j]) {
               return false;
            }
         }
      }

      /*
       * Code point wont fit in utf-16, not allowed.
       */
      if (c > 0x0010FFFF) {
         return false;
      }

      /*
       * Byte is in reserved range for UTF-16 high-marks
       * for surrogate pairs.
       */
      if ((c & 0xFFFFF800) == 0xD800) {
         return false;
      }

      /*
       * Check non-shortest form unicode.
       */
      switch (seq_length) {
      case 1:
         if (c <= 0x007F) {
            continue;
         }
         return false;

      case 2:
         if ((c >= 0x0080) && (c <= 0x07FF)) {
            continue;
         } else if (c == 0) {
            /* Two-byte representation for NULL. */
            continue;
         }
         return false;

      case 3:
         if (((c >= 0x0800) && (c <= 0x0FFF)) ||
             ((c >= 0x1000) && (c <= 0xFFFF))) {
            continue;
         }
         return false;

      case 4:
         if (((c >= 0x10000) && (c <= 0x3FFFF)) ||
             ((c >= 0x40000) && (c <= 0xFFFFF)) ||
             ((c >= 0x100000) && (c <= 0x10FFFF))) {
            continue;
         }
         return false;

      default:
         return false;
      }
   }

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_escape_for_json --
 *
 *       Allocates a new string matching @utf8 except that special
 *       characters in JSON will be escaped. The resulting string is also
 *       UTF-8 encoded.
 *
 *       Both " and \ characters will be escaped. Additionally, if a NUL
 *       byte is found before @utf8_len bytes, it will be converted to the
 *       two byte UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A UTF-8 encoded string.
 *       @utf8_len: The length of @utf8 in bytes or -1 if NUL terminated.
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
bson_utf8_escape_for_json (const char *utf8,     /* IN */
                           ssize_t     utf8_len) /* IN */
{
   bson_unichar_t c;
   bson_string_t *str;
   bool length_provided = true;
   const char *end;

   BSON_ASSERT (utf8);

   str = bson_string_new (NULL);

   if (utf8_len < 0) {
      length_provided = false;
      utf8_len = strlen (utf8);
   }

   end = utf8 + utf8_len;

   while (utf8 < end) {
      c = bson_utf8_get_char (utf8);

      switch (c) {
      case '\\':
      case '"':
      case '/':
         bson_string_append_c (str, '\\');
         bson_string_append_unichar (str, c);
         break;
      case '\b':
         bson_string_append (str, "\\b");
         break;
      case '\f':
         bson_string_append (str, "\\f");
         break;
      case '\n':
         bson_string_append (str, "\\n");
         break;
      case '\r':
         bson_string_append (str, "\\r");
         break;
      case '\t':
         bson_string_append (str, "\\t");
         break;
      default:
         if (c < ' ') {
            bson_string_append_printf (str, "\\u%04u", (unsigned)c);
         } else {
            bson_string_append_unichar (str, c);
         }
         break;
      }

      if (c) {
         utf8 = bson_utf8_next_char (utf8);
      } else {
         if (length_provided && !*utf8) {
            /* we escaped nil as '\u0000', now advance past it */
            utf8++;
         } else {
            /* invalid UTF-8 */
            bson_string_free (str, true);
            return NULL;
         }
      }
   }

   return bson_string_free (str, false);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_get_char --
 *
 *       Fetches the next UTF-8 character from the UTF-8 sequence.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       A 32-bit bson_unichar_t reprsenting the multi-byte sequence.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_unichar_t
bson_utf8_get_char (const char *utf8) /* IN */
{
   bson_unichar_t c;
   uint8_t mask;
   uint8_t num;
   int i;

   BSON_ASSERT (utf8);

   _bson_utf8_get_sequence (utf8, &num, &mask);
   c = (*utf8) & mask;

   for (i = 1; i < num; i++) {
      c = (c << 6) | (utf8[i] & 0x3F);
   }

   return c;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_next_char --
 *
 *       Returns an incremented pointer to the beginning of the next
 *       multi-byte sequence in @utf8.
 *
 * Parameters:
 *       @utf8: A string containing validated UTF-8.
 *
 * Returns:
 *       An incremented pointer in @utf8.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
bson_utf8_next_char (const char *utf8) /* IN */
{
   uint8_t mask;
   uint8_t num;

   BSON_ASSERT (utf8);

   _bson_utf8_get_sequence (utf8, &num, &mask);

   return utf8 + num;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_utf8_from_unichar --
 *
 *       Converts the unichar to a sequence of utf8 bytes and stores those
 *       in @utf8. The number of bytes in the sequence are stored in @len.
 *
 * Parameters:
 *       @unichar: A bson_unichar_t.
 *       @utf8: A location for the multi-byte sequence.
 *       @len: A location for number of bytes stored in @utf8.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @utf8 is set.
 *       @len is set.
 *
 *--------------------------------------------------------------------------
 */

void
bson_utf8_from_unichar (
      bson_unichar_t  unichar,                               /* IN */
      char            utf8[BSON_ENSURE_ARRAY_PARAM_SIZE(6)], /* OUT */
      uint32_t       *len)                                   /* OUT */
{
   BSON_ASSERT (utf8);
   BSON_ASSERT (len);

   if (unichar <= 0x7F) {
      utf8[0] = unichar;
      *len = 1;
   } else if (unichar <= 0x7FF) {
      *len = 2;
      utf8[0] = 0xC0 | ((unichar >> 6) & 0x3F);
      utf8[1] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0xFFFF) {
      *len = 3;
      utf8[0] = 0xE0 | ((unichar >> 12) & 0xF);
      utf8[1] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[2] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x1FFFFF) {
      *len = 4;
      utf8[0] = 0xF0 | ((unichar >> 18) & 0x7);
      utf8[1] = 0x80 | ((unichar >> 12) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[3] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x3FFFFFF) {
      *len = 5;
      utf8[0] = 0xF8 | ((unichar >> 24) & 0x3);
      utf8[1] = 0x80 | ((unichar >> 18) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 12) & 0x3F);
      utf8[3] = 0x80 | ((unichar >> 6) & 0x3F);
      utf8[4] = 0x80 | ((unichar) & 0x3F);
   } else if (unichar <= 0x7FFFFFFF) {
      *len = 6;
      utf8[0] = 0xFC | ((unichar >> 31) & 0x1);
      utf8[1] = 0x80 | ((unichar >> 25) & 0x3F);
      utf8[2] = 0x80 | ((unichar >> 19) & 0x3F);
      utf8[3] = 0x80 | ((unichar >> 13) & 0x3F);
      utf8[4] = 0x80 | ((unichar >> 7) & 0x3F);
      utf8[5] = 0x80 | ((unichar) & 0x1);
   } else {
      *len = 0;
   }
}

/*==============================================================*/
/* --- bson-value.c */

void
bson_value_copy (const bson_value_t *src, /* IN */
                 bson_value_t       *dst) /* OUT */
{
   BSON_ASSERT (src);
   BSON_ASSERT (dst);

   dst->value_type = src->value_type;

   switch (src->value_type) {
   case BSON_TYPE_DOUBLE:
      dst->value.v_double = src->value.v_double;
      break;
   case BSON_TYPE_UTF8:
      dst->value.v_utf8.len = src->value.v_utf8.len;
      dst->value.v_utf8.str = bson_malloc (src->value.v_utf8.len + 1);
      memcpy (dst->value.v_utf8.str,
              src->value.v_utf8.str,
              dst->value.v_utf8.len);
      dst->value.v_utf8.str [dst->value.v_utf8.len] = '\0';
      break;
   case BSON_TYPE_DOCUMENT:
   case BSON_TYPE_ARRAY:
      dst->value.v_doc.data_len = src->value.v_doc.data_len;
      dst->value.v_doc.data = bson_malloc (src->value.v_doc.data_len);
      memcpy (dst->value.v_doc.data,
              src->value.v_doc.data,
              dst->value.v_doc.data_len);
      break;
   case BSON_TYPE_BINARY:
      dst->value.v_binary.subtype = src->value.v_binary.subtype;
      dst->value.v_binary.data_len = src->value.v_binary.data_len;
      dst->value.v_binary.data = bson_malloc (src->value.v_binary.data_len);
      memcpy (dst->value.v_binary.data,
              src->value.v_binary.data,
              dst->value.v_binary.data_len);
      break;
   case BSON_TYPE_OID:
      bson_oid_copy (&src->value.v_oid, &dst->value.v_oid);
      break;
   case BSON_TYPE_BOOL:
      dst->value.v_bool = src->value.v_bool;
      break;
   case BSON_TYPE_DATE_TIME:
      dst->value.v_datetime = src->value.v_datetime;
      break;
   case BSON_TYPE_REGEX:
      dst->value.v_regex.regex = bson_strdup (src->value.v_regex.regex);
      dst->value.v_regex.options = bson_strdup (src->value.v_regex.options);
      break;
   case BSON_TYPE_DBPOINTER:
      dst->value.v_dbpointer.collection_len = src->value.v_dbpointer.collection_len;
      dst->value.v_dbpointer.collection = bson_malloc (src->value.v_dbpointer.collection_len + 1);
      memcpy (dst->value.v_dbpointer.collection,
              src->value.v_dbpointer.collection,
              dst->value.v_dbpointer.collection_len);
      dst->value.v_dbpointer.collection [dst->value.v_dbpointer.collection_len] = '\0';
      bson_oid_copy (&src->value.v_dbpointer.oid, &dst->value.v_dbpointer.oid);
      break;
   case BSON_TYPE_CODE:
      dst->value.v_code.code_len = src->value.v_code.code_len;
      dst->value.v_code.code = bson_malloc (src->value.v_code.code_len + 1);
      memcpy (dst->value.v_code.code,
              src->value.v_code.code,
              dst->value.v_code.code_len);
      dst->value.v_code.code [dst->value.v_code.code_len] = '\0';
      break;
   case BSON_TYPE_SYMBOL:
      dst->value.v_symbol.len = src->value.v_symbol.len;
      dst->value.v_symbol.symbol = bson_malloc (src->value.v_symbol.len + 1);
      memcpy (dst->value.v_symbol.symbol,
              src->value.v_symbol.symbol,
              dst->value.v_symbol.len);
      dst->value.v_symbol.symbol [dst->value.v_symbol.len] = '\0';
      break;
   case BSON_TYPE_CODEWSCOPE:
      dst->value.v_codewscope.code_len = src->value.v_codewscope.code_len;
      dst->value.v_codewscope.code = bson_malloc (src->value.v_codewscope.code_len + 1);
      memcpy (dst->value.v_codewscope.code,
              src->value.v_codewscope.code,
              dst->value.v_codewscope.code_len);
      dst->value.v_codewscope.code [dst->value.v_codewscope.code_len] = '\0';
      dst->value.v_codewscope.scope_len = src->value.v_codewscope.scope_len;
      dst->value.v_codewscope.scope_data = bson_malloc (src->value.v_codewscope.scope_len);
      memcpy (dst->value.v_codewscope.scope_data,
              src->value.v_codewscope.scope_data,
              dst->value.v_codewscope.scope_len);
      break;
   case BSON_TYPE_INT32:
      dst->value.v_int32 = src->value.v_int32;
      break;
   case BSON_TYPE_TIMESTAMP:
      dst->value.v_timestamp.timestamp = src->value.v_timestamp.timestamp;
      dst->value.v_timestamp.increment = src->value.v_timestamp.increment;
      break;
   case BSON_TYPE_INT64:
      dst->value.v_int64 = src->value.v_int64;
      break;
   case BSON_TYPE_UNDEFINED:
   case BSON_TYPE_NULL:
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
      break;
   case BSON_TYPE_EOD:
   default:
      BSON_ASSERT (false);
      return;
   }
}


void
bson_value_destroy (bson_value_t *value) /* IN */
{
   switch (value->value_type) {
   case BSON_TYPE_UTF8:
      bson_free (value->value.v_utf8.str);
      break;
   case BSON_TYPE_DOCUMENT:
   case BSON_TYPE_ARRAY:
      bson_free (value->value.v_doc.data);
      break;
   case BSON_TYPE_BINARY:
      bson_free (value->value.v_binary.data);
      break;
   case BSON_TYPE_REGEX:
      bson_free (value->value.v_regex.regex);
      bson_free (value->value.v_regex.options);
      break;
   case BSON_TYPE_DBPOINTER:
      bson_free (value->value.v_dbpointer.collection);
      break;
   case BSON_TYPE_CODE:
      bson_free (value->value.v_code.code);
      break;
   case BSON_TYPE_SYMBOL:
      bson_free (value->value.v_symbol.symbol);
      break;
   case BSON_TYPE_CODEWSCOPE:
      bson_free (value->value.v_codewscope.code);
      bson_free (value->value.v_codewscope.scope_data);
      break;
   case BSON_TYPE_DOUBLE:
   case BSON_TYPE_UNDEFINED:
   case BSON_TYPE_OID:
   case BSON_TYPE_BOOL:
   case BSON_TYPE_DATE_TIME:
   case BSON_TYPE_NULL:
   case BSON_TYPE_INT32:
   case BSON_TYPE_TIMESTAMP:
   case BSON_TYPE_INT64:
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_MINKEY:
   case BSON_TYPE_EOD:
   default:
      break;
   }
}

/*==============================================================*/
/* --- bson-version-functions.c */

/**
 * bson_get_major_version:
 *
 * Helper function to return the runtime major version of the library.
 */
int
bson_get_major_version (void)
{
   return BSON_MAJOR_VERSION;
}


/**
 * bson_get_minor_version:
 *
 * Helper function to return the runtime minor version of the library.
 */
int
bson_get_minor_version (void)
{
   return BSON_MINOR_VERSION;
}

/**
 * bson_get_micro_version:
 *
 * Helper function to return the runtime micro version of the library.
 */
int
bson_get_micro_version (void)
{
   return BSON_MICRO_VERSION;
}

/**
 * bson_get_version:
 *
 * Helper function to return the runtime string version of the library.
 */
const char *
bson_get_version (void)
{
   return BSON_VERSION_S;
}

/**
 * bson_check_version:
 *
 * True if libmongoc's version is greater than or equal to the required
 * version.
 */
bool
bson_check_version (int required_major,
                    int required_minor,
                    int required_micro)
{
   return BSON_CHECK_VERSION(required_major, required_minor, required_micro);
}

/*==============================================================*/
/* --- bson-writer.c */

struct _bson_writer_t
{
   bool                ready;
   uint8_t           **buf;
   size_t             *buflen;
   size_t              offset;
   bson_realloc_func   realloc_func;
   void               *realloc_func_ctx;
   bson_t              b;
};


/*
 *--------------------------------------------------------------------------
 *
 * bson_writer_new --
 *
 *       Creates a new instance of bson_writer_t using the buffer, length,
 *       offset, and realloc() function supplied.
 *
 *       The caller is expected to clean up the structure when finished
 *       using bson_writer_destroy().
 *
 * Parameters:
 *       @buf: (inout): A pointer to a target buffer.
 *       @buflen: (inout): A pointer to the buffer length.
 *       @offset: The offset in the target buffer to start from.
 *       @realloc_func: A realloc() style function or NULL.
 *
 * Returns:
 *       A newly allocated bson_writer_t that should be freed with
 *       bson_writer_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_writer_t *
bson_writer_new (uint8_t           **buf,              /* IN */
                 size_t             *buflen,           /* IN */
                 size_t              offset,           /* IN */
                 bson_realloc_func   realloc_func,     /* IN */
                 void               *realloc_func_ctx) /* IN */
{
   bson_writer_t *writer;

   writer = bson_malloc0 (sizeof *writer);
   writer->buf = buf;
   writer->buflen = buflen;
   writer->offset = offset;
   writer->realloc_func = realloc_func;
   writer->realloc_func_ctx = realloc_func_ctx;
   writer->ready = true;

   return writer;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_writer_destroy --
 *
 *       Cleanup after @writer and release any allocated memory. Note that
 *       the buffer supplied to bson_writer_new() is NOT freed from this
 *       method.  The caller is responsible for that.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_writer_destroy (bson_writer_t *writer) /* IN */
{
   bson_free (writer);
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_writer_get_length --
 *
 *       Fetches the current length of the content written by the buffer
 *       (including the initial offset). This includes a partly written
 *       document currently being written.
 *
 *       This is useful if you want to check to see if you've passed a given
 *       memory boundry that cannot be sent in a packet. See
 *       bson_writer_rollback() to abort the current document being written.
 *
 * Returns:
 *       The number of bytes written plus initial offset.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

size_t
bson_writer_get_length (bson_writer_t *writer) /* IN */
{
   return writer->offset + writer->b.len;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_writer_begin --
 *
 *       Begins writing a new document. The caller may use the bson
 *       structure to write out a new BSON document. When completed, the
 *       caller must call either bson_writer_end() or
 *       bson_writer_rollback().
 *
 * Parameters:
 *       @writer: A bson_writer_t.
 *       @bson: (out): A location for a bson_t*.
 *
 * Returns:
 *       true if the underlying realloc was successful; otherwise false.
 *
 * Side effects:
 *       @bson is initialized if true is returned.
 *
 *--------------------------------------------------------------------------
 */

bool
bson_writer_begin (bson_writer_t  *writer, /* IN */
                   bson_t        **bson)   /* OUT */
{
   bson_impl_alloc_t *b;
   bool grown = false;

   BSON_ASSERT (writer);
   BSON_ASSERT (writer->ready);
   BSON_ASSERT (bson);

   writer->ready = false;

   memset (&writer->b, 0, sizeof (bson_t));

   b = (bson_impl_alloc_t *)&writer->b;
   b->flags = BSON_FLAG_STATIC | BSON_FLAG_NO_FREE;
   b->len = 5;
   b->parent = NULL;
   b->buf = writer->buf;
   b->buflen = writer->buflen;
   b->offset = writer->offset;
   b->alloc = NULL;
   b->alloclen = 0;
   b->realloc = writer->realloc_func;
   b->realloc_func_ctx = writer->realloc_func_ctx;

   while ((writer->offset + writer->b.len) > *writer->buflen) {
      if (!writer->realloc_func) {
         memset (&writer->b, 0, sizeof (bson_t));
         writer->ready = true;
         return false;
      }
      grown = true;

      if (!*writer->buflen) {
         *writer->buflen = 64;
      } else {
         (*writer->buflen) *= 2;
      }
   }

   if (grown) {
      *writer->buf = writer->realloc_func (*writer->buf, *writer->buflen, writer->realloc_func_ctx);
   }

   memset ((*writer->buf) + writer->offset + 1, 0, 5);
   (*writer->buf)[writer->offset] = 5;

   *bson = &writer->b;

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_writer_end --
 *
 *       Complete writing of a bson_writer_t to the buffer supplied.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_writer_end (bson_writer_t *writer) /* IN */
{
   BSON_ASSERT (writer);
   BSON_ASSERT (!writer->ready);

   writer->offset += writer->b.len;
   memset (&writer->b, 0, sizeof (bson_t));
   writer->ready = true;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_writer_rollback --
 *
 *       Abort the appending of the current bson_t to the memory region
 *       managed by @writer.  This is useful if you detected that you went
 *       past a particular memory limit.  For example, MongoDB has 48MB
 *       message limits.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_writer_rollback (bson_writer_t *writer) /* IN */
{
   BSON_ASSERT (writer);

   if (writer->b.len) {
      memset (&writer->b, 0, sizeof (bson_t));
   }

   writer->ready = true;
}

/*==============================================================*/
/* --- bson-atomic.c */

/*
 * We should only ever hit these on non-Windows systems, for which we require
 * pthread support. Therefore, we will avoid making a threading portability
 * for threads here and just use pthreads directly.
 */


#ifdef __BSON_NEED_BARRIER
#include <pthread.h>
static pthread_mutex_t gBarrier = PTHREAD_MUTEX_INITIALIZER;
void
bson_memory_barrier (void)
{
   pthread_mutex_lock (&gBarrier);
   pthread_mutex_unlock (&gBarrier);
}
#endif


#ifdef __BSON_NEED_ATOMIC_32
#warning "Using mutex to emulate 32-bit atomics."
#include <pthread.h>
static pthread_mutex_t gSync32 = PTHREAD_MUTEX_INITIALIZER;
int32_t
bson_atomic_int_add (volatile int32_t *p,
                     int32_t           n)
{
   int ret;

   pthread_mutex_lock (&gSync32);
   *p += n;
   ret = *p;
   pthread_mutex_unlock (&gSync32);

   return ret;
}
#endif


#ifdef __BSON_NEED_ATOMIC_64
#include <pthread.h>
static pthread_mutex_t gSync64 = PTHREAD_MUTEX_INITIALIZER;
int64_t
bson_atomic_int64_add (volatile int64_t *p,
                       int64_t           n)
{
   int64_t ret;

   pthread_mutex_lock (&gSync64);
   *p += n;
   ret = *p;
   pthread_mutex_unlock (&gSync64);

   return ret;
}
#endif

/*==============================================================*/
/* --- bson-timegm.c */

#ifndef BSON_OS_WIN32

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX. */
#define is_digit(c) ((unsigned)(c) - '0' <= 9)

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#if 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
# define ATTRIBUTE_CONST __attribute__ ((const))
# define ATTRIBUTE_PURE __attribute__ ((__pure__))
# define ATTRIBUTE_FORMAT(spec) __attribute__ ((__format__ spec))
#else
# define ATTRIBUTE_CONST /* empty */
# define ATTRIBUTE_PURE /* empty */
# define ATTRIBUTE_FORMAT(spec) /* empty */
#endif

#if !defined _Noreturn && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112)
# if 2 < __GNUC__ + (8 <= __GNUC_MINOR__)
#  define _Noreturn __attribute__ ((__noreturn__))
# else
#  define _Noreturn
# endif
#endif

#if (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901) && !defined restrict
# define restrict /* empty */
#endif

#ifndef TYPE_BIT
#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif /* !defined TYPE_BIT */

#ifndef TYPE_SIGNED
#define TYPE_SIGNED(type) (((type) -1) < 0)
#endif /* !defined TYPE_SIGNED */

/* The minimum and maximum finite time values.  */
static time_t const time_t_min =
    (TYPE_SIGNED(time_t)
        ? (time_t) -1 << (CHAR_BIT * sizeof (time_t) - 1)
        : 0);
static time_t const time_t_max =
    (TYPE_SIGNED(time_t)
        ? - (~ 0 < 0) - ((time_t) -1 << (CHAR_BIT * sizeof (time_t) - 1))
        : -1);


#ifndef TZ_MAX_TIMES
#define TZ_MAX_TIMES	2000
#endif /* !defined TZ_MAX_TIMES */

#ifndef TZ_MAX_TYPES
/* This must be at least 17 for Europe/Samara and Europe/Vilnius.  */
#define TZ_MAX_TYPES	256 /* Limited by what (unsigned char)'s can hold */
#endif /* !defined TZ_MAX_TYPES */

#ifndef TZ_MAX_CHARS
#define TZ_MAX_CHARS	50	/* Maximum number of abbreviation characters */
				/* (limited by what unsigned chars can hold) */
#endif /* !defined TZ_MAX_CHARS */

#ifndef TZ_MAX_LEAPS
#define TZ_MAX_LEAPS	50	/* Maximum number of leap second corrections */
#endif /* !defined TZ_MAX_LEAPS */

#define SECSPERMIN	60
#define MINSPERHOUR	60
#define HOURSPERDAY	24
#define DAYSPERWEEK	7
#define DAYSPERNYEAR	365
#define DAYSPERLYEAR	366
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	((int_fast32_t) SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR	12

#define TM_SUNDAY	0
#define TM_MONDAY	1
#define TM_TUESDAY	2
#define TM_WEDNESDAY	3
#define TM_THURSDAY	4
#define TM_FRIDAY	5
#define TM_SATURDAY	6

#define TM_JANUARY	0
#define TM_FEBRUARY	1
#define TM_MARCH	2
#define TM_APRIL	3
#define TM_MAY		4
#define TM_JUNE		5
#define TM_JULY		6
#define TM_AUGUST	7
#define TM_SEPTEMBER	8
#define TM_OCTOBER	9
#define TM_NOVEMBER	10
#define TM_DECEMBER	11

#define TM_YEAR_BASE	1900

#define EPOCH_YEAR	1970
#define EPOCH_WDAY	TM_THURSDAY

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/*
** Since everything in isleap is modulo 400 (or a factor of 400), we know that
**	isleap(y) == isleap(y % 400)
** and so
**	isleap(a + b) == isleap((a + b) % 400)
** or
**	isleap(a + b) == isleap(a % 400 + b % 400)
** This is true even if % means modulo rather than Fortran remainder
** (which is allowed by C89 but not C99).
** We use this to avoid addition overflow problems.
*/

#define isleap_sum(a, b)	isleap((a) % 400 + (b) % 400)

#ifndef TZ_ABBR_MAX_LEN
#define TZ_ABBR_MAX_LEN	16
#endif /* !defined TZ_ABBR_MAX_LEN */

#ifndef TZ_ABBR_CHAR_SET
#define TZ_ABBR_CHAR_SET \
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :+-._"
#endif /* !defined TZ_ABBR_CHAR_SET */

#ifndef TZ_ABBR_ERR_CHAR
#define TZ_ABBR_ERR_CHAR	'_'
#endif /* !defined TZ_ABBR_ERR_CHAR */

#ifndef WILDABBR
/*
** Someone might make incorrect use of a time zone abbreviation:
**	1.	They might reference tzname[0] before calling tzset (explicitly
**		or implicitly).
**	2.	They might reference tzname[1] before calling tzset (explicitly
**		or implicitly).
**	3.	They might reference tzname[1] after setting to a time zone
**		in which Daylight Saving Time is never observed.
**	4.	They might reference tzname[0] after setting to a time zone
**		in which Standard Time is never observed.
**	5.	They might reference tm.TM_ZONE after calling offtime.
** What's best to do in the above cases is open to debate;
** for now, we just set things up so that in any of the five cases
** WILDABBR is used. Another possibility: initialize tzname[0] to the
** string "tzname[0] used before set", and similarly for the other cases.
** And another: initialize tzname[0] to "ERA", with an explanation in the
** manual page of what this "time zone abbreviation" means (doing this so
** that tzname[0] has the "normal" length of three characters).
*/
#define WILDABBR	"   "
#endif /* !defined WILDABBR */

#ifdef TM_ZONE
static const char	wildabbr[] = WILDABBR;
#endif

static const char	gmt[] = "GMT";

struct ttinfo {				/* time type information */
	int_fast32_t	tt_gmtoff;	/* UT offset in seconds */
	int		tt_isdst;	/* used to set tm_isdst */
	int		tt_abbrind;	/* abbreviation list index */
	int		tt_ttisstd;	/* true if transition is std time */
	int		tt_ttisgmt;	/* true if transition is UT */
};

struct lsinfo {				/* leap second information */
	time_t		ls_trans;	/* transition time */
	int_fast64_t	ls_corr;	/* correction to apply */
};

#define BIGGEST(a, b)	(((a) > (b)) ? (a) : (b))

#ifdef TZNAME_MAX
#define MY_TZNAME_MAX	TZNAME_MAX
#endif /* defined TZNAME_MAX */
#ifndef TZNAME_MAX
#define MY_TZNAME_MAX	255
#endif /* !defined TZNAME_MAX */

struct state {
	int		leapcnt;
	int		timecnt;
	int		typecnt;
	int		charcnt;
	int		goback;
	int		goahead;
	time_t		ats[TZ_MAX_TIMES];
	unsigned char	types[TZ_MAX_TIMES];
	struct ttinfo	ttis[TZ_MAX_TYPES];
	char		chars[BIGGEST(BIGGEST(TZ_MAX_CHARS + 1, sizeof gmt),
				(2 * (MY_TZNAME_MAX + 1)))];
	struct lsinfo	lsis[TZ_MAX_LEAPS];
	int		defaulttype; /* for early times or if no transitions */
};

struct rule {
	int		r_type;		/* type of rule--see below */
	int		r_day;		/* day number of rule */
	int		r_week;		/* week number of rule */
	int		r_mon;		/* month number of rule */
	int_fast32_t	r_time;		/* transition time of rule */
};

#define JULIAN_DAY		0	/* Jn - Julian day */
#define DAY_OF_YEAR		1	/* n - day of year */
#define MONTH_NTH_DAY_OF_WEEK	2	/* Mm.n.d - month, week, day of week */

/*
** Prototypes for static functions.
*/

static void		gmtload(struct state * sp);
static struct tm *	gmtsub(const time_t * timep, int_fast32_t offset,
				struct tm * tmp);
static int		increment_overflow(int * number, int delta);
static int		leaps_thru_end_of(int y) ATTRIBUTE_PURE;
static int		increment_overflow32(int_fast32_t * number, int delta);
static int		normalize_overflow32(int_fast32_t * tensptr,
				int * unitsptr, int base);
static int		normalize_overflow(int * tensptr, int * unitsptr,
				int base);
static time_t		time1(struct tm * tmp,
				struct tm * (*funcp)(const time_t *,
				int_fast32_t, struct tm *),
				int_fast32_t offset);
static time_t		time2(struct tm *tmp,
				struct tm * (*funcp)(const time_t *,
				int_fast32_t, struct tm*),
				int_fast32_t offset, int * okayp);
static time_t		time2sub(struct tm *tmp,
				struct tm * (*funcp)(const time_t *,
				int_fast32_t, struct tm*),
				int_fast32_t offset, int * okayp, int do_norm_secs);
static struct tm *	timesub(const time_t * timep, int_fast32_t offset,
				const struct state * sp, struct tm * tmp);
static int		tmcomp(const struct tm * atmp,
				const struct tm * btmp);

static struct state	gmtmem;
#define gmtptr		(&gmtmem)

static int		gmt_is_set;

static const int	mon_lengths[2][MONSPERYEAR] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const int	year_lengths[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

static void
gmtload(struct state *const sp)
{
    memset(sp, 0, sizeof(struct state));
    sp->typecnt = 1;
    sp->charcnt = 4;
    sp->chars[0] = 'G';
    sp->chars[1] = 'M';
    sp->chars[2] = 'T';
}

/*
** gmtsub is to gmtime as localsub is to localtime.
*/

static struct tm *
gmtsub(const time_t *const timep, const int_fast32_t offset,
       struct tm *const tmp)
{
	register struct tm *	result;

	if (!gmt_is_set) {
		gmt_is_set = true;
//		if (gmtptr != NULL)
			gmtload(gmtptr);
	}
	result = timesub(timep, offset, gmtptr, tmp);
#ifdef TM_ZONE
	/*
	** Could get fancy here and deliver something such as
	** "UT+xxxx" or "UT-xxxx" if offset is non-zero,
	** but this is no time for a treasure hunt.
	*/
	tmp->TM_ZONE = offset ? wildabbr : gmtptr ? gmtptr->chars : gmt;
#endif /* defined TM_ZONE */
	return result;
}

/*
** Return the number of leap years through the end of the given year
** where, to make the math easy, the answer for year zero is defined as zero.
*/

static int
leaps_thru_end_of(register const int y)
{
	return (y >= 0) ? (y / 4 - y / 100 + y / 400) :
		-(leaps_thru_end_of(-(y + 1)) + 1);
}

static struct tm *
timesub(const time_t *const timep, const int_fast32_t offset,
	register const struct state *const sp,
	register struct tm *const tmp)
{
	register const struct lsinfo *	lp;
	register time_t			tdays;
	register int			idays;	/* unsigned would be so 2003 */
	register int_fast64_t		rem;
	int				y;
	register const int *		ip;
	register int_fast64_t		corr;
	register int			hit;
	register int			i;

	corr = 0;
	hit = 0;
	i = (sp == NULL) ? 0 : sp->leapcnt;
	while (--i >= 0) {
		lp = &sp->lsis[i];
		if (*timep >= lp->ls_trans) {
			if (*timep == lp->ls_trans) {
				hit = ((i == 0 && lp->ls_corr > 0) ||
					lp->ls_corr > sp->lsis[i - 1].ls_corr);
				if (hit)
					while (i > 0 &&
						sp->lsis[i].ls_trans ==
						sp->lsis[i - 1].ls_trans + 1 &&
						sp->lsis[i].ls_corr ==
						sp->lsis[i - 1].ls_corr + 1) {
							++hit;
							--i;
					}
			}
			corr = lp->ls_corr;
			break;
		}
	}
	y = EPOCH_YEAR;
	tdays = *timep / SECSPERDAY;
	rem = *timep - tdays * SECSPERDAY;
	while (tdays < 0 || tdays >= year_lengths[isleap(y)]) {
		int		newy;
		register time_t	tdelta;
		register int	idelta;
		register int	leapdays;

		tdelta = tdays / DAYSPERLYEAR;
		if (! ((! TYPE_SIGNED(time_t) || INT_MIN <= tdelta)
		       && tdelta <= INT_MAX))
			return NULL;
		idelta = (int) tdelta;
		if (idelta == 0)
			idelta = (tdays < 0) ? -1 : 1;
		newy = y;
		if (increment_overflow(&newy, idelta))
			return NULL;
		leapdays = leaps_thru_end_of(newy - 1) -
			leaps_thru_end_of(y - 1);
		tdays -= ((time_t) newy - y) * DAYSPERNYEAR;
		tdays -= leapdays;
		y = newy;
	}
	{
		register int_fast32_t	seconds;

		seconds = (int_fast32_t) (tdays * SECSPERDAY);
		tdays = seconds / SECSPERDAY;
		rem += seconds - tdays * SECSPERDAY;
	}
	/*
	** Given the range, we can now fearlessly cast...
	*/
	idays = (int) tdays;
	rem += offset - corr;
	while (rem < 0) {
		rem += SECSPERDAY;
		--idays;
	}
	while (rem >= SECSPERDAY) {
		rem -= SECSPERDAY;
		++idays;
	}
	while (idays < 0) {
		if (increment_overflow(&y, -1))
			return NULL;
		idays += year_lengths[isleap(y)];
	}
	while (idays >= year_lengths[isleap(y)]) {
		idays -= year_lengths[isleap(y)];
		if (increment_overflow(&y, 1))
			return NULL;
	}
	tmp->tm_year = y;
	if (increment_overflow(&tmp->tm_year, -TM_YEAR_BASE))
		return NULL;
	tmp->tm_yday = idays;
	/*
	** The "extra" mods below avoid overflow problems.
	*/
	tmp->tm_wday = EPOCH_WDAY +
		((y - EPOCH_YEAR) % DAYSPERWEEK) *
		(DAYSPERNYEAR % DAYSPERWEEK) +
		leaps_thru_end_of(y - 1) -
		leaps_thru_end_of(EPOCH_YEAR - 1) +
		idays;
	tmp->tm_wday %= DAYSPERWEEK;
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYSPERWEEK;
	tmp->tm_hour = (int) (rem / SECSPERHOUR);
	rem %= SECSPERHOUR;
	tmp->tm_min = (int) (rem / SECSPERMIN);
	/*
	** A positive leap second requires a special
	** representation. This uses "... ??:59:60" et seq.
	*/
	tmp->tm_sec = (int) (rem % SECSPERMIN) + hit;
	ip = mon_lengths[isleap(y)];
	for (tmp->tm_mon = 0; idays >= ip[tmp->tm_mon]; ++(tmp->tm_mon))
		idays -= ip[tmp->tm_mon];
	tmp->tm_mday = (int) (idays + 1);
	tmp->tm_isdst = 0;
#ifdef TM_GMTOFF
	tmp->TM_GMTOFF = offset;
#endif /* defined TM_GMTOFF */
	return tmp;
}

/*
** Adapted from code provided by Robert Elz, who writes:
**	The "best" way to do mktime I think is based on an idea of Bob
**	Kridle's (so its said...) from a long time ago.
**	It does a binary search of the time_t space. Since time_t's are
**	just 32 bits, its a max of 32 iterations (even at 64 bits it
**	would still be very reasonable).
*/

#ifndef WRONG
#define WRONG	(-1)
#endif /* !defined WRONG */

/*
** Normalize logic courtesy Paul Eggert.
*/

static int
increment_overflow(int *const ip, int j)
{
	register int const	i = *ip;

	/*
	** If i >= 0 there can only be overflow if i + j > INT_MAX
	** or if j > INT_MAX - i; given i >= 0, INT_MAX - i cannot overflow.
	** If i < 0 there can only be overflow if i + j < INT_MIN
	** or if j < INT_MIN - i; given i < 0, INT_MIN - i cannot overflow.
	*/
	if ((i >= 0) ? (j > INT_MAX - i) : (j < INT_MIN - i))
		return true;
	*ip += j;
	return false;
}

static int
increment_overflow32(int_fast32_t *const lp, int const m)
{
	register int_fast32_t const	l = *lp;

	if ((l >= 0) ? (m > INT_FAST32_MAX - l) : (m < INT_FAST32_MIN - l))
		return true;
	*lp += m;
	return false;
}

static int
normalize_overflow(int *const tensptr, int *const unitsptr, const int base)
{
	register int	tensdelta;

	tensdelta = (*unitsptr >= 0) ?
		(*unitsptr / base) :
		(-1 - (-1 - *unitsptr) / base);
	*unitsptr -= tensdelta * base;
	return increment_overflow(tensptr, tensdelta);
}

static int
normalize_overflow32(int_fast32_t *const tensptr, int *const unitsptr,
		     const int base)
{
	register int	tensdelta;

	tensdelta = (*unitsptr >= 0) ?
		(*unitsptr / base) :
		(-1 - (-1 - *unitsptr) / base);
	*unitsptr -= tensdelta * base;
	return increment_overflow32(tensptr, tensdelta);
}

static int
tmcomp(register const struct tm *const atmp,
       register const struct tm *const btmp)
{
	register int	result;

	if (atmp->tm_year != btmp->tm_year)
		return atmp->tm_year < btmp->tm_year ? -1 : 1;
	if ((result = (atmp->tm_mon - btmp->tm_mon)) == 0 &&
		(result = (atmp->tm_mday - btmp->tm_mday)) == 0 &&
		(result = (atmp->tm_hour - btmp->tm_hour)) == 0 &&
		(result = (atmp->tm_min - btmp->tm_min)) == 0)
			result = atmp->tm_sec - btmp->tm_sec;
	return result;
}

static time_t
time2sub(struct tm *const tmp,
	 struct tm *(*const funcp)(const time_t *, int_fast32_t, struct tm *),
	 const int_fast32_t offset,
	 int *const okayp,
	 const int do_norm_secs)
{
	register const struct state *	sp;
	register int			dir;
	register int			i, j;
	register int			saved_seconds;
	register int_fast32_t			li;
	register time_t			lo;
	register time_t			hi;
	int_fast32_t				y;
	time_t				newt;
	time_t				t;
	struct tm			yourtm, mytm;

	*okayp = false;
	yourtm = *tmp;
	if (do_norm_secs) {
		if (normalize_overflow(&yourtm.tm_min, &yourtm.tm_sec,
			SECSPERMIN))
				return WRONG;
	}
	if (normalize_overflow(&yourtm.tm_hour, &yourtm.tm_min, MINSPERHOUR))
		return WRONG;
	if (normalize_overflow(&yourtm.tm_mday, &yourtm.tm_hour, HOURSPERDAY))
		return WRONG;
	y = yourtm.tm_year;
	if (normalize_overflow32(&y, &yourtm.tm_mon, MONSPERYEAR))
		return WRONG;
	/*
	** Turn y into an actual year number for now.
	** It is converted back to an offset from TM_YEAR_BASE later.
	*/
	if (increment_overflow32(&y, TM_YEAR_BASE))
		return WRONG;
	while (yourtm.tm_mday <= 0) {
		if (increment_overflow32(&y, -1))
			return WRONG;
		li = y + (1 < yourtm.tm_mon);
		yourtm.tm_mday += year_lengths[isleap(li)];
	}
	while (yourtm.tm_mday > DAYSPERLYEAR) {
		li = y + (1 < yourtm.tm_mon);
		yourtm.tm_mday -= year_lengths[isleap(li)];
		if (increment_overflow32(&y, 1))
			return WRONG;
	}
	for ( ; ; ) {
		i = mon_lengths[isleap(y)][yourtm.tm_mon];
		if (yourtm.tm_mday <= i)
			break;
		yourtm.tm_mday -= i;
		if (++yourtm.tm_mon >= MONSPERYEAR) {
			yourtm.tm_mon = 0;
			if (increment_overflow32(&y, 1))
				return WRONG;
		}
	}
	if (increment_overflow32(&y, -TM_YEAR_BASE))
		return WRONG;
	yourtm.tm_year = y;
	if (yourtm.tm_year != y)
		return WRONG;
	if (yourtm.tm_sec >= 0 && yourtm.tm_sec < SECSPERMIN)
		saved_seconds = 0;
	else if (y + TM_YEAR_BASE < EPOCH_YEAR) {
		/*
		** We can't set tm_sec to 0, because that might push the
		** time below the minimum representable time.
		** Set tm_sec to 59 instead.
		** This assumes that the minimum representable time is
		** not in the same minute that a leap second was deleted from,
		** which is a safer assumption than using 58 would be.
		*/
		if (increment_overflow(&yourtm.tm_sec, 1 - SECSPERMIN))
			return WRONG;
		saved_seconds = yourtm.tm_sec;
		yourtm.tm_sec = SECSPERMIN - 1;
	} else {
		saved_seconds = yourtm.tm_sec;
		yourtm.tm_sec = 0;
	}
	/*
	** Do a binary search (this works whatever time_t's type is).
	*/
	if (!TYPE_SIGNED(time_t)) {
		lo = 0;
		hi = lo - 1;
	} else {
		lo = 1;
		for (i = 0; i < (int) TYPE_BIT(time_t) - 1; ++i)
			lo *= 2;
		hi = -(lo + 1);
	}
	for ( ; ; ) {
		t = lo / 2 + hi / 2;
		if (t < lo)
			t = lo;
		else if (t > hi)
			t = hi;
		if ((*funcp)(&t, offset, &mytm) == NULL) {
			/*
			** Assume that t is too extreme to be represented in
			** a struct tm; arrange things so that it is less
			** extreme on the next pass.
			*/
			dir = (t > 0) ? 1 : -1;
		} else	dir = tmcomp(&mytm, &yourtm);
		if (dir != 0) {
			if (t == lo) {
				if (t == time_t_max)
					return WRONG;
				++t;
				++lo;
			} else if (t == hi) {
				if (t == time_t_min)
					return WRONG;
				--t;
				--hi;
			}
			if (lo > hi)
				return WRONG;
			if (dir > 0)
				hi = t;
			else	lo = t;
			continue;
		}
		if (yourtm.tm_isdst < 0 || mytm.tm_isdst == yourtm.tm_isdst)
			break;
		/*
		** Right time, wrong type.
		** Hunt for right time, right type.
		** It's okay to guess wrong since the guess
		** gets checked.
		*/
		sp = (const struct state *) gmtptr;
		if (sp == NULL)
			return WRONG;
		for (i = sp->typecnt - 1; i >= 0; --i) {
			if (sp->ttis[i].tt_isdst != yourtm.tm_isdst)
				continue;
			for (j = sp->typecnt - 1; j >= 0; --j) {
				if (sp->ttis[j].tt_isdst == yourtm.tm_isdst)
					continue;
				newt = t + sp->ttis[j].tt_gmtoff -
					sp->ttis[i].tt_gmtoff;
				if ((*funcp)(&newt, offset, &mytm) == NULL)
					continue;
				if (tmcomp(&mytm, &yourtm) != 0)
					continue;
				if (mytm.tm_isdst != yourtm.tm_isdst)
					continue;
				/*
				** We have a match.
				*/
				t = newt;
				goto label;
			}
		}
		return WRONG;
	}
label:
	newt = t + saved_seconds;
	if ((newt < t) != (saved_seconds < 0))
		return WRONG;
	t = newt;
	if ((*funcp)(&t, offset, tmp))
		*okayp = true;
	return t;
}

static time_t
time2(struct tm * const	tmp,
      struct tm * (*const funcp)(const time_t *, int_fast32_t, struct tm *),
      const int_fast32_t offset,
      int *const okayp)
{
	time_t	t;

	/*
	** First try without normalization of seconds
	** (in case tm_sec contains a value associated with a leap second).
	** If that fails, try with normalization of seconds.
	*/
	t = time2sub(tmp, funcp, offset, okayp, false);
	return *okayp ? t : time2sub(tmp, funcp, offset, okayp, true);
}

static time_t
time1(struct tm *const tmp,
      struct tm *(*const funcp) (const time_t *, int_fast32_t, struct tm *),
      const int_fast32_t offset)
{
	register time_t			t;
	register const struct state *	sp;
	register int			samei, otheri;
	register int			sameind, otherind;
	register int			i;
	register int			nseen;
	int				seen[TZ_MAX_TYPES];
	int				types[TZ_MAX_TYPES];
	int				okay;

	if (tmp == NULL) {
		errno = EINVAL;
		return WRONG;
	}
	if (tmp->tm_isdst > 1)
		tmp->tm_isdst = 1;
	t = time2(tmp, funcp, offset, &okay);
	if (okay)
		return t;
	if (tmp->tm_isdst < 0)
#ifdef PCTS
		/*
		** POSIX Conformance Test Suite code courtesy Grant Sullivan.
		*/
		tmp->tm_isdst = 0;	/* reset to std and try again */
#else
		return t;
#endif /* !defined PCTS */
	/*
	** We're supposed to assume that somebody took a time of one type
	** and did some math on it that yielded a "struct tm" that's bad.
	** We try to divine the type they started from and adjust to the
	** type they need.
	*/
	sp = (const struct state *) gmtptr;
	if (sp == NULL)
		return WRONG;
	for (i = 0; i < sp->typecnt; ++i)
		seen[i] = false;
	nseen = 0;
	for (i = sp->timecnt - 1; i >= 0; --i)
		if (!seen[sp->types[i]]) {
			seen[sp->types[i]] = true;
			types[nseen++] = sp->types[i];
		}
	for (sameind = 0; sameind < nseen; ++sameind) {
		samei = types[sameind];
		if (sp->ttis[samei].tt_isdst != tmp->tm_isdst)
			continue;
		for (otherind = 0; otherind < nseen; ++otherind) {
			otheri = types[otherind];
			if (sp->ttis[otheri].tt_isdst == tmp->tm_isdst)
				continue;
			tmp->tm_sec += sp->ttis[otheri].tt_gmtoff -
					sp->ttis[samei].tt_gmtoff;
			tmp->tm_isdst = !tmp->tm_isdst;
			t = time2(tmp, funcp, offset, &okay);
			if (okay)
				return t;
			tmp->tm_sec -= sp->ttis[otheri].tt_gmtoff -
					sp->ttis[samei].tt_gmtoff;
			tmp->tm_isdst = !tmp->tm_isdst;
		}
	}
	return WRONG;
}

time_t
_bson_timegm(struct tm *const tmp)
{
	if (tmp != NULL)
		tmp->tm_isdst = 0;
	return time1(tmp, gmtsub, 0L);
}

#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
