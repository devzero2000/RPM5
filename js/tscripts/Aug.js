if (loglvl) print("--> Aug.js");

var aug = new Aug();
ack("typeof aug;", "object");
ack("aug instanceof Aug;", true);
ack("aug = 1;", 1);
ack("aug = 0;", 0);

if (loglvl) print("<-- Aug.js");
