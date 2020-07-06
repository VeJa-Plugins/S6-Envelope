EFFECTS_DIR = \
m8-env

ifeq ($(MOD),1)
EFFECTS_DIR += HardwareBypass
endif

INSTALL_PATH = /usr/local/lib/lv2

all:
		@for fx in $(EFFECTS_DIR); do \
		cd $$fx; \
		$(MAKE); \
		cd ..; \
		done

install:
		@for fx in $(EFFECTS_DIR); do \
		cd $$fx; \
		$(MAKE) install INSTALL_PATH=$(INSTALL_PATH); \
		cd ..; \
		done

clean:
		@for fx in $(EFFECTS_DIR); do \
		cd $$fx; \
		$(MAKE) clean; \
		cd ..; \
		done
