# Makefile for a test case
include ../../testenv.mk
ROOT = ../..

TARGET = $(shell basename $$(pwd))
SOURCES = $(wildcard *.c) $(wildcard *.cpp) $(wildcard *.s) $(wildcard *.S)

.PHONY: all
all: $(BUILDDIR)$(TARGET)

$(BUILDDIR)$(TARGET): $(SOURCES)
	@mkdir -p $(BUILDDIR)
	$(SHORT_CC) $(CFLAGS) $(WITH_PIE) -o $@ $^ -ffreestanding -nostdlib
	@ln -sf $@ $(TARGET)

.PHONY: clean
clean:
	-@rm -f $(BUILDDIR)$(TARGET) $(TARGET)
