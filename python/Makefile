CPPFLAGS = -std=c++20 -I. -I../include
CXXFLAGS := -Wall -O3 -flto -fmax-errors=3
# CXXFLAGS := -Wall -g -fmax-errors=3

# generate .d file during compilation
CPPFLAGS += -MT $@ -MMD -MP -MF $*.d

PYTHON_INCLUDE := $(shell \
  python3 -c 'import sysconfig; print(sysconfig.get_paths()["include"])')

histograms.o: CPPFLAGS += -DNDEBUG -isystem $(PYTHON_INCLUDE)
histograms.o: CXXFLAGS += -fPIC -fwrapv
histograms.so: LDFLAGS += -shared # -pthread

LIBNAME := histograms

.PHONY: all clean

all: $(LIBNAME).so

-include $(LIBNAME).d

%.so: %.o
	$(CXX) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@

clean:
	@rm -fv $(LIBNAME).so $(LIBNAME).o $(LIBNAME).d

