/**
 * \file rpmio/rpmkey.c
 */

#include "system.h"

#include <poptIO.h>

#include <keyutils.h>

#include "debug.h"

static int _debug = 0;

static uid_t myuid;
static gid_t mygid, *mygroups;
static int myngroups;

#ifdef	NOTYET
static int verbose;
#endif

/*
 * handle an error
 */
__attribute__ ((noreturn))
static inline void rpmkeyError(const char *msg)
{
    perror(msg);
    exit(1);
}

/*
 * display command format information
 */
__attribute__ ((noreturn))
static void rpmkeyFormat(void)
{
#ifdef	NOTYET
    const struct poptOption *cmd;

    fprintf(stderr, "Format:\n");

    for (cmd = _rpmkeyCommandTable; cmd->longName; cmd++)
	fprintf(stderr, "  keyctl %s %s\n", cmd->longName, cmd->argDescrip);
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
    fprintf(stderr,
	    "If you do this, prefix the description with \"<subtype>:\"\n");

    exit(2);
}

/*
 * Load the groups list and grab the process's UID and GID.
 */
static inline void grab_creds(void)
{
    static int inited;

    if (inited)
	return;
    inited = 1;

    /* grab my UID, GID and groups */
    myuid = geteuid();
    mygid = getegid();
    myngroups = getgroups(0, NULL);

    if (myuid == (uid_t)-1 || mygid == (gid_t)-1 || myngroups == -1)
	rpmkeyError("Unable to get UID/GID/#Groups\n");

    mygroups = calloc(myngroups, sizeof(gid_t));
    if (!mygroups)
	rpmkeyError("calloc");

    myngroups = getgroups(myngroups, mygroups);
    if (myngroups < 0)
	rpmkeyError("Unable to get Groups\n");
}

/*
 * convert the permissions mask to a string representing the permissions we
 * have actually been granted
 */
static void calc_perms(char *pretty, key_perm_t perm, uid_t uid, gid_t gid)
{
    unsigned perms;
    gid_t *pg;
    int loop;

    grab_creds();

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
	    (perms & KEY_OTH_SETATTR) ? 'a' : '-',
	    (perms & KEY_OTH_LINK) ? 'l' : '-',
	    (perms & KEY_OTH_SEARCH) ? 's' : '-',
	    (perms & KEY_OTH_WRITE) ? 'w' : '-',
	    (perms & KEY_OTH_READ) ? 'r' : '-',
	    (perms & KEY_OTH_VIEW) ? 'v' : '-');
}

/*
 * recursively display a key/keyring tree
 */
static int dump_key_tree_aux(key_serial_t key, int depth, int more,
			     int hex_key_IDs)
{
    static char dumpindent[64];
    key_serial_t *pk;
    key_perm_t perm;
    size_t ringlen;
    void *payload;
    char *desc, type[255], pretty_mask[9];
    int uid, gid, ret, n, dpos, rdepth, kcount = 0;

    if (depth > 8 * 4)
	return 0;

    /* read the description */
    ret = keyctl_describe_alloc(key, &desc);
    if (ret < 0) {
	printf("%d: key inaccessible (%m)\n", key);
	return 0;
    }

    /* parse */
    type[0] = 0;
    uid = 0;
    gid = 0;
    perm = 0;

    n = sscanf(desc, "%[^;];%d;%d;%x;%n", type, &uid, &gid, &perm, &dpos);

    if (n != 4) {
	fprintf(stderr, "Unparseable description obtained for key %d\n",
		key);
	exit(3);
    }

    /* and print */
    calc_perms(pretty_mask, perm, uid, gid);

    if (hex_key_IDs)
	printf("0x%08x %s  %5d %5d  %s%s%s: %s\n",
	       key,
	       pretty_mask,
	       uid, gid,
	       dumpindent, depth > 0 ? "\\_ " : "", type, desc + dpos);
    else
	printf("%10d %s  %5d %5d  %s%s%s: %s\n",
	       key,
	       pretty_mask,
	       uid, gid,
	       dumpindent, depth > 0 ? "\\_ " : "", type, desc + dpos);

    free(desc);

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
	payload = xmalloc(ringlen);
	if (!payload)
	    rpmkeyError("malloc");

	ret = keyctl_read(key, payload, ringlen);
	if (ret < 0)
	    rpmkeyError("keyctl_read");

	ringlen = (size_t) ret < ringlen ? (size_t) ret : ringlen;
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
		} else {
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
					    ringlen - 4 >=
					    sizeof(key_serial_t),
					    hex_key_IDs);
	    }

	} while (ringlen -= 4, ringlen >= sizeof(key_serial_t));

	free(payload);
    }

    return kcount;
}

