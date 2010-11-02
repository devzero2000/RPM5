AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = types desc

# http://ispras.linuxfoundation.org/index.php/ABI_compliance_checker
ABITEST =	@abs_top_srcdir@/scripts/abi-compliance-checker.pl
# http://ispras.linuxfoundation.org/index.php/API_Sanity_Autotest
AUTOTEST =	@abs_top_srcdir@/scripts/api-sanity-autotest.pl

TDIRS =		descriptors_storage header_compile_errors test_results tests

clean-local:
	rm -rf $(TDIRS)

check-local:
	-[ -d tests ] && ${AUTOTEST} -l rpmio -d desc -clean
	-${AUTOTEST} -l rpmio -d desc -st types -td test_data -gen -build -run
