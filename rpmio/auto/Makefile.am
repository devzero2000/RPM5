AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = types desc

# http://forge.ispras.ru/projects/abi-compliance-checker
ABITEST =	abi-compliance-checker
# http://forge.ispras.ru/projects/api-sanity-autotest
AUTOTEST =	api-sanity-checker

TDIRS =		descriptors_storage header_compile_errors test_results tests

desc.xml: desc
	cp $< $@

clean-local:
	rm -rf $(TDIRS) desc.xml

check-local: desc.xml
	-[ -d tests ] && ${AUTOTEST} -l rpmio -d desc.xml -clean
	-${AUTOTEST} -l rpmio -d desc.xml -st types -td test_data \
		-gen -build -run