/*
 * recursively list a keyring's contents
 */
static int dump_key_tree(key_serial_t keyring, const char *name,
			 int hex_key_IDs)
{
    printf("%s\n", name);

    keyring = keyctl_get_keyring_ID(keyring, 0);
    if (keyring == -1)
	rpmkeyError("Unable to dump key");

    return dump_key_tree_aux(keyring, 0, 0, hex_key_IDs);
}

/*
 * parse a key identifier
 */
static key_serial_t get_key_id(char *arg)
{
    key_serial_t id;
    char *end;

    /* handle a special keyring name */
    if (arg[0] == '@') {
	if (strcmp(arg, "@t") == 0)
	    return KEY_SPEC_THREAD_KEYRING;
	if (strcmp(arg, "@p") == 0)
	    return KEY_SPEC_PROCESS_KEYRING;
	if (strcmp(arg, "@s") == 0)
	    return KEY_SPEC_SESSION_KEYRING;
	if (strcmp(arg, "@u") == 0)
	    return KEY_SPEC_USER_KEYRING;
	if (strcmp(arg, "@us") == 0)
	    return KEY_SPEC_USER_SESSION_KEYRING;
	if (strcmp(arg, "@g") == 0)
	    return KEY_SPEC_GROUP_KEYRING;
	if (strcmp(arg, "@a") == 0)
	    return KEY_SPEC_REQKEY_AUTH_KEY;

	fprintf(stderr, "Unknown special key: '%s'\n", arg);
	exit(2);
    }

    /* handle a lookup-by-name request "%<type>:<desc>", eg: "%keyring:_ses" */
    if (arg[0] == '%') {
	char *type;

	arg++;
	if (!*arg)
	    goto incorrect_key_by_name_spec;

	if (*arg == ':') {
	    type = "keyring";
	    arg++;
	} else {
	    type = arg;
	    arg = strchr(arg, ':');
	    if (!arg)
		goto incorrect_key_by_name_spec;
	    *(arg++) = '\0';
	}

	if (!*arg)
	    goto incorrect_key_by_name_spec;

	id = find_key_by_type_and_desc(type, arg, 0);
	if (id == -1) {
	    fprintf(stderr, "Can't find '%s:%s'\n", type, arg);
	    exit(1);
	}
	return id;
    }

    /* handle a numeric key ID */
    id = strtoul(arg, &end, 0);
    if (*end) {
	fprintf(stderr, "Unparsable key: '%s'\n", arg);
	exit(2);
    }

    return id;

  incorrect_key_by_name_spec:
    fprintf(stderr, "Incorrect key-by-name spec\n");
    exit(2);
}

/*
 * grab data from stdin
 */
static char *grab_stdin(size_t * _size)
{
    static char input[1024 * 1024 + 1];
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
    *_size = n;

    return input;
}

/*
 * Display version information
 */
static int rpmkeyVersion(int argc, char *argv[])
{
    printf("keyctl from %s (Built %s)\n",
	   keyutils_version_string, keyutils_build_string);
    return 0;
}

/*
 * show the parent process's session keyring
 */
static int rpmkeyShow(int argc, char *argv[])
{
    key_serial_t keyring = KEY_SPEC_SESSION_KEYRING;
    int hex_key_IDs = 0;

    if (argc >= 2 && strcmp(argv[1], "-x") == 0) {
	hex_key_IDs = 1;
	argc--;
	argv++;
    }

    if (argc > 2)
	rpmkeyFormat();

    if (argc == 2)
	keyring = get_key_id(argv[1]);

    dump_key_tree(keyring, argc == 2 ? "Keyring" : "Session Keyring",
		  hex_key_IDs);
    return 0;
}

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

/*
 * add a key, reading from a pipe
 */
static int rpmkeyPAdd(int argc, char *argv[])
{
    key_serial_t dest;
    size_t datalen;
    void *data;
    int ret;


    if (argc != 4)
	rpmkeyFormat();

    dest = get_key_id(argv[3]);

    data = grab_stdin(&datalen);

    ret = add_key(argv[1], argv[2], data, datalen, dest);
    if (ret < 0)
	rpmkeyError("add_key");

    /* print the resulting key ID */
    printf("%d\n", ret);
    return 0;
}

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

/*
 * request a key, with recourse to /sbin/request-key, reading the callout info
 * from a pipe
 */
