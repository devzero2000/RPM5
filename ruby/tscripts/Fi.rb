if ($loglvl)
  print("--> Fi.rb\n");
end

$RPMTAG_NAME = 1000;
$N = "popt";

$fi = Fi.new
ack("$fi.instance_of?(Fi)", true)
ack("$fi.kind_of?(Fi)", true)
ack("$fi.class.to_s", "Fi")
ack("$fi.debug = 1;", 1);
ack("$fi.debug = 0;", 0);

if ($loglvl)
  print("<-- Fi.rb\n");
end
