var te = new Te(ts,h)
ack("typeof te;", "object");
ack("te instanceof Te;", true);
ack("te.debug = 1;", 1);
ack("te.debug = 0;", 0);
