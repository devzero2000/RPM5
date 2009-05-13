/**
 * \file rpmio/rpmkey.c
 */

#include "system.h"
#include <keyutils.h>
#include <rpmio.h>
#include <rpmcli.h>
#include "debug.h"

static int _debug = 0;

static uid_t myuid;
static gid_t mygid, *mygroups;
static int myngroups;

/*****************************************************************************/
/*
 *  * handle an error
 *   */
static inline void rpmkeyError(const char *msg)
{
        perror(msg);
        exit(1);

} /* end rpmkeyError() */

/*****************************************************************************/
/*
 * display command format information
 */
static void rpmkeyFormat(void)
{
#ifdef	NOTYET
	const struct command *cmd;

	fprintf(stderr, "Format:\n");

	for (cmd = commands; cmd->action; cmd++)
		fprintf(stderr, "  keyctl %s %s\n", cmd->name, cmd->format);
#endif

	fprintf(stderr, "\n");
	fprintf(stderr, "Key/keyring ID:\n");
	fprintf(stderr, "  <nnn>   numeric keyring ID\n");
	fprintf(stderr, "  @t      thread keyring\n");
	fprintf(stderr, "  @p      process keyring\n");
	fprintf(stderr, "  @s      session keyring\n");
	fprintf(stderr, "  @u      user keyring\n");
	fprintf(stderr, "  @us     user default session keyring\n");
	fprintf(stderr, "  @g      group keyring\n");
	fprintf(stderr, "  @a      assumed request_key authorisation key\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "<type> can be \"user\" for a user-defined keyring\n");
	fprintf(stderr, "If you do this, prefix the description with \"<subtype>:\"\n");

	exit(2);

} /* end rpmkeyFormat() */

/*****************************************************************************/
/*
 * convert the permissions mask to a string representing the permissions we
 * have actually been granted
 */
static void calc_perms(char *pretty, key_perm_t perm, uid_t uid, gid_t gid)
{
	unsigned perms;
	gid_t *pg;
	int loop;

	perms = (perm & KEY_POS_ALL) >> 24;

	if (uid == myuid) {
		perms |= (perm & KEY_USR_ALL) >> 16;
		goto write_mask;
	}

	if (gid != (gid_t)-1) {
		if (gid == mygid) {
			perms |= (perm & KEY_GRP_ALL) >> 8;
			goto write_mask;
		}

		pg = mygroups;
		for (loop = myngroups; loop > 0; loop--, pg++) {
			if (gid == *pg) {
				perms |= (perm & KEY_GRP_ALL) >> 8;
				goto write_mask;
			}
		}
	}

	perms |= (perm & KEY_OTH_ALL);

write_mask:
	sprintf(pretty, "--%c%c%c%c%c%c",
		perms & KEY_OTH_SETATTR	? 'a' : '-',
		perms & KEY_OTH_LINK	? 'l' : '-',
		perms & KEY_OTH_SEARCH	? 's' : '-',
		perms & KEY_OTH_WRITE	? 'w' : '-',
		perms & KEY_OTH_READ	? 'r' : '-',
		perms & KEY_OTH_VIEW	? 'v' : '-');

} /* end calc_perms() */

/*****************************************************************************/
/*
 * recursively display a key/keyring tree
 */
