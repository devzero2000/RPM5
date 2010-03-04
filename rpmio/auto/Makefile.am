AUTOMAKE_OPTIONS = 1.4 foreign

AUTOTEST =      api-sanity-autotest.pl

TDIRS =		descriptors_storage header_compile_errors test_results tests

clean-local:
	rm -rf $(TDIRS)

check-local:
	-[ -d tests ] && ${AUTOTEST} -l rpmio -d desc -clean
	-${AUTOTEST} -l rpmio -d desc -st types -gen -build -run
