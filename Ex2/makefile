CC=g++
CXX=g++

CODESRC= thread.cpp uthreads.cpp
CODEHDR= thread.h
CODEOBJ= thread.o uthreads.o
EXESRC= $(CODESRC)
LIBOBJ= libuthreads.a

INCS=-I.
CFLAGS = -Wall -std=c++11 $(INCS) -c
CXXFLAGS = -Wall -std=c++11 $(INCS) -c

TARGETS = $(LIBOBJ)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex2.tar
TARSRCS=$(CODESRC) $(CODEHDR) Makefile README

AR = ar
ARFLAGS = rvs

all: $(TARGETS)

$(TARGETS): $(EXESRC)
	$(CXX) $(CXXFLAGS) $^
	$(AR) $(ARFLAGS) $@ $(CODEOBJ)

clean:
	$(RM) $(TARGETS)

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(EXESRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
