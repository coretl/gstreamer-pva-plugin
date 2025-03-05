all: gstpvasource.so

PKGCONFIG = PKG_CONFIG_PATH=/dls_sw/work/tools/RHEL6-x86_64/gstreamer/prefix/lib/pkgconfig:/dls_sw/work/tools/RHEL6-x86_64/glib/prefix/lib/pkgconfig pkg-config
EPICS_BASE = /dls_sw/epics/R3.14.12.3/base
EPICSV4_BASE = /dls_sw/work/epics/R3.14.12.3/v4/20140926
PVCOMMON = $(EPICSV4_BASE)/pvCommonCPP
PVDATA = $(EPICSV4_BASE)/pvDataCPP
PVACCESS = $(EPICSV4_BASE)/pvAccessCPP
PVDATABASE = $(EPICSV4_BASE)/pvDatabaseCPP

%.so: %.o
	g++ -shared -o $@ $< $(shell $(PKGCONFIG) --libs gstreamer-1.0 gstreamer-base-1.0) -L$(PVACCESS)/lib/linux-x86_64 -Wl,-rpath,$(PVACCESS)/lib/linux-x86_64 -lpvAccess -L$(PVDATA)/lib/linux-x86_64 -Wl,-rpath,$(PVDATA)/lib/linux-x86_64 -lpvData -L$(PVDATABASE)/lib/linux-x86_64 -Wl,-rpath,$(PVDATABASE)/lib/linux-x86_64 -lpvDatabase 
	
	#-lpvData  -lpvDatabase


%.o: %.cpp %.h
	g++ -Wall -Werror -fPIC $(shell $(PKGCONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0) -g -I $(EPICS_BASE)/include -I $(EPICS_BASE)/include/os/Linux -I $(PVACCESS)/include -I $(PVDATA)/include -c -o  $@ $<

clean:
	rm -f gstpvasource.so gstpvasource.o
