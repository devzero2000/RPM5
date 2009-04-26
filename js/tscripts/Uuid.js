var uuid = new Uuid();
ack("typeof uuid;", "object");
ack("uuid instanceof Uuid;", true);
ack("uuid.debug = 0;", 0);
ack("uuid.debug = 1;", 1);
