PKG_NAME=spiffs
PKG_URL=https://github.com/pellepl/spiffs.git
PKG_VERSION=bdcf008e755ca70c15aa0b49821631aaea353565
PKG_BUILDDIR ?= $(BINDIRBASE)/pkg/$(BOARD)/$(PKG_NAME)

CFLAGS += -std=c11

.PHONY: all

all: git-download
	@mkdir -p "$(PKG_BUILDDIR)/riotbuild"
	@cp $(PKG_BUILDDIR)/src/*.c $(PKG_BUILDDIR)/src/*.h $(PKG_BUILDDIR)/riotbuild

	@echo 'MODULE:=spiffs' > $(PKG_BUILDDIR)/riotbuild/Makefile
	@echo 'include $$(RIOTBASE)/Makefile.base' >> $(PKG_BUILDDIR)/riotbuild/Makefile
	
	"$(MAKE)" -C $(PKG_BUILDDIR)/riotbuild

include $(RIOTBASE)/pkg/pkg.mk
