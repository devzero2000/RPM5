var fi = new Fi();
ack("typeof fi;", "object");
ack("fi instanceof Fi;", true);
ack("fi.debug = 0;", 0);
ack("fi.debug = 1;", 1);
