# Makefile for coreutils test case
export PACKAGE = coreutils-8.29
export DIR = install

.PHONY: all full
all:
	$(info Skipping coreutils test, use 'make full' in test/binary/target/coreutils to build)
full: $(DIR)

$(DIR):
	$(MAKE) -f sub.mk all
	mv $(DIR) $(DIR).b
	$(MAKE) -f sub.mk clean
	$(MAKE) -f sub.mk all LDFLAGS='-Wl,-q'
	$(MAKE) rename
	mv $(DIR).b/bin/* $(DIR)/bin
	rm -fr $(DIR).b

rename:
	$(foreach f,$(wildcard $(DIR)/bin/*), mv $f $f-q;)

.PHONY: clean realclean
clean realclean:
	$(MAKE) -f sub.mk $@

