AUTOMAKE_OPTIONS = check-news

bin_PROGRAMS = postf

postf_SOURCES = postf.c mnifile.c conversions.c

noinst_HEADERS = glhacks.h global_variables.h image.h mnifile.h conversions.h

dist_man1_MANS = postf.man1

EXTRA_DIST = $(m4_files) \
	epm-header.in

m4_files = m4/mni_REQUIRE_LIB.m4 \
	m4/mni_REQUIRE_MNILIBS.m4 \
	m4/smr_WITH_BUILD_PATH.m4 \
	m4/smr_REQUIRED_LIB.m4 \
	m4/mni_REQUIRE_OPENINVENTOR.m4

