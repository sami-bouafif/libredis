h_sources = bstr.h redis.h
c_sources = bstr.c redis.c

include_HEADERS = $(h_sources)

lib_LTLIBRARIES= libredis.la
libredis_la_SOURCES= $(h_sources) $(c_sources)
libredis_la_LDFLAGS= -version-info $(GENERIC_LIBRARY_VERSION) -release $(GENERIC_RELEASE)
