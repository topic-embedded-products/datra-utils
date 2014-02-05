ACLOCAL_AMFLAGS = -I m4
AM_DEFAULT_SOURCE_EXT = .cpp
AM_CPPFLAGS = $(DYPLO_CFLAGS)
AM_LDFLAGS = $(DYPLO_LIBS)

bin_PROGRAMS = dyploprogrammer dyploroute dyploaxiprobe dyploproxy

dyploaxiprobe_LDADD = -lrt
dyploaxiprobe_SOURCES = dyploaxiprobe.cpp benchmark.hpp