static int rpmkeyPRequest2(int argc, char *argv[])
{
    char *args[6];
    size_t datalen;

    if (argc != 3 && argc != 4)
	rpmkeyFormat();

    args[0] = argv[0];
    args[1] = argv[1];
    args[2] = argv[2];
    args[3] = grab_stdin(&datalen);
    args[4] = argv[3];
    args[5] = NULL;

    return rpmkeyRequest2(argc + 1, args);
}

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

/*
 * update a key, reading from a pipe
 */
static int rpmkeyPUpdate(int argc, char *argv[])
{
    key_serial_t key;
    size_t datalen;
    void *data;

    if (argc != 2)
	rpmkeyFormat();

    key = get_key_id(argv[1]);
    data = grab_stdin(&datalen);

    if (keyctl_update(key, data, datalen) < 0)
	rpmkeyError("keyctl_update");

    return 0;
}

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

/*
 * Attempt to unlink a key matching the ID
 */
static int rpmkeyUnlink_func(key_serial_t parent, key_serial_t key,
				  char *desc, int desc_len, void *data)
{
    key_serial_t *target = data;

    if (key == *target)
	return keyctl_unlink(key, parent) < 0 ? 0 : 1;
    return 0;
}

/*
 * Unlink a key from a keyring or from the session keyring tree.
 */
static int rpmkeyUnlink(int argc, char *argv[])
{
    key_serial_t keyring, key;
    int n;

    if (argc != 2 && argc != 3)
	rpmkeyFormat();

    key = get_key_id(argv[1]);

    if (argc == 3) {
	keyring = get_key_id(argv[2]);
	if (keyctl_unlink(key, keyring) < 0)
	    rpmkeyError("keyctl_unlink");
    } else {
	n = recursive_session_key_scan(rpmkeyUnlink_func, &key);
	printf("%d links removed\n", n);
    }

    return 0;
}

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
    printf("%u bytes of data in key:\n", (unsigned)ret);

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
	printf("%u keys in keyring:\n", (unsigned)count);

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
	    fprintf(stderr,
		    "Unparseable description obtained for key %d\n", key);
	    exit(3);
	}

	calc_perms(pretty_mask, perm, uid, gid);

	printf("%9d: %s %5d %5d %*.*s: %s\n",
	       key,
	       pretty_mask, uid, gid, tlen, tlen, buffer, buffer + dpos);

	free(buffer);

    } while (--count);

    return 0;
}

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
    } else {
	pk = keylist;
	for (; count > 0; count--) {
	    key = *pk++;
	    printf("%d%c", key, count == 1 ? '\n' : ' ');
	}
    }

    return 0;
}

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
	fprintf(stderr, "Unparseable description obtained for key %d\n",
		key);
	exit(3);
    }

    /* display it */
    printf("%9d:"
	   " %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
	   " %5d %5d %*.*s: %s\n",
	   key,
	   (perm & KEY_POS_SETATTR) ? 'a' : '-',
	   (perm & KEY_POS_LINK) ? 'l' : '-',
	   (perm & KEY_POS_SEARCH) ? 's' : '-',
	   (perm & KEY_POS_WRITE) ? 'w' : '-',
	   (perm & KEY_POS_READ) ? 'r' : '-',
	   (perm & KEY_POS_VIEW) ? 'v' : '-',

	   (perm & KEY_USR_SETATTR) ? 'a' : '-',
	   (perm & KEY_USR_LINK) ? 'l' : '-',
	   (perm & KEY_USR_SEARCH) ? 's' : '-',
	   (perm & KEY_USR_WRITE) ? 'w' : '-',
	   (perm & KEY_USR_READ) ? 'r' : '-',
	   (perm & KEY_USR_VIEW) ? 'v' : '-',

	   (perm & KEY_GRP_SETATTR) ? 'a' : '-',
	   (perm & KEY_GRP_LINK) ? 'l' : '-',
	   (perm & KEY_GRP_SEARCH) ? 's' : '-',
	   (perm & KEY_GRP_WRITE) ? 'w' : '-',
	   (perm & KEY_GRP_READ) ? 'r' : '-',
	   (perm & KEY_GRP_VIEW) ? 'v' : '-',

	   (perm & KEY_OTH_SETATTR) ? 'a' : '-',
	   (perm & KEY_OTH_LINK) ? 'l' : '-',
	   (perm & KEY_OTH_SEARCH) ? 's' : '-',
	   (perm & KEY_OTH_WRITE) ? 'w' : '-',
	   (perm & KEY_OTH_READ) ? 'r' : '-',
	   (perm & KEY_OTH_VIEW) ? 'v' : '-',
	   uid, gid, tlen, tlen, buffer, buffer + dpos);

    return 0;
}

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

