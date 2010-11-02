AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = types desc

AUTOTEST =      @abs_top_srcdir@/scripts/api-sanity-autotest.pl

TDIRS =		descriptors_storage header_compile_errors test_results tests

clean-local:
	rm -rf $(TDIRS)

check-local:
	-[ -d tests ] && ${AUTOTEST} -l build -d desc -clean
	-${AUTOTEST} -l build -d desc -st types -gen -build -run