static int dump_key_tree_aux(key_serial_t key, int depth, int more)
{
	static char dumpindent[64];
	key_serial_t *pk;
	key_perm_t perm;
	size_t ringlen, desclen;
	void *payload;
	char *desc, type[255], pretty_mask[9];
	int uid, gid, ret, n, dpos, rdepth, kcount = 0;

	if (depth > 8)
		return 0;

	/* find out how big this key's description is */
	ret = keyctl_describe(key, NULL, 0);
	if (ret < 0) {
		printf("%d: key inaccessible (%m)\n", key);
		return 0;
	}
	desclen = ret + 1;

	desc = malloc(desclen);
	if (!desc)
		rpmkeyError("malloc");

	/* read the description */
	ret = keyctl_describe(key, desc, desclen);
	if (ret < 0) {
		printf("%d: key inaccessible (%m)\n", key);
		free(desc);
		return 0;
	}

	desclen = (size_t)ret < desclen ? (size_t)ret : desclen;

	desc[desclen] = 0;

	/* parse */
	type[0] = 0;
	uid = 0;
	gid = 0;
	perm = 0;

	n = sscanf(desc, "%[^;];%d;%d;%x;%n",
		   type, &uid, &gid, &perm, &dpos);

	if (n != 4) {
		fprintf(stderr, "Unparseable description obtained for key %d\n", key);
		exit(3);
	}

	/* and print */
	calc_perms(pretty_mask, perm, uid, gid);

	printf("%9d %s  %5d %5d  %s%s%s: %s\n",
	       key,
	       pretty_mask,
	       uid, gid,
	       dumpindent,
	       depth > 0 ? "\\_ " : "",
	       type, desc + dpos);

	/* if it's a keyring then we're going to want to recursively
	 * display it if we can */
	if (strcmp(type, "keyring") == 0) {
		/* find out how big the keyring is */
		ret = keyctl_read(key, NULL, 0);
		if (ret < 0)
			rpmkeyError("keyctl_read");
		if (ret == 0)
			return 0;
		ringlen = ret;

		/* read its contents */
		payload = malloc(ringlen);
		if (!payload)
			rpmkeyError("malloc");

		ret = keyctl_read(key, payload, ringlen);
		if (ret < 0)
			rpmkeyError("keyctl_read");

		ringlen = (size_t)ret < ringlen ? (size_t)ret : ringlen;
		kcount = ringlen / sizeof(key_serial_t);

		/* walk the keyring */
		pk = payload;
		do {
			key = *pk++;

			/* recurse into nexted keyrings */
			if (strcmp(type, "keyring") == 0) {
				if (depth == 0) {
					rdepth = depth;
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth] = 0;
				}
				else {
					rdepth = depth;
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth] = 0;
				}

				if (more)
					dumpindent[depth + 0] = '|';

				kcount += dump_key_tree_aux(key,
							    rdepth,
							    ringlen - 4 >= sizeof(key_serial_t));
			}

		} while (ringlen -= 4, ringlen >= sizeof(key_serial_t));

		free(payload);
	}

	free(desc);
	return kcount;

} /* end dump_key_tree_aux() */

/*****************************************************************************/
/*
 * recursively list a keyring's contents
 */
static int dump_key_tree(key_serial_t keyring, const char *name)
{
	printf("%s\n", name);
	return dump_key_tree_aux(keyring, 0, 0);

} /* end dump_key_tree() */

/*****************************************************************************/
/*
 * parse a key identifier
 */
static key_serial_t get_key_id(const char *arg)
{
	key_serial_t id;
	char *end;

	/* handle a special keyring name */
	if (arg[0] == '@') {
		if (strcmp(arg, "@t" ) == 0) return KEY_SPEC_THREAD_KEYRING;
		if (strcmp(arg, "@p" ) == 0) return KEY_SPEC_PROCESS_KEYRING;
		if (strcmp(arg, "@s" ) == 0) return KEY_SPEC_SESSION_KEYRING;
		if (strcmp(arg, "@u" ) == 0) return KEY_SPEC_USER_KEYRING;
		if (strcmp(arg, "@us") == 0) return KEY_SPEC_USER_SESSION_KEYRING;
		if (strcmp(arg, "@g" ) == 0) return KEY_SPEC_GROUP_KEYRING;
		if (strcmp(arg, "@a" ) == 0) return KEY_SPEC_REQKEY_AUTH_KEY;

		fprintf(stderr, "Unknown special key: '%s'\n", arg);
		exit(2);
	}

	/* handle a numeric key ID */
	id = strtoul(arg, &end, 0);
	if (*end) {
		fprintf(stderr, "Unparsable key: '%s'\n", arg);
		exit(2);
	}

	return id;

} /* end get_key_id() */

/*****************************************************************************/
/*
 * grab data from stdin
 */