/*
 * instantiate a key, reading from a pipe
 */
static int rpmkeyPInstantiate(int argc, char *argv[])
{
    key_serial_t key, dest;
    size_t datalen;
    void *data;

    if (argc != 3)
	rpmkeyFormat();

    key = get_key_id(argv[1]);
    dest = get_key_id(argv[2]);
    data = grab_stdin(&datalen);

    if (keyctl_instantiate(key, data, datalen, dest) < 0)
	rpmkeyError("keyctl_instantiate");

    return 0;
}

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

/*
 * get a key's security label
 */
static int rpmkeySecurity(int argc, char *argv[])
{
    key_serial_t key;
    char *buffer;
    int ret;

    if (argc != 2)
	rpmkeyFormat();

    key = get_key_id(argv[1]);

    /* get key description */
    ret = keyctl_get_security_alloc(key, &buffer);
    if (ret < 0)
	rpmkeyError("keyctl_getsecurity");

    printf("%s\n", buffer);
    return 0;
}

/*
 * install a new session keyring on the parent process
 */
static int rpmkeyNewSession(int argc, char *argv[])
{
    key_serial_t keyring;

    if (argc != 1)
	rpmkeyFormat();

    if (keyctl_join_session_keyring(NULL) < 0)
	rpmkeyError("keyctl_join_session_keyring");

    if (keyctl_session_to_parent() < 0)
	rpmkeyError("keyctl_session_to_parent");

    keyring = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 0);
    if (keyring < 0)
	rpmkeyError("keyctl_get_keyring_ID");

    /* print the resulting key ID */
    printf("%d\n", keyring);
    return 0;
}

#ifdef	REFERENCE
/*
 * reject a key that's under construction
 */
static int rpmkeyReject(int argc, char *argv[])
{
    unsigned long timeout;
    key_serial_t key, dest;
    unsigned long rejerr;
    char *q;

    if (argc != 5)
	rpmkeyFormat();

    key = get_key_id(argv[1]);

    timeout = strtoul(argv[2], &q, 10);
    if (*q) {
	fprintf(stderr, "Unparsable timeout: '%s'\n", argv[2]);
	exit(2);
    }

    if (strcmp(argv[3], "rejected") == 0) {
	rejerr = EKEYREJECTED;
    } else if (strcmp(argv[3], "revoked") == 0) {
	rejerr = EKEYREVOKED;
    } else if (strcmp(argv[3], "expired") == 0) {
	rejerr = EKEYEXPIRED;
    } else {
	rejerr = strtoul(argv[3], &q, 10);
	if (*q) {
	    fprintf(stderr, "Unparsable error: '%s'\n", argv[3]);
	    exit(2);
	}
    }

    dest = get_key_id(argv[4]);

    if (keyctl_reject(key, timeout, rejerr, dest) < 0)
	rpmkeyError("keyctl_negate");

    return 0;
}

/*
 * Attempt to unlink a key if we can't read it for reasons other than we don't
 * have permission
 */
static int rpmkeyReap_func(key_serial_t parent, key_serial_t key,
				char *desc, int desc_len, void *data)
{
    if (desc_len < 0 && errno != EACCES) {
	if (verbose)
	    printf("Reap %d", key);
	if (keyctl_unlink(key, parent) < 0) {
	    if (verbose)
		printf("... failed %m\n");
	    return 0;
	} else {
	    if (verbose)
		printf("\n");
	    return 1;
	};
    }
    return 0;
}

/*
 * Reap the dead keys from the session keyring tree
 */
static int rpmkeyReap(int argc, char *argv[])
{
    int n;

    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
	verbose = 1;
	argc--;
	argv++;
    }

    if (argc != 1)
	rpmkeyFormat();

    n = recursive_session_key_scan(rpmkeyReap_func, NULL);
    printf("%d keys reaped\n", n);
    return 0;
}

struct purge_data {
    const char *type;
    const char *desc;
    size_t desc_len;
    size_t type_len;
    char prefix_match;
    char case_indep;
};

/*
 * Attempt to unlink a key matching the type
 */
