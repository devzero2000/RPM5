if (loglvl) print("--> Cudf.js");

var cudf = new Cudf();
ack("typeof cudf;", "object");
ack("cudf instanceof Cudf;", true);
ack("cudf.debug = 1;", 1);
ack("cudf.debug = 0;", 0);

if (loglvl) print("<-- Cudf.js");
