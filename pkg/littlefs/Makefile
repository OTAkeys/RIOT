PKG_NAME=littlefs
PKG_URL=https://github.com/geky/littlefs.git
# v2.0.0
PKG_VERSION=0907ba7813173901bc9a7689e8781434629a3696
PKG_LICENSE=BSD-3-Clause

.PHONY: all

all: git-download
	"$(MAKE)" -C $(PKG_BUILDDIR) -f $(CURDIR)/Makefile.littlefs

include $(RIOTBASE)/pkg/pkg.mk
