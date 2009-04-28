if (loglvl) print("--> Macro.js");

var macro = new Macro();
ack("typeof macro;", "object");
ack("macro instanceof Macro;", true);
ack("macro.debug = 1;", 1);
ack("macro.debug = 0;", 0);


ack('macro.expand("%{_bindir}")', "/usr/bin");

ack('macro.expand("%{lua:print(\\"lua\\")}")',	"lua");

// FIXME: reloading rpm modules within embedded interpreter segfaults
// ack('macro.expand("%{perl:print \\"perl\\"}")',	"perl");
// ack('macro.expand("%{python:print \\"python\\"}")', "python");

ack('macro.expand("%{ruby:puts \\"ruby\\"}")',	"ruby");
ack('macro.expand("%{tcl:puts \\"tcl\\"}")',	"tcl");

if (loglvl) print("<-- Macro.js");