static char *grab_stdin(void)
{
	static char input[65536 + 1];
	size_t n;
	int tmp;

	n = 0;
	do {
		tmp = read(0, input + n, sizeof(input) - 1 - n);
		if (tmp < 0)
			rpmkeyError("stdin");

		if (tmp == 0)
			break;

		n += tmp;

	} while (n < sizeof(input));

	if (n >= sizeof(input)) {
		fprintf(stderr, "Too much data read on stdin\n");
		exit(1);
	}

	input[n] = '\0';

	return input;

} /* end grab_stdin() */

/*****************************************************************************/
/*
 * show the parent process's session keyring
 */
static int rpmkeyShow(int argc, char *argv[])
{
    if (argc != 1)
	rpmkeyFormat();

    dump_key_tree(KEY_SPEC_SESSION_KEYRING, "Session Keyring");
    return 0;
}

/*****************************************************************************/
/*
 * add a key
 */
static int rpmkeyAdd(int argc, char *argv[])
{
    key_serial_t dest;
    int ret;

    if (argc != 5)
	rpmkeyFormat();

    dest = get_key_id(argv[4]);

    ret = add_key(argv[1], argv[2], argv[3], strlen(argv[3]), dest);
    if (ret < 0)
	rpmkeyError("add_key");

    /* print the resulting key ID */
    printf("%d\n", ret);
    return 0;

}

/*****************************************************************************/
/*
 * add a key, reading from a pipe
 */
static int rpmkeyPAdd(int argc, char *argv[])
{
	char *args[6];

	if (argc != 4)
		rpmkeyFormat();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = argv[2];
	args[3] = grab_stdin();
	args[4] = argv[3];
	args[5] = NULL;

	return rpmkeyAdd(5, args);

}

/*****************************************************************************/
/*
 * request a key
 */
static int rpmkeyRequest(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 3 && argc != 4)
		rpmkeyFormat();

	dest = 0;
	if (argc == 4)
		dest = get_key_id(argv[3]);

	ret = request_key(argv[1], argv[2], NULL, dest);
	if (ret < 0)
		rpmkeyError("request_key");

	/* print the resulting key ID */
	printf("%d\n", ret);
	return 0;

}

/*****************************************************************************/
/*
 * request a key, with recourse to /sbin/request-key
 */
static int rpmkeyRequest2(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 4 && argc != 5)
		rpmkeyFormat();

	dest = 0;
	if (argc == 5)
		dest = get_key_id(argv[4]);

	ret = request_key(argv[1], argv[2], argv[3], dest);
	if (ret < 0)
		rpmkeyError("request_key");

	/* print the resulting key ID */
	printf("%d\n", ret);
	return 0;

}

/*****************************************************************************/
/*
 * request a key, with recourse to /sbin/request-key, reading the callout info
 * from a pipe
 */
static int rpmkeyPRequest2(int argc, char *argv[])
{
	char *args[6];

	if (argc != 3 && argc != 4)
		rpmkeyFormat();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = argv[2];
	args[3] = grab_stdin();
	args[4] = argv[3];
	args[5] = NULL;

	return rpmkeyRequest2(argc + 1, args);

}

/*****************************************************************************/
/*
 * update a key
 */
static int rpmkeyUpdate(int argc, char *argv[])
{
	key_serial_t key;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	if (keyctl_update(key, argv[2], strlen(argv[2])) < 0)
		rpmkeyError("keyctl_update");

	return 0;

}

/*****************************************************************************/
/*
 * update a key, reading from a pipe
 */
static int rpmkeyPUpdate(int argc, char *argv[])
{
	char *args[4];

	if (argc != 2)
		rpmkeyFormat();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = grab_stdin();
	args[3] = NULL;

	return rpmkeyUpdate(3, args);

}

/*****************************************************************************/
/*
 * create a new keyring
 */
static int rpmkeyNewring(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 3)
		rpmkeyFormat();

	dest = get_key_id(argv[2]);

	ret = add_key("keyring", argv[1], NULL, 0, dest);
	if (ret < 0)
		rpmkeyError("add_key");

	printf("%d\n", ret);
	return 0;

}

/*****************************************************************************/
/*
 * revoke a key
 */
