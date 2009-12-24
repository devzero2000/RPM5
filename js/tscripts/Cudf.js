if (loglvl) print("--> Cudf.js");

var cudf = new Cudf();
ack("typeof cudf;", "object");
ack("cudf instanceof Cudf;", true);
ack("cudf.debug = 1;", 1);
ack("cudf.debug = 0;", 0);

ack('cudf.preamble', false);
ack('cudf.request', false);
ack('cudf.consistent', false);
ack('cudf.installedsize', 0);
ack('cudf.universesize', 0);
delete cudf

var fn = "legacy.cudf";
cudf = new Cudf(fn);
ack('cudf.preamble', true);
ack('cudf.request', true);
ack('cudf.consistent', true);
ack('cudf.installedsize', 6);
ack('cudf.universesize', 20);
delete cudf

if (loglvl) print("<-- Cudf.js");
