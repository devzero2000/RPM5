if ($loglvl)
  print("--> Hdr.rb\n");
end

$RPMTAG_NAME = 1000;
$N = "popt";

$mi = Hdr.new
ack("$mi.instance_of?(Hdr)", true)
ack("$mi.kind_of?(Hdr)", true)
ack("$mi.class.to_s", "Hdr")
ack("$mi.debug = 1;", 1);
ack("$mi.debug = 0;", 0);

if ($loglvl)
  print("<-- Hdr.rb\n");
end
