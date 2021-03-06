EXTRA_DIST += \
	src/config.ent.in \
	src/common-man.xml \
	$(man1_MANS:.1=.xml) \
	$(man5_MANS:.5=.xml) \
	$(null)

CLEANFILES += \
	src/config.ent \
	$(man1_MANS) \
	$(man5_MANS) \
	$(null)

MAN_GEN_RULE = $(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(XSLTPROC) \
	-o $(dir $@) \
	$(AM_XSLTPROCFLAGS) \
	--stringparam profile.condition '${AM_DOCBOOK_CONDITIONS}' \
	$(XSLTPROCFLAGS) \
	http://docbook.sourceforge.net/release/xsl/current/manpages/profile-docbook.xsl \
	$<

$(man1_MANS): %.1: %.xml $(NKUTILS_MANFILES) src/common-man.xml src/config.ent
	$(MAN_GEN_RULE)

$(man5_MANS): %.5: %.xml $(NKUTILS_MANFILES) src/common-man.xml src/config.ent
	$(MAN_GEN_RULE)

src/config.ent: src/config.ent.in src/config.h
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]sysconfdir[@]:$(sysconfdir):g' \
		-e 's:[@]libdir[@]:$(libdir):g' \
		-e 's:[@]datadir[@]:$(datadir):g' \
		-e 's:[@]PACKAGE_NAME[@]:$(PACKAGE_NAME):g' \
		-e 's:[@]PACKAGE_VERSION[@]:$(PACKAGE_VERSION):g' \
		-e 's:[@]DEFAULT_BIND_PORT[@]:$(DEFAULT_BIND_PORT):g' \
		-e 's:[@]EVP_UNIX_SOCKET[@]:$(EVP_UNIX_SOCKET):g' \
		< $< > $@ || rm $@
