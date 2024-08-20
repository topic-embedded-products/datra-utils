ACLOCAL_AMFLAGS = -I m4
AM_DEFAULT_SOURCE_EXT = .cpp
AM_CPPFLAGS = $(DATRA_CFLAGS)
AM_LDFLAGS = $(DATRA_LIBS)

bin_PROGRAMS = datraprogrammer datraroute datraaxiprobe datraproxy datralicense

datraaxiprobe_LDADD = -lrt
datraaxiprobe_SOURCES = datraaxiprobe.cpp benchmark.hpp
