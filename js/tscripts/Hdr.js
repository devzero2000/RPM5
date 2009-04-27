var h = mi.next()
ack("typeof h;", "object");
ack("h instanceof Hdr;", true);
ack("h.debug = 1;", 1);
ack("h.debug = 0;", 0);
