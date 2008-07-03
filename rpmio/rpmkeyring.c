#include "system.h"

#include "rpmio_internal.h"
#include "rpmkeyring.h"

#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

struct rpmPubkey_s {
    uint8_t * pkt;
    size_t pktlen;
    pgpKeyID_t keyid;
};

struct rpmKeyring_s {
    rpmPubkey * keys;
    size_t numkeys;
};

static int keyidcmp(const void *k1, const void *k2)
{
    const struct rpmPubkey_s *key1 = *(const struct rpmPubkey_s **) k1;
    const struct rpmPubkey_s *key2 = *(const struct rpmPubkey_s **) k2;
    return memcmp(key1->keyid, key2->keyid, sizeof(key1->keyid));
}

rpmKeyring rpmKeyringNew(void)
{
    rpmKeyring keyring = xcalloc(1, sizeof(*keyring));
    keyring->keys = NULL;
    keyring->numkeys = 0;
    return keyring;
}

rpmKeyring rpmKeyringFree(rpmKeyring keyring)
{
    if (keyring && keyring->keys) {
	int i;
/*@-unqualifiedtrans @*/
	for (i = 0; i < (int)keyring->numkeys; i++)
	    keyring->keys[i] = rpmPubkeyFree(keyring->keys[i]);
/*@=unqualifiedtrans @*/
	keyring->keys = _free(keyring->keys);
    }
    keyring = _free(keyring);
    return NULL;
}

/*@owned@*/
static rpmPubkey rpmKeyringFindKeyid(rpmKeyring keyring, rpmPubkey key)
	/*@*/
{
    rpmPubkey *found = NULL;
    found = bsearch(&key, keyring->keys, keyring->numkeys, sizeof(*keyring->keys), keyidcmp);
    return found ? *found : NULL;
}

int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key)
{
    if (keyring == NULL || key == NULL)
	return -1;

    /* check if we already have this key */
    if (rpmKeyringFindKeyid(keyring, key))
	return 1;

    keyring->keys = xrealloc(keyring->keys, (keyring->numkeys + 1) * sizeof(*keyring->keys));
/*@-assignexpose @*/
    keyring->keys[keyring->numkeys] = key;
/*@=assignexpose @*/
    keyring->numkeys++;
    qsort(keyring->keys, keyring->numkeys, sizeof(*keyring->keys), keyidcmp);

    return 0;
}

rpmPubkey rpmPubkeyRead(const char *filename)
{
    uint8_t *pkt = NULL;
    size_t pktlen = 0;
    rpmPubkey key = NULL;

    if (pgpReadPkts(filename, &pkt, &pktlen) > 0) {
	key = rpmPubkeyNew(pkt, pktlen);
	pkt = _free(pkt);
    }

exit:
    return key;
}

rpmPubkey rpmPubkeyNew(const uint8_t *pkt, size_t pktlen)
{
    rpmPubkey key = NULL;

    if (pkt != NULL && pktlen > 0) {
	key = xcalloc(1, sizeof(*key));
	(void) pgpPubkeyFingerprint(pkt, pktlen, key->keyid);
	key->pkt = xmalloc(pktlen);
	key->pktlen = pktlen;
	memcpy(key->pkt, pkt, pktlen);
    }
    return key;
}

rpmPubkey rpmPubkeyFree(rpmPubkey key)
{
    if (key != NULL) {
	key->pkt = _free(key->pkt);
	key = _free(key);
    }
    return NULL;
}

rpmRC rpmKeyringLookup(rpmKeyring keyring, pgpDig sig)
{
    rpmRC res = RPMRC_NOKEY;

    if (keyring && sig) {
	pgpDigParams sigp = &sig->signature;
	pgpDigParams pubp = &sig->pubkey;
	struct rpmPubkey_s needle, *key;
	needle.pkt = NULL;
	needle.pktlen = 0;
	memcpy(needle.keyid, sigp->signid, sizeof(needle.keyid));

	if ((key = rpmKeyringFindKeyid(keyring, &needle))) {
	    /* Retrieve parameters from pubkey packet(s) */
	    (void) pgpPrtPkts(key->pkt, key->pktlen, sig, 0);
	    /* Do the parameters match the signature? */
	    if (sigp->pubkey_algo == pubp->pubkey_algo &&
		memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid)) == 0) {
		res = RPMRC_OK;
	    }
	}
    }

    return res;
}
