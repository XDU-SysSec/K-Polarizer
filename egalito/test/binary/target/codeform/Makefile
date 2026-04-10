# Makefile for codeform test case
include ../../testenv.mk
ROOT = ../..

TARGET = $(shell basename $$(pwd))
DIR = dir

.PHONY: all
all: $(DIR)/codeform $(DIR)/codeform-s

$(DIR):
	@unzip -q codeform.zip -d $(DIR)

$(DIR)/codeform: $(DIR)
	@cd $(DIR) && $(MAKE) CC=$(CC) CFLAGS="-W -Wall -ansi -pedantic -O2 -g -Wno-unused-but-set-variable $(WITH_PIE)"

$(DIR)/codeform-s: $(DIR)/codeform
	@cp $(DIR)/codeform $(DIR)/codeform-s
	strip $(DIR)/codeform-s

.PHONY: clean
clean:
	-@rm -rf $(DIR)
