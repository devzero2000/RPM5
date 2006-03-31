/********************************************************************\
 *
 *      FILE:     hashtest.c
 *
 *      CONTENTS: test file for sample C-implementation of
 *                RIPEMD-160 and RIPEMD128
 *        * command line arguments:                                         
 *           filename  -- compute hash code of file read binary     
 *           -sstring  -- print string & hashcode                   
 *           -t        -- perform time trial                        
 *           -x        -- execute standard test suite, ASCII input
 *        * for linkage with rmd128.c: define RMDsize as 128
 *          for linkage with rmd160.c: define RMDsize as 160 (default)
 *      TARGET:   any computer with an ANSI C compiler
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     18 April 1996
 *      VERSION:  1.1
 *      HISTORY:  bug in RMDonemillion() corrected
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
 *  Conditions for use of the RIPEMD-160 Software
 *
 *  The RIPEMD-160 software is freely available for use under the terms and
 *  conditions described hereunder, which shall be deemed to be accepted by
 *  any user of the software and applicable on any use of the software:
 * 
 *  1. K.U.Leuven Department of Electrical Engineering-ESAT/COSIC shall for
 *     all purposes be considered the owner of the RIPEMD-160 software and of
 *     all copyright, trade secret, patent or other intellectual property
 *     rights therein.
 *  2. The RIPEMD-160 software is provided on an "as is" basis without
 *     warranty of any sort, express or implied. K.U.Leuven makes no
 *     representation that the use of the software will not infringe any
 *     patent or proprietary right of third parties. User will indemnify
 *     K.U.Leuven and hold K.U.Leuven harmless from any claims or liabilities
 *     which may arise as a result of its use of the software. In no
 *     circumstances K.U.Leuven R&D will be held liable for any deficiency,
 *     fault or other mishappening with regard to the use or performance of
 *     the software.
 *  3. User agrees to give due credit to K.U.Leuven in scientific publications 
 *     or communications in relation with the use of the RIPEMD-160 software 
 *     as follows: RIPEMD-160 software written by Antoon Bosselaers, 
 *     available at http://www.esat.kuleuven.be/~cosicart/ps/AB-9601/.
 *
\********************************************************************/
#ifndef RMDsize
#define RMDsize 128
#endif

#include "system.h"

#if RMDsize == 128
#include "rmd128.h"
#define	rmdParam	rmd128Param
#define rmdReset	rmd128Reset
#define rmdUpdate	rmd128Update
#define rmdDigest	rmd128Digest
#elif RMDsize == 160
#include "rmd160.h"
#define	rmdParam	rmd160Param
#define rmdReset	rmd160Reset
#define rmdUpdate	rmd160Update
#define rmdDigest	rmd160Digest
#endif

#include "debug.h"

#define TEST_BLOCK_SIZE 8000
#define TEST_BLOCKS 1250
#define TEST_BYTES ((long)TEST_BLOCK_SIZE * (long)TEST_BLOCKS)

/********************************************************************/

static byte *RMD(byte *message)
/*
 * returns RMD(message)
 * message should be a string terminated by '\0'
 */
{
   static byte digest[RMDsize/8];
   rmdParam param;
   size_t length = strlen((char *)message);

   rmdReset(&param);
   rmdUpdate(&param, message, length);
   rmdDigest(&param, digest);

   return (byte *)digest;
}

/********************************************************************/

static byte *RMDbinary(char *fname)
/*
 * returns RMD(message in file fname)
 * fname is read as binary data.
 */
{
   static byte digest[RMDsize/8];
   rmdParam param;
   FILE *fp;
   byte data[BUFSIZ];
   size_t nbytes;

   /* initialize */
   if ((fp = fopen(fname, "rb")) == NULL) {
      fprintf(stderr, "\nRMDbinary: cannot open file \"%s\".\n",
              fname);
      exit(1);
   }

   rmdReset(&param);
   while ((nbytes = fread(data, 1, 1024, fp)) != 0)
	rmdUpdate(&param, data, nbytes);
   rmdDigest(&param, digest);

   fclose(fp);

   return (byte *)digest;
}

/********************************************************************/

