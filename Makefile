# DamisService.cgi - DAMIS SOAP web service (gSOAP, runs as an Apache CGI).
#
#   make                 build ./DamisService.cgi
#   make -j"$(nproc)"    parallel build
#   make clean           remove build output
#   make install PREFIX=/usr/lib/cgi-bin   install the CGI binary to $(PREFIX)
#
# Requires (Ubuntu 26.04 / Debian 13):
#   apt-get install build-essential gsoap libgsoap-dev libcurl4-openssl-dev
#
# The gSOAP stubs are generated from wsdl/damis.wsdl (wsdl2h + soapcpp2) into build/gen and are
# not committed: they must match the installed gSOAP library version.

CXX      ?= g++
CXXSTD   ?= -std=c++17
CXXFLAGS ?= -O2 -Wall -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
CPPFLAGS += -I. -I$(GENDIR)
LDFLAGS  ?=
LDLIBS   += -lgsoap++ -lcurl

WSDL2H       ?= wsdl2h
SOAPCPP2     ?= soapcpp2
GSOAP_SHARE  ?= /usr/share/gsoap

TARGET   := DamisService.cgi
BUILDDIR := build
GENDIR   := $(BUILDDIR)/gen
WSDL     := wsdl/damis.wsdl
PREFIX   ?= /usr/lib/cgi-bin

SOURCES     := $(wildcard *.cpp)
GEN_SOURCES := $(GENDIR)/soapC.cpp $(GENDIR)/soapDAMISService.cpp
OBJECTS     := $(SOURCES:%.cpp=$(BUILDDIR)/%.o) $(GEN_SOURCES:$(GENDIR)/%.cpp=$(BUILDDIR)/gen-%.o)
DEPS        := $(OBJECTS:.o=.d)
GEN_STAMP   := $(GENDIR)/.generated

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Same options as the original 2014 generation (gsoap/gen headers/1.bat, 2.bat):
# wsdl2h:   -n/-N prefixes (Damis__PCAResponse ...), -_ no underscores in names, -w always wrap responses
# soapcpp2: -S server only, -L no lib files, -j C++ service class sharing a soap context,
#           -b serialize char[N] arrays as strings, -e SOAP RPC encoding, -w -x no WSDL/XML sample files
$(GEN_STAMP): $(WSDL)
	@mkdir -p $(GENDIR)
	$(WSDL2H) -I$(GSOAP_SHARE)/WS -n DAMIS -N Damis -_ -w -o $(GENDIR)/DAMIS.h $<
	$(SOAPCPP2) -S -L -j -b -e -I$(GSOAP_SHARE)/import -w -x -d $(GENDIR) $(GENDIR)/DAMIS.h
	@touch $@

$(GEN_SOURCES): $(GEN_STAMP)

$(BUILDDIR)/%.o: %.cpp | $(GEN_STAMP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/gen-%.o: $(GENDIR)/%.cpp
	$(CXX) $(CXXSTD) $(CPPFLAGS) $(CXXFLAGS) -w -MMD -MP -c $< -o $@

install: $(TARGET)
	install -D -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/$(TARGET)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(DEPS)