static int rpmkeyPurge_type_func(key_serial_t parent, key_serial_t key,
				char *raw, int raw_len, void *data)
{
    const struct purge_data *purge = data;
    char *p, *type;

    if (parent == 0 || !raw)
	return 0;

    /* type is everything before the first semicolon */
    type = raw;
    p = memchr(raw, ';', raw_len);
    if (!p)
	return 0;
    *p = 0;
    if (strcmp(type, purge->type) != 0)
	return 0;

    return keyctl_unlink(key, parent) < 0 ? 0 : 1;
}

/*
 * Attempt to unlink a key matching the type and description literally
 */
static int rpmkeyPurge_literal_func(key_serial_t parent, key_serial_t key,
				char *raw, int raw_len, void *data)
{
    const struct purge_data *purge = data;
    size_t tlen;
    char *p, *type, *desc;

    if (parent == 0 || !raw)
	return 0;

    /* type is everything before the first semicolon */
    type = raw;
    p = memchr(type, ';', raw_len);
    if (!p)
	return 0;

    tlen = p - type;
    if (tlen != purge->type_len)
	return 0;
    if (memcmp(type, purge->type, tlen) != 0)
	return 0;

    /* description is everything after the last semicolon */
    p++;
    desc = memrchr(p, ';', raw + raw_len - p);
    if (!desc)
	return 0;
    desc++;

    if (purge->prefix_match) {
	if (raw_len - (desc - raw) < purge->desc_len)
	    return 0;
    } else {
	if (raw_len - (desc - raw) != purge->desc_len)
	    return 0;
    }

    if (purge->case_indep) {
	if (strncasecmp(purge->desc, desc, purge->desc_len) != 0)
	    return 0;
    } else {
	if (memcmp(purge->desc, desc, purge->desc_len) != 0)
	    return 0;
    }

    printf("%*.*s '%s'\n", (int) tlen, (int) tlen, type, desc);

    return keyctl_unlink(key, parent) < 0 ? 0 : 1;
}

/*
 * Attempt to unlink a key matching the type and description literally
 */
static int rpmkeyPurge_search_func(key_serial_t parent, key_serial_t keyring,
				char *raw, int raw_len, void *data)
{
    const struct purge_data *purge = data;
    key_serial_t key;
    int kcount = 0;

    if (!raw || memcmp(raw, "keyring;", 8) != 0)
	return 0;

    for (;;) {
	key = keyctl_search(keyring, purge->type, purge->desc, 0);
	if (keyctl_unlink(key, keyring) < 0)
	    return kcount;
	kcount++;
    }
    return kcount;
}

/*
 * Purge matching keys from a keyring
 */
static int rpmkeyPurge(int argc, char *argv[])
{
    recursive_key_scanner_t func;
    struct purge_data purge = {
	.prefix_match = 0,
	.case_indep = 0,
    };
    int n = 0, search_mode = 0;

    argc--;
    argv++;
    while (argc > 0 && argv[0][0] == '-') {
	if (argv[0][1] == 's')
	    search_mode = 1;
	else if (argv[0][1] == 'p')
	    purge.prefix_match = 1;
	else if (argv[0][1] == 'i')
	    purge.case_indep = 1;
	else
	    rpmkeyFormat();
	argc--;
	argv++;
    }

    if (argc < 1)
	rpmkeyFormat();

    purge.type = argv[0];
    purge.desc = argv[1];
    purge.type_len = strlen(purge.type);
    purge.desc_len = purge.desc ? strlen(purge.desc) : 0;

    if (search_mode == 1) {
	if (argc != 2 || purge.prefix_match || purge.case_indep)
	    rpmkeyFormat();
	/* purge all keys of a specific type and description, according
	 * to the kernel's comparator */
	func = rpmkeyPurge_search_func;
    } else if (argc == 1) {
	if (purge.prefix_match || purge.case_indep)
	    rpmkeyFormat();
	/* purge all keys of a specific type */
	func = rpmkeyPurge_type_func;
    } else if (argc == 2) {
	/* purge all keys of a specific type with literally matching
	 * description */
	func = rpmkeyPurge_literal_func;
    } else {
	rpmkeyFormat();
    }

    n = recursive_session_key_scan(func, &purge);
    printf("purged %d keys\n", n);
    return 0;
}
#endif	/* REFERENCE */

/*
 * Invalidate a key
 */
static int rpmkeyInvalidate(int argc, char *argv[])
{
    key_serial_t key;

    if (argc != 2)
	rpmkeyFormat();

    key = get_key_id(argv[1]);

    if (keyctl_invalidate(key) < 0)
	rpmkeyError("keyctl_invalidate");

    return 0;
}

/*
 * Get the per-UID persistent keyring
 */