static int rpmkeyRevoke(int argc, char *argv[])
{
	key_serial_t key;

	if (argc != 2)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	if (keyctl_revoke(key) < 0)
		rpmkeyError("keyctl_revoke");

	return 0;

}

/*****************************************************************************/
/*
 * clear a keyring
 */
static int rpmkeyClear(int argc, char *argv[])
{
	key_serial_t keyring;

	if (argc != 2)
		rpmkeyFormat();

	keyring = get_key_id(argv[1]);

	if (keyctl_clear(keyring) < 0)
		rpmkeyError("keyctl_clear");

	return 0;

}

/*****************************************************************************/
/*
 * link a key to a keyring
 */
static int rpmkeyLink(int argc, char *argv[])
{
	key_serial_t keyring, key;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);
	keyring = get_key_id(argv[2]);

	if (keyctl_link(key, keyring) < 0)
		rpmkeyError("keyctl_link");

	return 0;

}

/*****************************************************************************/
/*
 * unlink a key from a keyrign
 */
static int rpmkeyUnlink(int argc, char *argv[])
{
	key_serial_t keyring, key;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);
	keyring = get_key_id(argv[2]);

	if (keyctl_unlink(key, keyring) < 0)
		rpmkeyError("keyctl_unlink");

	return 0;

}

/*****************************************************************************/
/*
 * search a keyring for a key
 */
static int rpmkeySearch(int argc, char *argv[])
{
	key_serial_t keyring, dest;
	int ret;

	if (argc != 4 && argc != 5)
		rpmkeyFormat();

	keyring = get_key_id(argv[1]);

	dest = 0;
	if (argc == 5)
		dest = get_key_id(argv[4]);

	ret = keyctl_search(keyring, argv[2], argv[3], dest);
	if (ret < 0)
		rpmkeyError("keyctl_search");

	/* print the ID of the key we found */
	printf("%d\n", ret);
	return 0;

}

/*****************************************************************************/
/*
 * read a key
 */
static int rpmkeyRead(int argc, char *argv[])
{
	key_serial_t key;
	void *buffer;
	char *p;
	int ret, sep, col;

	if (argc != 2)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	/* read the key payload data */
	ret = keyctl_read_alloc(key, &buffer);
	if (ret < 0)
		rpmkeyError("keyctl_read_alloc");

	if (ret == 0) {
		printf("No data in key\n");
		return 0;
	}

	/* hexdump the contents */
	printf("%u bytes of data in key:\n", ret);

	sep = 0;
	col = 0;
	p = buffer;

	do {
		if (sep) {
			putchar(sep);
			sep = 0;
		}

		printf("%02hhx", *p);
		p++;

		col++;
		if (col % 32 == 0)
			sep = '\n';
		else if (col % 4 == 0)
			sep = ' ';

	} while (--ret > 0);

	printf("\n");
	return 0;

}

/*****************************************************************************/
/*
 * read a key and dump raw to stdout
 */
static int rpmkeyPipe(int argc, char *argv[])
{
	key_serial_t key;
	void *buffer;
	int ret;

	if (argc != 2)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	/* read the key payload data */
	ret = keyctl_read_alloc(key, &buffer);
	if (ret < 0)
		rpmkeyError("keyctl_read_alloc");

	if (ret > 0 && write(1, buffer, ret) < 0)
		rpmkeyError("write");
	return 0;

}

/*****************************************************************************/
/*
 * read a key and dump to stdout in printable form
 */
static int rpmkeyPrint(int argc, char *argv[])
{
	key_serial_t key;
	void *buffer;
	char *p;
	int loop, ret;

	if (argc != 2)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	/* read the key payload data */
	ret = keyctl_read_alloc(key, &buffer);
	if (ret < 0)
		rpmkeyError("keyctl_read_alloc");

	/* see if it's printable */
	p = buffer;
	for (loop = ret; loop > 0; loop--, p++)
		if (!isprint(*p))
			goto not_printable;

	/* it is */
	printf("%s\n", (char *) buffer);
	return 0;

not_printable:
	/* it isn't */
	printf(":hex:");
	p = buffer;
	for (loop = ret; loop > 0; loop--, p++)
		printf("%02hhx", *p);
	printf("\n");
	return 0;

}

