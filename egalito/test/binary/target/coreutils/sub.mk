# Makefile for coreutils test case
include ../../testenv.mk

LDFLAGS =

.PHONY: all
all: $(DIR)

$(DIR): build/Makefile;
	@$(MAKE) -C build install > /dev/null

build/Makefile: $(PACKAGE)
	@mkdir -p build && \
	cd build && \
	env CC=$(CC) ../$(PACKAGE)/configure --prefix=$(abspath ./$(DIR)) > /dev/null 2>&1

$(PACKAGE): $(PACKAGE).tar.xz
	@tar axf $<
	@touch $@

$(PACKAGE).tar.xz:
	@wget https://ftp.gnu.org/gnu/coreutils/$(PACKAGE).tar.xz > /dev/null 2>&1

.PHONY: clean
clean:
	$(if $(wildcard build/Makefile),$(MAKE) -C build clean)

realclean:
	-@rm -rf $(DIR) $(PACKAGE) $(PACKAGE).tar.xz build
