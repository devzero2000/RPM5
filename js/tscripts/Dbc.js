if (loglvl) print("--> Dbc.js");

var dbc = new Dbc();
ack("typeof dbc;", "object");
ack("dbc instanceof Dbc;", true);
ack("dbc.debug = 1;", 1);
ack("dbc.debug = 0;", 0);

dbc.close();

if (loglvl) print("<-- Dbc.js");
