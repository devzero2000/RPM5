if (loglvl) print("--> Sm.js");

const RPMSM_FLAGS_NONE    = 0;
const RPMSM_FLAGS_BASE    = (1 <<  1);    /* -b,--base ... */
const RPMSM_FLAGS_INSTALL = (1 <<  2);    /* -i,--install ... */
const RPMSM_FLAGS_LIST    = (1 <<  3);    /* -l,--list-modules ... */
const RPMSM_FLAGS_REMOVE  = (1 <<  4);    /* -r,--remove ... */
const RPMSM_FLAGS_UPGRADE = (1 <<  5);    /* -u,--upgrade ... */
const RPMSM_FLAGS_RELOAD  = (1 <<  6);    /* -R,--reload ... */
const RPMSM_FLAGS_BUILD   = (1 <<  7);    /* -B,--build ... */
const RPMSM_FLAGS_NOAUDIT = (1 <<  8);    /* -D,--disable_dontaudit ... */
const RPMSM_FLAGS_COMMIT  = (1 <<  9);
const RPMSM_FLAGS_CREATE  = (1 << 10);

var smstore = "targeted";
var smflags = 0;

var sm = new Sm(smstore, smflags);
ack("typeof sm;", "object");
ack("sm instanceof Sm;", true);
// ack("sm.debug = 1;", 1);
// ack("sm.debug = 0;", 0);

ack("sm.store", smstore);
ack("sm.flags", smflags);

nack('sm("list unconfined");', false);

delete sm;

if (loglvl) print("<-- Sm.js");
