# Makefile for nginx test case
include ../../testenv.mk

DIR = nginx

.PHONY: all full
all:
	$(info Skipping nginx test, use 'make full' in test/binary/target/nginx to build)
full: $(DIR)/sbin/nginx

$(DIR)/sbin/nginx:
	@cd nginx-1.11.3; \
	./configure --prefix=$(abspath ./$(DIR)) --with-cc-opt='-O2 -fPIC' --with-ld-opt='-pie' --with-threads > config.log 2>&1 || \
		tail config.log;\
	$(MAKE); \
	$(MAKE) install; \
	cd - > /dev/null; \
	rm -rf $(DIR)/conf; \
	dd if=/dev/zero of=$(DIR)/html/100k bs=1024 count=100 > /dev/null 2>&1; \
	dd if=/dev/zero of=$(DIR)/html/1024k bs=1024 count=1024 > /dev/null 2>&1

.PHONY: clean
clean:
	-@rm -rf $(DIR)
	$(if $(wildcard nginx-1.11.3/Makefile),$(MAKE) -C nginx-1.11.3 clean)
