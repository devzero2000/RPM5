APP_NAME      = rpm-test-harness
VERSION       = 12.2
NV            = $(APP_NAME)-$(VERSION)
TMP_DIR       = /tmp
PREFIX        = /
BUILD_ROOT    = $(TMP_DIR)/$(USER)
BUILD_DIR     = $(BUILD_ROOT)$(PREFIX)/$(NV)
SRC_DIR       = $(shell pwd)
TARBALL       = $(BUILD_ROOT)/$(NV).tgz
CACHE_DIR     = $(SRC_DIR)/cache
RPMSOURCE_DIR = $(shell rpm --eval '%{_sourcedir}')
SPECFILE      = $(shell pwd)/$(APP_NAME).spec

.PHONY: test

test:
	@echo "APP_NAME  = $(APP_NAME)"
	@echo "VERSION   = $(VERSION)"
	@echo "NV        = $(NV)"
	@echo "TMP_DIR   = $(TMP_DIR)"
	@echo "BUILD_DIR = $(BUILD_DIR)"
	@echo "SRC_DIR   = $(SRC_DIR)"
	@echo "TARBALL   = $(TARBALL)"

buildprep:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	(cd $(SRC_DIR); tar cf - .) | (cd $(BUILD_DIR); tar xvf -)
	cd $(BUILD_DIR); make clean
	cd $(BUILD_DIR); ./init clean

distro: 
	make buildprep
	cd $(BUILD_ROOT); tar czhvf "$(TARBALL)" "$(NV)"
	@echo ""
	@echo "TARBALL: $(TARBALL)"
	@echo ""

rpmbuild:
	make buildprep

rpm:
	make distro
	cp $(TARBALL) $(RPMSOURCE_DIR)
	rpmbuild -ba --define 'version $(VERSION)' $(SPECFILE)

clean:
	@echo "Cleaning up core files..."
	rm -f core.*
	rm -f core

