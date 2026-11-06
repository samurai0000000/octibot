##
## Makefile
##
## Copyright (C) 2026, Charles Chiou
##

SHELL := /bin/bash
BUILD_DIR := build
NUM_PROCS := $(shell nproc 2>/dev/null || echo 4)
CMAKE := $(shell if [ -x /usr/bin/cmake ]; then echo /usr/bin/cmake; else echo cmake; fi)
OCTI_DIR ?= $(abspath $(CURDIR)/../octi)

SANITIZE ?= 0
ifneq (,$(findstring -fsanitize,$(CFLAGS) $(CXXFLAGS)))
SANITIZE := 1
endif
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release
ifeq ($(SANITIZE),1)
BUILD_DIR := build/asan
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DOCTIBOT_SANITIZE=ON
endif

.PHONY: all clean distclean test sync-core-from-octi sync-core-to-octi play-agy

all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && (test -f Makefile || $(CMAKE) $(CURDIR) $(CMAKE_FLAGS))
ifeq ($(SANITIZE),1)
	@cd $(BUILD_DIR) && $(CMAKE) $(CURDIR) $(CMAKE_FLAGS) > /dev/null
endif
	@$(MAKE) -C $(BUILD_DIR) -j$(NUM_PROCS)

test: all
	@$(MAKE) -C $(BUILD_DIR) test_octibot_sdk -j$(NUM_PROCS)
	@./$(BUILD_DIR)/test_octibot_sdk

play-agy: all
	@mkdir -p -m 0700 $(HOME)/.gemini/antigravity-octibot /tmp/octibot-ai-sandbox
	@for f in antigravity-oauth-token settings.json installation_id; do \
		if [ ! -L "$(HOME)/.gemini/antigravity-octibot/$$f" ] && [ -e "$(HOME)/.gemini/antigravity-cli/$$f" ]; then \
			ln -s "../antigravity-cli/$$f" "$(HOME)/.gemini/antigravity-octibot/$$f"; \
		fi; \
	done
	./$(BUILD_DIR)/octibot --profile $(or $(PROFILE),stock) --ai-cli agy $(if $(MODEL),--ai-model $(MODEL)) $(if $(PROMPT),--ai-prompt "$(PROMPT)")

clean:
	@if [ -d $(BUILD_DIR) ] && [ -f $(BUILD_DIR)/Makefile ]; then \
		$(MAKE) -C $(BUILD_DIR) clean; \
	fi

distclean:
	@rm -rf build

sync-core-from-octi:
	@if [ ! -d "$(OCTI_DIR)/core" ]; then \
		echo "$(OCTI_DIR)/core does not exist"; exit 1; \
	fi
	@mkdir -p core
	rsync -a --delete --exclude='build' "$(OCTI_DIR)/core/" core/
	@echo "Synced core/ from $(OCTI_DIR)/core/"

sync-core-to-octi:
	@if [ ! -d "$(OCTI_DIR)" ]; then \
		echo "OCTI_DIR $(OCTI_DIR) does not exist"; exit 1; \
	fi
	@mkdir -p "$(OCTI_DIR)/core"
	rsync -a --delete --exclude='build' core/ "$(OCTI_DIR)/core/"
	@echo "Synced core/ to $(OCTI_DIR)/core/"
