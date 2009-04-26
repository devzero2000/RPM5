var mi = new Mi(ts);
ack("typeof mi;", "object");
ack("mi instanceof Mi;", true);
ack("mi.debug = 0;", 0);
ack("mi.debug = 1;", 1);
