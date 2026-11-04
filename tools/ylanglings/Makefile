PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/ylanglings
PYTHON ?= python3

.PHONY: install uninstall

install:
	install -d "$(BINDIR)" "$(DATADIR)"
	cp -R exercises manifest.json README.md ylanglings.py "$(DATADIR)/"
	printf '#!/bin/sh\nexec "%s" "%s/ylanglings.py" "$@"\n' "$(PYTHON)" "$(DATADIR)" > "$(BINDIR)/ylanglings"
	chmod 755 "$(BINDIR)/ylanglings"
	@echo "Installed ylanglings to $(BINDIR)/ylanglings"
	@echo "Make sure $(BINDIR) is in PATH and the ylang compiler is available."

uninstall:
	rm -f "$(BINDIR)/ylanglings"
	rm -rf "$(DATADIR)"
