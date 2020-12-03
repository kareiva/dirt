# Unix (GNU) makefile.
# This compiles for Cygwin as well.
# On FreeBSD, use "gmake -f unix.mak".

dirt-unix:

CC = gcc
CXX = g++
EXE =

include gnu.mk

CPPFLAGS += -D DIRT_UNIX
DIRTIRC_SRC += unix.cpp

dirt-unix: $(DIRTIRC)

$(DIRTIRC): $(DIRTIRC_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ -lcrypto

$(BIN2C): $(BIN2C_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

include default.mak
