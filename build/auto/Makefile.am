AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = types desc

# http://forge.ispras.ru/projects/abi-compliance-checker
ABITEST =       @abs_top_srcdir@/scripts/abi-check/abi-compliance-checker.pl
# http://forge.ispras.ru/projects/api-sanity-autotest
AUTOTEST =      @abs_top_srcdir@/scripts/api-sanity-autotest.pl

TDIRS =		descriptors_storage header_compile_errors test_results tests

desc.xml: desc
	cp $< $@

clean-local:
	rm -rf $(TDIRS) desc.xml

check-local: desc.xml
	-[ -d tests ] && ${AUTOTEST} -l build -d desc.xml -clean
	-${AUTOTEST} -l build -d desc.xml -st types -td test_data \
		-gen -splint-specs -build -run