static void speedtest(void)
/*
 * A time trial routine, to measure the speed of ripemd.
 * Measures processor time required to process TEST_BLOCKS times
 *  a message of TEST_BLOCK_SIZE characters.
 */
{
   static byte digest[RMDsize/8];
   rmdParam param;
   clock_t      t0, t1;
   byte        *data;
   unsigned int i;

   srand(time(NULL));

   /* allocate and initialize test data */
   if ((data = (byte*)malloc(TEST_BLOCK_SIZE)) == NULL) {
      fprintf(stderr, "speedtest: allocation error\n");
      exit(1);
   }
   for (i=0; i<TEST_BLOCK_SIZE; i++)
      data[i] = (byte)(rand() >> 7);

   /* start timer */
   printf("\n\nRIPEMD-%u time trial. Processing %ld characters...\n",
          RMDsize, TEST_BYTES);

   t0 = clock();

   /* process data */
   rmdReset(&param);
   for (i=0; i<TEST_BLOCKS; i++)
	rmdUpdate(&param, data, TEST_BLOCK_SIZE);
   rmdDigest(&param, digest);

   /* stop timer, get time difference */
   t1 = clock();
   printf("\nTest input processed in %g seconds.\n",
          (double)(t1-t0)/(double)CLOCKS_PER_SEC);
   printf("Characters processed per second: %g\n",
          (double)CLOCKS_PER_SEC*TEST_BYTES/((double)t1-t0));

   printf("\nhashcode: ");
   for (i=0; i<RMDsize/8; i++)
      printf("%02x", digest[i]);
   printf("\n");

   free(data);
   return;
}

/********************************************************************/

static void RMDonemillion(void)
/*
 * returns RMD() of message consisting of 1 million 'a' characters
 */
{
   static byte digest[RMDsize/8];
   rmdParam param;
   byte data[64];
   unsigned int  i;                   /* counter                    */

   memset(data, 'a', sizeof(data));
   memcpy(data, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 32);
   memcpy(data+32, data, 32);

   rmdReset(&param);
   for (i=15625; i>0; i--)
      rmdUpdate(&param, data, sizeof(data));
   rmdDigest(&param, digest);

   printf("\n* message: 1 million times \"a\"\n  hashcode: ");
   for (i=0; i<RMDsize/8; i++)
      printf("%02x", digest[i]);
}

/********************************************************************/

static void RMDstring(char *message, char *print)
{
   unsigned int  i;
   byte         *digest;

   digest = RMD((byte *)message);
   printf("\n* message: %s\n  hashcode: ", print);
   for (i=0; i<RMDsize/8; i++)
      printf("%02x", digest[i]);
}

/********************************************************************/

static void testsuite (void)
/*
 *   standard test suite
 */
{
   printf("\n\nRIPEMD-%u test suite results (ASCII):\n", RMDsize);

   RMDstring("", "\"\" (empty string)");
   RMDstring("a", "\"a\"");
   RMDstring("abc", "\"abc\"");
   RMDstring("message digest", "\"message digest\"");
   RMDstring("abcdefghijklmnopqrstuvwxyz", "\"abcdefghijklmnopqrstuvwxyz\"");
   RMDstring("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
             "\"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq\"");
   RMDstring("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
             "\"A...Za...z0...9\"");
   RMDstring("1234567890123456789012345678901234567890"
             "1234567890123456789012345678901234567890", 
             "8 times \"1234567890\"");
   RMDonemillion();
   printf("\n");

   return;
}

/********************************************************************/

main (int argc, char *argv[])
/*
 *  main program. calls one or more of the test routines depending
 *  on command line arguments. see the header of this file.
 *
 */
{
  unsigned int   i, j;
  byte          *digest;

   if (argc == 1) {
      printf("For each command line argument in turn:\n");
      printf("  filename  -- compute hash code of file binary read\n");
      printf("  -sstring  -- print string & hashcode\n");
      printf("  -t        -- perform time trial\n");
      printf("  -x        -- execute standard test suite, ASCII input\n");
   }
   else {
      for (i = 1; i < argc; i++) {
         if (argv[i][0] == '-' && argv[i][1] == 's') {
            printf("\n\nmessage: %s", argv[i]+2);
            digest = RMD((byte *)argv[i] + 2);
            printf("\nhashcode: ");
            for (j=0; j<RMDsize/8; j++)
               printf("%02x", digest[j]);
            printf("\n");
         }
         else if (strcmp (argv[i], "-t") == 0)
            speedtest ();
         else if (strcmp (argv[i], "-x") == 0)
            testsuite ();
         else {
            digest = RMDbinary (argv[i]);
            printf("\n\nmessagefile (binary): %s", argv[i]);
            printf("\nhashcode: ");
            for (j=0; j<RMDsize/8; j++)
               printf("%02x", digest[j]);
            printf("\n");
         }
      }
   }
   printf("\n");

   return 0;
}

/********************** end of file hashtest.c **********************/

