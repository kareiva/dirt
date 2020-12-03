# mingw32 makefile.

dirt-mingw:

CC = mingw32-gcc
CXX = mingw32-g++
EXE = .exe

include gnu.mk

CPPFLAGS += -D DIRT_WINDOWS
DIRTIRC_SRC += windows.cpp
DIRTIRC_OBJ += resource.o

dirt-mingw: $(DIRTIRC)

$(DIRTIRC): $(DIRTIRC_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ -mwindows -lcrypto -lws2_32

$(BIN2C): $(BIN2C_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

resource.o: resource.rc resource.h define.h res/*

%.o: %.rc
	windres -i $< -o $@ -O coff

include default.mak
