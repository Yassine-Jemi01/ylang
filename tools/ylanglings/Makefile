PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/ylanglings
PYTHON ?= python3

INSTALL_MARKER = .ylanglings-install-marker
INSTALL_MARKER_VALUE = ylanglings-managed-data-v1

escape_sed = $(subst \,\\,$(subst &,\&,$(subst |,\|,$(1))))

.PHONY: install uninstall

install:
	@case "$(DATADIR)" in ylanglings|*/ylanglings) ;; *) echo "Refusing unexpected YLanglings data path: $(DATADIR)" >&2; exit 1 ;; esac
	@if [ -L "$(DATADIR)" ]; then echo "Refusing symlinked YLanglings data directory: $(DATADIR)" >&2; exit 1; fi
	@if [ -e "$(DATADIR)" ]; then \
		if [ ! -f "$(DATADIR)/$(INSTALL_MARKER)" ] || [ -L "$(DATADIR)/$(INSTALL_MARKER)" ] || \
		   [ "$$(cat "$(DATADIR)/$(INSTALL_MARKER)" 2>/dev/null)" != "$(INSTALL_MARKER_VALUE)" ]; then \
			echo "Refusing to overwrite an unmanaged YLanglings data directory: $(DATADIR)" >&2; exit 1; \
		fi; \
	else \
		install -d "$(DATADIR)"; \
		printf '%s\n' "$(INSTALL_MARKER_VALUE)" > "$(DATADIR)/$(INSTALL_MARKER)"; \
	fi
	install -d "$(BINDIR)"
	cp -R exercises manifest.json README.md ylanglings.py "$(DATADIR)/"
	sed -e 's|@PYTHON@|$(call escape_sed,$(PYTHON))|g' -e 's|@DATADIR@|$(call escape_sed,$(DATADIR))|g' launcher.sh.in > "$(BINDIR)/ylanglings"
	chmod 755 "$(BINDIR)/ylanglings"
	@echo "Installed ylanglings to $(BINDIR)/ylanglings"
	@echo "Make sure $(BINDIR) is in PATH and the ylang compiler is available."

uninstall:
	@case "$(DATADIR)" in ylanglings|*/ylanglings) ;; *) echo "Refusing to remove unexpected YLanglings data path: $(DATADIR)" >&2; exit 1 ;; esac
	@if [ -L "$(DATADIR)" ] || [ ! -f "$(DATADIR)/$(INSTALL_MARKER)" ] || [ -L "$(DATADIR)/$(INSTALL_MARKER)" ] || \
	   [ "$$(cat "$(DATADIR)/$(INSTALL_MARKER)" 2>/dev/null)" != "$(INSTALL_MARKER_VALUE)" ]; then \
		echo "Refusing to remove unmanaged YLanglings data directory: $(DATADIR)" >&2; exit 1; \
	fi
	rm -f "$(BINDIR)/ylanglings"
	rm -rf "$(DATADIR)"
