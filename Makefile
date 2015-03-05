VERSION := 7.1.6

CC := gcc
CFLAGS := -Wall -O

override CPPFLAGS += -DUDP_FLIP -DSSL_AUTH
override LDFLAGS += -ltropicssl

LUAC := luac
LUACFLAGS := -s

PREFIX := /usr/local

# Cygwin
#CFLAGS += -m32 -march=i486
#EXEEXT := .exe

MANIFEST := nattcp$(EXEEXT) udp-climber
DIST := Makefile nattcp.c tropicssl.c udp-climber.lua nuttcp.8 LICENSE \
	xinetd.d/nattcp xinetd.d/nattcp4 xinetd.d/nattcp6 \
	upstart/nattcp.conf

all : $(MANIFEST)

nattcp$(EXEEXT) : nattcp.o tropicssl.o
	$(CC) -o $@ $^ $(LDFLAGS)

udp-climber : udp-climber.lua
	echo "#!/usr/bin/lua" >$@
	$(LUAC) $(LUACFLAGS) -o - $< >>$@
	chmod +x $@

install : $(MANIFEST)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 nattcp $(DESTDIR)$(PREFIX)/bin/
	install -m 0755 udp-climber $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f *.o $(MANIFEST)

# Win32 binary release
release : $(MANIFEST) cyggcc_s-1.dll cygwin1.dll lua.exe lua5.1.dll
	rm -f nattcp-$(VERSION)-win32.zip
	zip nattcp-$(VERSION)-win32.zip $^

# automake-style source distro
dist : $(DIST)
	tar czf nattcp-$(VERSION).tar.gz --xform "s,^,nattcp-$(VERSION)/,S" $^