static int rpmkeyGetPersistent(int argc, char *argv[])
{
    key_serial_t dest, ret;
    uid_t uid = -1;
    char *q;

    if (argc != 2 && argc != 3)
	rpmkeyFormat();

    dest = get_key_id(argv[1]);

    if (argc > 2) {
	uid = strtoul(argv[2], &q, 0);
	if (*q) {
	    fprintf(stderr, "Unparsable uid: '%s'\n", argv[2]);
	    exit(2);
	}
    }

    ret = keyctl_get_persistent(uid, dest);
    if (ret < 0)
	rpmkeyError("keyctl_get_persistent");

    /* print the resulting key ID */
    printf("%d\n", ret);
    return 0;
}

/*==============================================================*/

#define	ARGMINMAX(_min, _max)	(int)(((_min) << 8) | ((_max) & 0xff))

struct poptOption _rpmkeyCommandTable[] = {
  { "debug", 'd', POPT_ARG_VAL | POPT_ARGFLAG_DOC_HIDDEN, &_debug, 1,
     NULL, NULL},

  { "add", '\0', POPT_ARG_MAINCALL,	rpmkeyAdd, ARGMINMAX(5, 5),
     N_("Add key to <kr>"), N_("<type> <desc> <data> <kr>") },
  { "chgrp", '\0', POPT_ARG_MAINCALL,	rpmkeyChgrp, ARGMINMAX(3, 3),
     N_("Change <gid> on <key>"), N_("<key> <gid>") },
  { "chown", '\0', POPT_ARG_MAINCALL,	rpmkeyChown, ARGMINMAX(3, 3),
     N_("Change <uid> on <key>"), N_("<key> <uid>") },
  { "clear", '\0', POPT_ARG_MAINCALL,	rpmkeyClear, ARGMINMAX(2, 2),
     N_("Clear <kr>"), N_("<kr>") },
  { "describe", '\0', POPT_ARG_MAINCALL,	rpmkeyDescribe, ARGMINMAX(2, 2),
     N_("Describe <kr>"), N_("<kr>") },
  { "instantiate", '\0', POPT_ARG_MAINCALL,	rpmkeyInstantiate, ARGMINMAX(4, 4),
     N_("Instantiate <key> in <kr>"), N_("<key> <data> <kr>") },
  { "invalidate", '\0', POPT_ARG_MAINCALL,	rpmkeyInvalidate, ARGMINMAX(2, 2),
     N_("Invalidate <key>"), N_("<key>") },
  { "get_persistent", '\0', POPT_ARG_MAINCALL,	rpmkeyGetPersistent, ARGMINMAX(2, 3),
     N_("Get a persistent <kr>"), N_("<kr> [<uid>]") },
  { "link", '\0', POPT_ARG_MAINCALL,	rpmkeyLink, ARGMINMAX(3, 3),
     N_("Link <key> into <kr>"), N_("<key> <kr>") },
  { "list", '\0', POPT_ARG_MAINCALL,	rpmkeyList, ARGMINMAX(2, 2),
     N_("List <kr>"), N_("<kr>") },
  { "negate", '\0', POPT_ARG_MAINCALL,	rpmkeyNegate, ARGMINMAX(4, 4),
     N_("Negate <key> in <kr>"), N_("<key> <timeout> <kr>") },
  { "new_session", '\0', POPT_ARG_MAINCALL,	rpmkeyNewSession, ARGMINMAX(1, 1),
     N_("Install new session <kr>"), NULL },
  { "newring", '\0', POPT_ARG_MAINCALL,	rpmkeyNewring, ARGMINMAX(3, 3),
     N_("Create <kr>"), N_("<name> <kr>") },
  { "padd", '\0', POPT_ARG_MAINCALL,	rpmkeyPAdd, ARGMINMAX(4, 4),
     N_("Add key to <kr>"), N_("<type> <desc> <kr>") },
  { "pinstantiate", '\0', POPT_ARG_MAINCALL,	rpmkeyPInstantiate, ARGMINMAX(3, 3),
     N_("Instantiate <key> in <kr>"), N_("<key> <kr>") },
  { "pipe", '\0', POPT_ARG_MAINCALL,	rpmkeyPipe, ARGMINMAX(2, 2),
     N_("Read <key>"), N_("<key>") },
  { "prequest2", '\0', POPT_ARG_MAINCALL,	rpmkeyPRequest2, ARGMINMAX(3, 4),
     N_("Request <key>"), N_("<type> <desc> [<destkr>]") },
  { "print", '\0', POPT_ARG_MAINCALL,	rpmkeyPrint, ARGMINMAX(2, 2),
     N_("Print <key>"), N_("<key>") },
  { "pupdate", '\0', POPT_ARG_MAINCALL,	rpmkeyPUpdate, ARGMINMAX(2, 2),
     N_("Update <key>"), N_("<key>") },
#ifdef	NOTYET
	/* XXX FIXME: variable args and flags */
  { "purge", '\0', POPT_ARG_MAINCALL,	rpmkeyPurge, ARGMINMAX(0, 0),
     N_("Purge all keys of <type>"), N_("<type>") },
	/* XXX FIXME: variable args and flags */
  { "purge", '\0', POPT_ARG_MAINCALL,	NULL, ARGMINMAX(0, 0),
     N_(" Purge <key>"), N_("[-p] [-i] <type> <desc>") },
	/* XXX FIXME: variable args and flags */
  { "purge", '\0', POPT_ARG_MAINCALL,	NULL, ARGMINMAX(0, 0),
     N_("Purge <key>"), N_("-s <type> <desc>") },
#endif
  { "rdescribe", '\0', POPT_ARG_MAINCALL,	rpmkeyRDescribe, ARGMINMAX(2, 3),
     N_("Describe <kr>"), N_("<kr> [sep]") },
  { "read", '\0', POPT_ARG_MAINCALL,	rpmkeyRead, ARGMINMAX(2, 2),
     N_("Read <key>"), N_("<key>") },
#ifdef	NOTYET
  { "reap", '\0', POPT_ARG_MAINCALL,	rpmkeyReap, ARGMINMAX(1, 2),
     N_("Reap"), N_("[-v]") },
  { "reject", '\0', POPT_ARG_MAINCALL,	rpmkeyReject, ARGMINMAX(5, 5),
     N_("Reject"), N_("<key> <timeout> <error> <kr>") },
#endif
  { "request", '\0', POPT_ARG_MAINCALL,	rpmkeyRequest, ARGMINMAX(3, 4),
     N_("Request key"), N_("<type> <desc> [<destkr>]") },
  { "request2", '\0', POPT_ARG_MAINCALL,	rpmkeyRequest2, ARGMINMAX(0, 0),
     N_("Request <key>"), N_("<type> <desc> <info> [<destkr>]") },
  { "revoke", '\0', POPT_ARG_MAINCALL,	rpmkeyRevoke, ARGMINMAX(4, 5),
     N_("Revoke <key>"), N_("<key>") },
  { "rlist", '\0', POPT_ARG_MAINCALL,	rpmkeyRList, ARGMINMAX(2, 2),
     N_("List <kr>"), N_("<kr>") },
  { "search", '\0', POPT_ARG_MAINCALL,	rpmkeySearch, ARGMINMAX(4, 5),
     N_("Search <kr>"), N_("<kr> <type> <desc> [<destkr>]") },
  { "security", '\0', POPT_ARG_MAINCALL,	rpmkeySecurity, ARGMINMAX(2, 2),
     N_("Retrieve <key> security"), N_("<key>") },
	/* XXX FIXME: variable args and flags */
  { "session", '\0', POPT_ARG_MAINCALL,	rpmkeySession, ARGMINMAX(0, 0),
     N_("Start new session"), NULL },
#ifdef	NOTYET
	/* XXX FIXME: variable args and flags */
  { "session", '\0', POPT_ARG_MAINCALL,	NULL, ARGMINMAX(0, 0),
     N_("Start new session"),
     N_("- [<prog> <arg1> <arg2> ...]") },
	/* XXX FIXME: variable args and flags */
  { "session", '\0', POPT_ARG_MAINCALL,	NULL, ARGMINMAX(0, 0),
     N_("Start new session"),
     N_("<name> [<prog> <arg1> <arg2> ...]") },
#endif
  { "setperm", '\0', POPT_ARG_MAINCALL,	rpmkeySetperm, ARGMINMAX(3, 3),
     N_("Set <mask> on <kr>"), N_("<key> <mask>") },
  { "show", '\0', POPT_ARG_MAINCALL,	rpmkeyShow, ARGMINMAX(1, 3),
     N_("Show process <kr>"), N_("[-x] [<kr>]") },
  { "timeout", '\0', POPT_ARG_MAINCALL,	rpmkeyTimeout, ARGMINMAX(3, 3),
     N_("Set <timeout> on <key>"), N_("<key> <timeout>") },
  { "unlink", '\0', POPT_ARG_MAINCALL,	rpmkeyUnlink, ARGMINMAX(2, 3),
     N_("Unlink <key> from <kr>"), N_("<key> [<kr>]") },
  { "update", '\0', POPT_ARG_MAINCALL,	rpmkeyUpdate, ARGMINMAX(3, 3),
     N_("Update <key> with <data>"), N_("<key> <data>") },
  { "version", '\0', POPT_ARG_MAINCALL,	rpmkeyVersion, ARGMINMAX(1, 1),
     N_("Display keyutils version"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP

  { NULL, (char) -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Format:\n\
  keyctl --version \n\
  keyctl add <type> <desc> <data> <keyring>\n\
  keyctl chgrp <key> <gid>\n\
  keyctl chown <key> <uid>\n\
  keyctl clear <keyring>\n\
  keyctl describe <keyring>\n\
  keyctl instantiate <key> <data> <keyring>\n\
  keyctl invalidate <key>\n\
  keyctl get_persistent <keyring> [<uid>]\n\
  keyctl link <key> <keyring>\n\
  keyctl list <keyring>\n\
  keyctl negate <key> <timeout> <keyring>\n\
  keyctl new_session \n\
  keyctl newring <name> <keyring>\n\
  keyctl padd <type> <desc> <keyring>\n\
  keyctl pinstantiate <key> <keyring>\n\
  keyctl pipe <key>\n\
  keyctl prequest2 <type> <desc> [<dest_keyring>]\n\
  keyctl print <key>\n\
  keyctl pupdate <key>\n\
  keyctl purge <type>\n\
  keyctl purge [-p] [-i] <type> <desc>\n\
  keyctl purge -s <type> <desc>\n\
  keyctl rdescribe <keyring> [sep]\n\
  keyctl read <key>\n\
  keyctl reap [-v]\n\
  keyctl reject <key> <timeout> <error> <keyring>\n\
  keyctl request <type> <desc> [<dest_keyring>]\n\
  keyctl request2 <type> <desc> <info> [<dest_keyring>]\n\
  keyctl revoke <key>\n\
  keyctl rlist <keyring>\n\
  keyctl search <keyring> <type> <desc> [<dest_keyring>]\n\
  keyctl security <key>\n\
  keyctl session \n\
  keyctl session - [<prog> <arg1> <arg2> ...]\n\
  keyctl session <name> [<prog> <arg1> <arg2> ...]\n\
  keyctl setperm <key> <mask>\n\
  keyctl show [-x] [<keyring>]\n\
  keyctl timeout <key> <timeout>\n\
  keyctl unlink <key> [<keyring>]\n\
  keyctl update <key> <data>\n\
\n\
Key/keyring ID:\n\
  <nnn>   numeric keyring ID\n\
  @t      thread keyring\n\
  @p      process keyring\n\
  @s      session keyring\n\
  @u      user keyring\n\
  @us     user default session keyring\n\
  @g      group keyring\n\
  @a      assumed request_key authorisation key\n\
\n\
<type> can be \"user\" for a user-defined keyring\n\
If you do this, prefix the description with \"<subtype>:\"\n\
"), NULL},

    POPT_TABLEEND
};

static int cmd_run(int argc, char *argv[])
{
    const struct poptOption * c;
    const char * cmd;
    int rc = 1;

    if (argv == NULL || argv[0] == NULL)	/* XXX segfault avoidance */
	goto exit;
    cmd = argv[0];
    for (c = _rpmkeyCommandTable; c->longName != NULL; c++) {
	int (*func) (int argc, char *argv[]) = NULL;

	if (strcmp(cmd, c->longName))
	    continue;

	func = c->arg;
	rc = (*func) (argc, argv);
	break;
    }

exit:
    return rc;
}

/*
 * execute the appropriate subcommand
 */
int main(int argc, char *argv[])
{
    char * arg0 = argv[0];
    poptContext con = rpmioInit(argc, argv, _rpmkeyCommandTable);
    char ** av = NULL;
    int ac;
    int ec;
    int xx;

    while ((xx = poptGetNextOpt(con)) > 0)
    switch (xx) {
    default:
	fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		arg0, xx);
	ec = EXIT_FAILURE;
	goto exit;
	break;
    }

    av = (char **) poptGetArgs(con);
    for (ac = 0; av[ac] != NULL; ac++)
	;

    ec = cmd_run(ac, av);
    if (ec)
	goto exit;

exit:
    con = rpmioFini(con);
    return ec;

}
