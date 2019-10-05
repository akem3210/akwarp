# $Id $
########################################

########################################
#
# programs and flags
# 
INSTALL?=install
TAR?=tar

DESTDIR?=
PREFIX?=/usr/local

CFLAGS?=-Os
CPPFLAGS?=
LDFLAGS?=-L/usr/X11R6/lib
LIBS?=-lX11

########################################
#
# project and default target
#
TARGET=akwarp
VERSION=1.0-rc1
DISTNAME=$(TARGET)-$(VERSION)
SHAREFILES=\
    LICENSE\
    README.txt\
    bottomleft.xbm bottomright.xbm topleft.xbm topright.xbm\
    $(TARGET).html
DISTFILES=\
	Makefile\
	$(TARGET).c\
    $(SHAREFILES)

$(TARGET): $(TARGET).o Makefile
	$(CC) -o $@ $< $(LDFLAGS) $(LIBS)

xrender :
	$(MAKE) LIBS="$(LIBS) -lXrender -lXext" CFLAGS="$(CFLAGS) -DHAVE_XRENDER"
xshape:
	$(MAKE) LIBS="$(LIBS) -lXext" CFLAGS="$(CFLAGS) -DHAVE_XSHAPE"
xrender-xshape:
	$(MAKE) LIBS="$(LIBS) -lXrender -lXext" CFLAGS="$(CFLAGS) -DHAVE_XRENDER -DHAVE_XSHAPE"

$(DISTNAME).tar.bz2 : $(DISTFILES)
	mkdir -p $(DISTNAME) &&\
	cp $(DISTFILES) $(DISTNAME) &&\
	tar cvfj $@ $(DISTNAME) &&\
	chmod 0644 $(DISTNAME).tar.bz2 &&\
	sha1sum $@

#########################################
# 
# docu
#

docs: $(TARGET).1 $(TARGET).html

$(TARGET).1 : $(TARGET).xml
	xmlto man $<

$(TARGET).pdf : $(TARGET).xml
	docbook2pdf $<

$(TARGET).html: README.txt
	asciidoc -a name=$(TARGET) -b xhtml11 -d manpage -o $@ $<

$(TARGET).xml: README.txt
	asciidoc -a name=$(TARGET) -b docbook -d manpage -o $@ $<

#########################################
#
# administrative tasks
# 
clean:
	rm -f *.o $(TARGET).xml

cleanall:
	rm -f $(TARGET) *.o $(TARGET).1 $(TARGET).html $(TARGET).pdf $(TARGET).xml

dist: $(DISTNAME).tar.bz2

distclean:
	rm -fr $(DISTNAME) $(DISTNAME).tar.bz2

install: $(TARGET) docs
	$(INSTALL) -D -s -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	$(INSTALL) -D -m 0644 $(TARGET).1 $(DESTDIR)$(PREFIX)/man/man1/$(TARGET).1
	for f in $(SHAREFILES); \
        do \
            $(INSTALL) -D -m 0644 $$f $(DESTDIR)$(PREFIX)/share/$(DISTNAME)/$$f; \
        done
