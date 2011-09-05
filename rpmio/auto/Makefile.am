AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = types desc

# http://ispras.linuxfoundation.org/index.php/ABI_compliance_checker
ABITEST =	@abs_top_srcdir@/scripts/abi-compliance-checker.pl
# http://ispras.linuxfoundation.org/index.php/API_Sanity_Autotest
AUTOTEST =	@abs_top_srcdir@/scripts/api-sanity-autotest.pl

TDIRS =		descriptors_storage header_compile_errors test_results tests

desc.xml: desc
	cp $< $@

clean-local:
	rm -rf $(TDIRS) desc.xml

check-local: desc.xml
	-[ -d tests ] && ${AUTOTEST} -l rpmio -d desc.xml -clean
	-${AUTOTEST} -l rpmio -d desc.xml -st types -td test_data \
		-gen -splint-specs -build -run