/*****************************************************************************/
/*
 * list a keyring
 */
static int rpmkeyList(int argc, char *argv[])
{
	key_serial_t keyring, key, *pk;
	key_perm_t perm;
	void *keylist;
	char *buffer, pretty_mask[9];
	uid_t uid;
	gid_t gid;
	int count, tlen, dpos, n, ret;

	if (argc != 2)
		rpmkeyFormat();

	keyring = get_key_id(argv[1]);

	/* read the key payload data */
	count = keyctl_read_alloc(keyring, &keylist);
	if (count < 0)
		rpmkeyError("keyctl_read_alloc");

	count /= sizeof(key_serial_t);

	if (count == 0) {
		printf("keyring is empty\n");
		return 0;
	}

	/* list the keys in the keyring */
	if (count == 1)
		printf("1 key in keyring:\n");
	else
		printf("%u keys in keyring:\n", count);

	pk = keylist;
	do {
		key = *pk++;

		ret = keyctl_describe_alloc(key, &buffer);
		if (ret < 0) {
			printf("%9d: key inaccessible (%m)\n", key);
			continue;
		}

		uid = 0;
		gid = 0;
		perm = 0;

		tlen = -1;
		dpos = -1;

		n = sscanf((char *) buffer, "%*[^;]%n;%d;%d;%x;%n",
			   &tlen, &uid, &gid, &perm, &dpos);
		if (n != 3) {
			fprintf(stderr, "Unparseable description obtained for key %d\n", key);
			exit(3);
		}

		calc_perms(pretty_mask, perm, uid, gid);

		printf("%9d: %s %5d %5d %*.*s: %s\n",
		       key,
		       pretty_mask,
		       uid, gid,
		       tlen, tlen, buffer,
		       buffer + dpos);

		free(buffer);

	} while (--count);

	return 0;

}

/*****************************************************************************/
/*
 * produce a raw list of a keyring
 */
static int rpmkeyRList(int argc, char *argv[])
{
	key_serial_t keyring, key, *pk;
	void *keylist;
	int count;

	if (argc != 2)
		rpmkeyFormat();

	keyring = get_key_id(argv[1]);

	/* read the key payload data */
	count = keyctl_read_alloc(keyring, &keylist);
	if (count < 0)
		rpmkeyError("keyctl_read_alloc");

	count /= sizeof(key_serial_t);

	/* list the keys in the keyring */
	if (count <= 0) {
		printf("\n");
	}
	else {
		pk = keylist;
		for (; count > 0; count--) {
			key = *pk++;
			printf("%d%c", key, count == 1 ? '\n' : ' ');
		}
	}

	return 0;

}

/*****************************************************************************/
/*
 * describe a key
 */
