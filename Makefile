#------------------------------------------------------------------#
# Usage: make install INSTALL_DIR=XXX
#------------------------------------------------------------------#

# Ian Mason's Makefile -- point to the llvm-config of the version you
# want to build against (using the LLVM_CONFIG environment variable)
# but set the default if there is no environment version there...

ifeq ($(LLVM_CONFIG),)
	LLVM_CONFIG=llvm-config
endif

# BD: made all things dependent on LLVM_HOME

LLVM_CFG = $(LLVM_HOME)/bin/$(LLVM_CONFIG)

LLVM_INCLUDE = $(shell ${LLVM_CFG} --includedir)

CXX_FLAGS += $(shell  ${LLVM_CFG} --cxxflags) -I${LLVM_INCLUDE} -I../include -I/usr/local/include  -fPIC

CXX_FLAGS += -I./include

C_FLAGS = $(shell  ${LLVM_CFG} --cflags)

CPP_FLAGS += $(shell  ${LLVM_CFG} --cppflags) -I${LLVM_INCLUDE} -I../include -I/usr/local/include

OS   =  $(shell uname)

## Default install directory
INSTALL_DIR=.

LIBRARYNAME=libSeaDsa

ifeq (Darwin, $(findstring Darwin, ${OS}))
#  DARWIN
LIBRARY = ${LIBRARYNAME}.dylib
LIBFLAGS = -dynamiclib
LD_FLAGS += -undefined suppress -flat_namespace
else ifeq (FreeBSD, $(findstring FreeBSD, ${OS}))
# FreeBSD
LIBRARY = ${LIBRARYNAME}.so
LIBFLAGS = -shared -Wl,-soname,${LIB}
else
# LINUX
LIBRARY = ${LIBRARYNAME}.so
LIBFLAGS = -shared -Wl,-soname,${LIB}
endif

SOURCES = $(wildcard src/*.cc)

OBJECTS := $(patsubst %.cc,%.o,${SOURCES}) 

INSTALL = install

all: ${LIBRARY}

${LIBRARY}: ${SOURCES}
	@echo "The sources are being built according to ${LLVM_CFG}"
	$(MAKE) lib

lib: ${OBJECTS} 
	$(CXX) ${OBJECTS} ${LIBFLAGS} -o ${LIBRARY} ${CXX_FLAGS} ${LD_FLAGS} 

src/%.o: src/%.cc
	$(CXX) ${CXX_FLAGS} $< -c -o $@

%.o: %.cc
	$(CXX) -I. ${CXX_FLAGS} $< -c 

clean: 
	rm -rf ${OBJECTS} ${LIBRARY} 

install: ${LIBRARY}
	mkdir -p $(INSTALL_DIR)
	$(INSTALL) -m 664 ${LIBRARY} $(INSTALL_DIR)

# XX: a bit dangerous
# uninstall:
# 	rm -Rf $(INSTALL_DIR)

