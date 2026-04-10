include testenv.mk

TARGET_DIRS = $(filter-out target/Makefile,$(wildcard target/*))
CONFIG_DIRS = $(wildcard x86_64*) $(wildcard aarch64*) $(wildcard arm*)

.PHONY: all clean realclean
all:  # default target

all clean:
	@echo '>>>' MAKE -C 'target/*' $@
	@$(foreach DIR, $(TARGET_DIRS), \
		$(MAKE) -C $(DIR) PLATFORM=$(PLATFORM) --silent --no-print-directory $@;)
	@echo '<<<' MAKE -C 'target/*' $@

realclean:
	$(foreach CONFIG, $(CONFIG_DIRS), rm -f $(CONFIG)/build/*;)

symlinks:
	@ln -sfn $(PLATFORM) platform
	@ln -sfn $(PLATFORM)/build build
