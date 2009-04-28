if (loglvl) print("--> Mc.js");

var mc = new Mc();
ack("typeof mc;", "object");
ack("mc instanceof Mc;", true);
ack("mc.debug = 1;", 1);
ack("mc.debug = 0;", 0);

ack('mc.expand("%{_bindir}")', "/usr/bin");

ack('mc.add("foo bar")', true);
ack('mc.expand("%{foo}")', "bar");
ack('mc.del("foo")', true);

ack('mc.expand("%{lua:print(\\"lua\\")}")',	"lua");

// FIXME: reloading rpm modules within embedded interpreter segfaults
// ack('mc.expand("%{perl:print \\"perl\\"}")',	"perl");
// ack('mc.expand("%{python:print \\"python\\"}")', "python");

ack('mc.expand("%{ruby:puts \\"ruby\\"}")',	"ruby");
ack('mc.expand("%{tcl:puts \\"tcl\\"}")',	"tcl");

if (loglvl) print("<-- Mc.js");
