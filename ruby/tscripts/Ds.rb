if ($loglvl)
  print("--> Ds.rb\n");
end

$RPMTAG_NAME = 1000;
$N = "popt";

$ds = Ds.new
ack("$ds.instance_of?(Ds)", true)
ack("$ds.kind_of?(Ds)", true)
ack("$ds.class.to_s", "Ds")
ack("$ds.debug = 1;", 1);
ack("$ds.debug = 0;", 0);

if ($loglvl)
  print("<-- Ds.rb\n");
end
