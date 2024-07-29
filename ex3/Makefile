CC=g++
CXX=g++
RANLIB=ranlib

LIBSRC=Barrier.cpp Mutex.cpp CSemaphore.cpp Thread.cpp Job.cpp MapReduceFramework.cpp
LIBHDR=Barrier.h Mutex.h CSemaphore.h Thread.h Job.h Common.h
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -std=c++11 -pthread -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -pthread -g $(INCS)

FRAMEWORKLIB = libMapReduceFramework.a
TARGETS = $(FRAMEWORKLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex3.tar
TARSRCS=$(LIBSRC) $(LIBHDR) Makefile README

all: $(TARGETS)

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(FRAMEWORKLIB) $(OBJ) $(LIBOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