static int rpmkeyDescribe(int argc, char *argv[])
{
	key_serial_t key;
	key_perm_t perm;
	char *buffer;
	uid_t uid;
	gid_t gid;
	int tlen, dpos, n, ret;

	if (argc != 2)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	/* get key description */
	ret = keyctl_describe_alloc(key, &buffer);
	if (ret < 0)
		rpmkeyError("keyctl_describe");

	/* parse it */
	uid = 0;
	gid = 0;
	perm = 0;

	tlen = -1;
	dpos = -1;

	n = sscanf(buffer, "%*[^;]%n;%d;%d;%x;%n",
		   &tlen, &uid, &gid, &perm, &dpos);
	if (n != 3) {
		fprintf(stderr, "Unparseable description obtained for key %d\n", key);
		exit(3);
	}

	/* display it */
	printf("%9d:"
	       " %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
	       " %5d %5d %*.*s: %s\n",
	       key,
	       perm & KEY_POS_SETATTR	? 'a' : '-',
	       perm & KEY_POS_LINK	? 'l' : '-',
	       perm & KEY_POS_SEARCH	? 's' : '-',
	       perm & KEY_POS_WRITE	? 'w' : '-',
	       perm & KEY_POS_READ	? 'r' : '-',
	       perm & KEY_POS_VIEW	? 'v' : '-',

	       perm & KEY_USR_SETATTR	? 'a' : '-',
	       perm & KEY_USR_LINK	? 'l' : '-',
	       perm & KEY_USR_SEARCH	? 's' : '-',
	       perm & KEY_USR_WRITE	? 'w' : '-',
	       perm & KEY_USR_READ	? 'r' : '-',
	       perm & KEY_USR_VIEW	? 'v' : '-',

	       perm & KEY_GRP_SETATTR	? 'a' : '-',
	       perm & KEY_GRP_LINK	? 'l' : '-',
	       perm & KEY_GRP_SEARCH	? 's' : '-',
	       perm & KEY_GRP_WRITE	? 'w' : '-',
	       perm & KEY_GRP_READ	? 'r' : '-',
	       perm & KEY_GRP_VIEW	? 'v' : '-',

	       perm & KEY_OTH_SETATTR	? 'a' : '-',
	       perm & KEY_OTH_LINK	? 'l' : '-',
	       perm & KEY_OTH_SEARCH	? 's' : '-',
	       perm & KEY_OTH_WRITE	? 'w' : '-',
	       perm & KEY_OTH_READ	? 'r' : '-',
	       perm & KEY_OTH_VIEW	? 'v' : '-',
	       uid, gid,
	       tlen, tlen, buffer,
	       buffer + dpos);

	return 0;

}

/*****************************************************************************/
/*
 * get raw key description
 */
static int rpmkeyRDescribe(int argc, char *argv[])
{
	key_serial_t key;
	char *buffer, *q;
	int ret;

	if (argc != 2 && argc != 3)
		rpmkeyFormat();
	if (argc == 3 && !argv[2][0])
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	/* get key description */
	ret = keyctl_describe_alloc(key, &buffer);
	if (ret < 0)
		rpmkeyError("keyctl_describe");

	/* replace semicolon separators with requested alternative */
	if (argc == 3) {
		for (q = buffer; *q; q++)
			if (*q == ';')
				*q = argv[2][0];
	}

	/* display raw description */
	printf("%s\n", buffer);
	return 0;

}

/*****************************************************************************/
/*
 * change a key's ownership
 */
