var ps = new Ps();
ack("typeof ps;", "object");
ack("ps instanceof Ps;", true);
ack("ps.debug = 0;", 0);
ack("ps.debug = 1;", 1);
