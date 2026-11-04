##
## Makefile
##
## Copyright (C) 2026, Charles Chiou
##

SHELL := /bin/bash
BUILD_DIR := build
NUM_PROCS := $(shell nproc 2>/dev/null || echo 4)
CMAKE := $(shell if [ -x /usr/bin/cmake ]; then echo /usr/bin/cmake; else echo cmake; fi)

SANITIZE ?= 0
ifneq (,$(findstring -fsanitize,$(CFLAGS) $(CXXFLAGS)))
SANITIZE := 1
endif
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release
ifeq ($(SANITIZE),1)
BUILD_DIR := build/asan
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DOCTI_CORE_SANITIZE=ON
endif

.PHONY: all clean distclean test

all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && (test -f Makefile || $(CMAKE) $(CURDIR) $(CMAKE_FLAGS))
ifeq ($(SANITIZE),1)
	@cd $(BUILD_DIR) && $(CMAKE) $(CURDIR) $(CMAKE_FLAGS) > /dev/null
endif
	@$(MAKE) -C $(BUILD_DIR) -j$(NUM_PROCS)

test: all
	@./$(BUILD_DIR)/test_octi_core

clean:
	@if [ -d $(BUILD_DIR) ] && [ -f $(BUILD_DIR)/Makefile ]; then \
		$(MAKE) -C $(BUILD_DIR) clean; \
	fi

distclean:
	@rm -rf build
