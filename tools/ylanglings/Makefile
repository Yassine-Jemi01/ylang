PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/ylanglings
PYTHON ?= python3

escape_sed = $(subst \,\\,$(subst &,\&,$(subst |,\|,$(1))))

.PHONY: install uninstall

install:
	install -d "$(BINDIR)" "$(DATADIR)"
	cp -R exercises manifest.json README.md ylanglings.py "$(DATADIR)/"
	sed -e 's|@PYTHON@|$(call escape_sed,$(PYTHON))|g' -e 's|@DATADIR@|$(call escape_sed,$(DATADIR))|g' launcher.sh.in > "$(BINDIR)/ylanglings"
	chmod 755 "$(BINDIR)/ylanglings"
	@echo "Installed ylanglings to $(BINDIR)/ylanglings"
	@echo "Make sure $(BINDIR) is in PATH and the ylang compiler is available."

uninstall:
	rm -f "$(BINDIR)/ylanglings"
	rm -rf "$(DATADIR)"
