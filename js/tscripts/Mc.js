if (loglvl) print("--> Mc.js");

var mc = new Mc();
ack("typeof mc;", "object");
ack("mc instanceof Mc;", true);
ack("mc.debug = 1;", 1);
ack("mc.debug = 0;", 0);

ack('mc.expand("%{_bindir}")', "/usr/bin");

// XXX noisy, adding pattern/index/level filters todo++
// nack('mc.list()', null);
ack('mc.add("foo bar")', true);
ack('mc.expand("%{foo}")', "bar");
ack('mc.del("foo")', true);
ack('mc.expand("%{foo}")', "%{foo}");
// XXX noisy, adding pattern/index/level filters todo++
// ack('mc.list()', false);

ack('mc.expand("%{lua:print(\\"lua\\")}")',	"lua");

// FIXME: reloading rpm modules within embedded interpreter segfaults
// ack('mc.expand("%{perl:print \\"perl\\"}")',	"perl");
// ack('mc.expand("%{python:print \\"python\\"}")', "python");

ack('mc.expand("%{ruby:puts \\"ruby\\"}")',	"ruby");
ack('mc.expand("%{tcl:puts \\"tcl\\"}")',	"tcl");
delete mc

mc = new Mc("cli");
ack('mc.list()', null);
ack('mc.add("foo bar")', true);
ack('mc.list()', "%foo	bar");
ack('mc.expand("%{foo}")', "bar");
ack('mc.del("foo")', true);
ack('mc.list()', null);
ack('mc.expand("%{foo}")', "%{foo}");
delete mc

mc = new Mc("tscripts/macros");
ack('mc.list()', null);
ack('mc.add("foo bar")', true);
ack('mc.list()', "%foo	bar");
ack('mc.expand("%{foo}")', "bar");
ack('mc.del("foo")', true);
ack('mc.list()', null);
ack('mc.expand("%{foo}")', "%{foo}");
delete mc

mc = new Mc("");
ack('mc.list()', null);
ack('mc.add("foo bar")', true);
ack('mc.list()', "%foo	bar");
ack('mc.expand("%{foo}")', "bar");
ack('mc.del("foo")', true);
ack('mc.list()', null);
ack('mc.expand("%{foo}")', "%{foo}");
delete mc

// FIXME: there's no internal code path error returns to force an error out.
mc = new Mc("tscripts/nonexistent");
delete mc

if (loglvl) print("<-- Mc.js");
