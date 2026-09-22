# Путь установки по умолчанию (можно переопределить: make PREFIX=/usr)
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: install uninstall

install:
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 bin/sshf $(DESTDIR)$(BINDIR)/sshf
	@echo "sshf successfully installed in $(DESTDIR)$(BINDIR)"

uninstall:
	@rm -f $(DESTDIR)$(BINDIR)/sshf
	@echo "sshf successfully removed from $(DESTDIR)$(BINDIR)"