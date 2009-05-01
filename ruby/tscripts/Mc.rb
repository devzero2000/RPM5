if ($loglvl)
  print("--> Mc.rb\n");
end

$RPMTAG_NAME = 1000;
$N = "popt";

$mc = Mc.new
ack("$mc.instance_of?(Mc)", true)
ack("$mc.kind_of?(Mc)", true)
ack("$mc.class.to_s", "Mc")
ack("$mc.debug = 1;", 1);
ack("$mc.debug = 0;", 0);

if ($loglvl)
  print("<-- Mc.rb\n");
end