static int rpmkeyChown(int argc, char *argv[])
{
	key_serial_t key;
	uid_t uid;
	char *q;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	uid = strtoul(argv[2], &q, 0);
	if (*q) {
		fprintf(stderr, "Unparsable uid: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_chown(key, uid, -1) < 0)
		rpmkeyError("keyctl_chown");

	return 0;

}

/*****************************************************************************/
/*
 * change a key's group ownership
 */
static int rpmkeyChgrp(int argc, char *argv[])
{
	key_serial_t key;
	gid_t gid;
	char *q;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	gid = strtoul(argv[2], &q, 0);
	if (*q) {
		fprintf(stderr, "Unparsable gid: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_chown(key, -1, gid) < 0)
		rpmkeyError("keyctl_chown");

	return 0;

}

/*****************************************************************************/
/*
 * set the permissions on a key
 */
static int rpmkeySetperm(int argc, char *argv[])
{
	key_serial_t key;
	key_perm_t perm;
	char *q;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);
	perm = strtoul(argv[2], &q, 0);
	if (*q) {
		fprintf(stderr, "Unparsable permissions: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_setperm(key, perm) < 0)
		rpmkeyError("keyctl_setperm");

	return 0;

}

/*****************************************************************************/
/*
 * start a process in a new session
 */
static int rpmkeySession(int argc, char *argv[])
{
	char *p, *q;
	int ret;

	argv++;
	argc--;

	/* no extra arguments signifies a standard shell in an anonymous
	 * session */
	p = NULL;
	if (argc != 0) {
		/* a dash signifies an anonymous session */
		p = *argv;
		if (strcmp(p, "-") == 0)
			p = NULL;

		argv++;
		argc--;
	}

	/* create a new session keyring */
	ret = keyctl_join_session_keyring(p);
	if (ret < 0)
		rpmkeyError("keyctl_join_session_keyring");

	fprintf(stderr, "Joined session keyring: %d\n", ret);

	/* run the standard shell if no arguments */
	if (argc == 0) {
		q = getenv("SHELL");
		if (!q)
			q = "/bin/sh";
		execl(q, q, NULL);
		rpmkeyError(q);
	}

	/* run the command specified */
	execvp(argv[0], argv);
	rpmkeyError(argv[0]);

	/*@notreached@*/
	return -1;
}

/*****************************************************************************/
/*
 * instantiate a key that's under construction
 */
static int rpmkeyInstantiate(int argc, char *argv[])
{
	key_serial_t key, dest;

	if (argc != 4)
		rpmkeyFormat();

	key = get_key_id(argv[1]);
	dest = get_key_id(argv[3]);

	if (keyctl_instantiate(key, argv[2], strlen(argv[2]), dest) < 0)
		rpmkeyError("keyctl_instantiate");

	return 0;

}

/*****************************************************************************/
/*
 * instantiate a key, reading from a pipe
 */
static int rpmkeyPInstantiate(int argc, char *argv[])
{
	char *args[5];

	if (argc != 3)
		rpmkeyFormat();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = grab_stdin();
	args[3] = argv[2];
	args[4] = NULL;

	return rpmkeyInstantiate(4, args);

}

/*****************************************************************************/
/*
 * negate a key that's under construction
 */
static int rpmkeyNegate(int argc, char *argv[])
{
	unsigned long timeout;
	key_serial_t key, dest;
	char *q;

	if (argc != 4)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	timeout = strtoul(argv[2], &q, 10);
	if (*q) {
		fprintf(stderr, "Unparsable timeout: '%s'\n", argv[2]);
		exit(2);
	}

	dest = get_key_id(argv[3]);

	if (keyctl_negate(key, timeout, dest) < 0)
		rpmkeyError("keyctl_negate");

	return 0;

}

/*****************************************************************************/
/*
 * set a key's timeout
 */
static int rpmkeyTimeout(int argc, char *argv[])
{
	unsigned long timeout;
	key_serial_t key;
	char *q;

	if (argc != 3)
		rpmkeyFormat();

	key = get_key_id(argv[1]);

	timeout = strtoul(argv[2], &q, 10);
	if (*q) {
		fprintf(stderr, "Unparsable timeout: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_set_timeout(key, timeout) < 0)
		rpmkeyError("keyctl_set_timeout");

	return 0;

}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,		&_debug, 1,
	NULL, NULL },

  { "show", '\0', POPT_ARG_MAINCALL,	rpmkeyShow, 0,
	N_("Show process keyrings"), N_("") },
  { "add", '\0', POPT_ARG_MAINCALL,	rpmkeyAdd, 0,
	N_("Add a key to a keyring"), N_("<type> <desc> <data> <keyring>") },
  { "padd", '\0', POPT_ARG_MAINCALL,	rpmkeyPAdd, 0,
	N_("Add a key to a keyring"), N_("<type> <desc> <keyring>") },
  { "request", '\0', POPT_ARG_MAINCALL,	rpmkeyRequest, 0,
	N_("Request a key"), N_("<type> <desc> [<dest_keyring>]") },
  { "request2", '\0', POPT_ARG_MAINCALL,rpmkeyRequest2, 0,
	N_("Request a key"), N_("<type> <desc> <info> [<dest_keyring>]") },
  { "prequest2", '\0', POPT_ARG_MAINCALL,rpmkeyPRequest2, 0,
	N_("Request a key"), N_("<type> <desc> [<dest_keyring>]") },
  { "update", '\0', POPT_ARG_MAINCALL,	rpmkeyUpdate, 0,
	N_("Update a key"), N_("<key> <data>") },
  { "pupdate", '\0', POPT_ARG_MAINCALL,	rpmkeyPUpdate, 0,
	N_("Update a key"), N_("<key>") },
  { "newring", '\0', POPT_ARG_MAINCALL,	rpmkeyNewring, 0,
	N_("Create a keyring"), N_("<name> <keyring>") },
  { "revoke", '\0', POPT_ARG_MAINCALL,	rpmkeyRevoke, 0,
	N_("Revoke a key"), N_("<key>") },
  { "clear", '\0', POPT_ARG_MAINCALL,	rpmkeyClear, 0,
	N_("Clear a keyring"), N_("<keyring>") },
  { "link", '\0', POPT_ARG_MAINCALL,	rpmkeyLink, 0,
	N_("Link a key to a keyring"), N_("<key> <keyring>") },
  { "unlink", '\0', POPT_ARG_MAINCALL,	rpmkeyUnlink, 0,
	N_("Unlink a key from a keyring"), N_("<key> <keyring>") },
  { "search", '\0', POPT_ARG_MAINCALL,	rpmkeySearch, 0,
	N_("Search a keyring"), N_("<keyring> <type> <desc> [<dest_keyring>]") },
  { "read", '\0', POPT_ARG_MAINCALL,	rpmkeyRead, 0,
	N_("Read a key"), N_("<key>") },
  { "pipe", '\0', POPT_ARG_MAINCALL,	rpmkeyPipe, 0,
	N_("Read a key"), N_("<key>") },
  { "print", '\0', POPT_ARG_MAINCALL,	rpmkeyPrint, 0,
	N_("Read a key"), N_("<key>") },
  { "list", '\0', POPT_ARG_MAINCALL,	rpmkeyList, 0,
	N_("List a keyring"), N_("<keyring>") },
  { "rlist", '\0', POPT_ARG_MAINCALL,	rpmkeyRList, 0,
	N_("List a keyring"), N_("<keyring>") },
  { "describe", '\0', POPT_ARG_MAINCALL,rpmkeyDescribe, 0,
	N_("Describe a key"), N_("<keyring>") },
  { "rdescribe", '\0', POPT_ARG_MAINCALL,rpmkeyRDescribe, 0,
	N_("Describe a key"), N_("<keyring> [sep]") },
  { "chown", '\0', POPT_ARG_MAINCALL,	rpmkeyChown, 0,
	N_("Change the access controls on a key"), N_("<key> <uid>") },
  { "chgrp", '\0', POPT_ARG_MAINCALL,	rpmkeyChgrp, 0,
	N_("Change the access controls on a key"), N_("<key> <gid>") },
  { "setperm", '\0', POPT_ARG_MAINCALL,	rpmkeySetperm, 0,
	N_("Set the permissions mask on a key"), N_("<key> <mask>") },
  { "session", '\0', POPT_ARG_MAINCALL,	rpmkeySession, 0,
	N_("Start a new session with fresh keyrings"), N_("[{-|<name>} [<prog> <arg1> <arg2> ...]]") },
  { "instantiate", '\0', POPT_ARG_MAINCALL,rpmkeyInstantiate, 0,
	N_("Instantiate a key"), N_("<key> <data> <keyring>") },
  { "pinstantiate", '\0', POPT_ARG_MAINCALL,rpmkeyPInstantiate, 0,
	N_("Instantiate a key"), N_("<key> <keyring>") },
  { "negate", '\0', POPT_ARG_MAINCALL,	rpmkeyNegate, 0,
	N_("Instantiate a key"), N_("<key> <timeout> <keyring>") },
  { "timeout", '\0', POPT_ARG_MAINCALL,	rpmkeyTimeout, 0,
	N_("Set the expiry time on a key"), N_("<key> <timeout>") },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options:"),
	NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    int ret = 0;

    if (optCon == NULL)
        exit(EXIT_FAILURE);

    /* grab my UID, GID and groups */
    myuid = geteuid();
    mygid = getegid();
    myngroups = getgroups(0, NULL);

    if (myuid == (uid_t)-1 || mygid == (uid_t)-1 || myngroups == -1)
	rpmkeyError("Unable to get UID/GID/#Groups\n");

    mygroups = calloc(myngroups, sizeof(gid_t));
    if (!mygroups)
	rpmkeyError("calloc");

    myngroups = getgroups(myngroups, mygroups);
    if (myngroups < 0)
	rpmkeyError("Unable to get Groups\n");

    optCon = rpmcliFini(optCon);
    return ret;
}
