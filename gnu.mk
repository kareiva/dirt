# This file is being included by any makefile that's using the GNU compiler
# collection.

MAKEDEPEND	= $(CXX) -MM $(CPPFLAGS) -o $*.d $<
CFLAGS		= -pipe -O3 -pedantic -Wall -Wno-long-long -std=c99
CXXFLAGS	= -pipe -O3 -pedantic -Wall -Wno-long-long -Wno-uninitialized
LDFLAGS		= -s

DIRTIRC		= dirtirc$(EXE)
BIN2C		= bin2c$(EXE)
DIRTIRC_SRC	= cfg.cpp dh1080.cpp fish.cpp ini.cpp key.cpp main.cpp \
		  misc.cpp net.cpp proxy.cpp syntax.cpp
DIRTIRC_OBJ	= inc.o $(DIRTIRC_SRC:.cpp=.o)
BIN2C_SRC	= bin2c.cpp misc.cpp
BIN2C_OBJ	= $(BIN2C_SRC:.cpp=.o)

perkele:
	@echo 'You should not use this makefile (gnu.mk) directly.'

$(BIN2C): $(BIN2C_OBJ)

inc.o: inc.cpp inc.h

inc.cpp inc.h: $(BIN2C) inc/*
	./$(BIN2C) -i inc/ax.asc -c inc.cpp -h inc.h

%$(EXE): %.cpp
	$(LINK.cpp) -o $@ $<

%.o: %.cpp
	@$(MAKEDEPEND); \
		cp $*.d $*.dp; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.dp; \
		rm -f $*.d
	$(COMPILE.cpp) -o $@ $<

-include $(DIRTIRC_SRC:.cpp=.dp)
-include $(BIN2C_SRC:.cpp=.dp)
