if ($loglvl)
  print("--> Ps.rb\n");
end

$RPMTAG_NAME = 1000;
$N = "popt";

$ps = Ps.new
ack("$ps.instance_of?(Ps)", true)
ack("$ps.kind_of?(Ps)", true)
ack("$ps.class.to_s", "Ps")
ack("$ps.debug = 1;", 1);
ack("$ps.debug = 0;", 0);

if ($loglvl)
  print("<-- Ps.rb\n");
end
