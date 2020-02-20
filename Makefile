SHELL := bash
CXXSTD := -std=c++17
CPPFLAGS := -Iinclude
CXXFLAGS := -Wall -O3 -flto -fmax-errors=3
# CXXFLAGS := -Wall -g -fmax-errors=3

BIN := bin
BLD := .build

.PHONY: all clean

ifeq (0, $(words $(findstring clean, $(MAKECMDGOALS))))

SRCS := $(shell find -L src -type f -regex '.*\.c[^.]*')

EXES_CMD := \
  grep -l '^ *int \+main *(' $(SRCS) | sed 's:src/\(.*\)\.c.*$$:$(BIN)/\1:'
EXES := $(shell $(EXES_CMD))

DEPS := $(shell echo '$(SRCS)' | sed 's:src/\(.*\)\.c.*$$:$(BLD)/\1.d:g')

all: $(EXES)

-include $(DEPS)

$(BLD)/%.d: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CPPFLAGS) -MM -MT '$@ $(@:.d=.o)' $< -MF $@
	@\
  if [ ! -z '$(findstring $(BIN)/$*, $(EXES))' ]; then \
    objs=$$(cat $@ \
    | awk \
      '{ for (i=1; i<=NF; i++) { \
           if (match($$i,/^include\/(.+)\.h[hpx]*\\?/,m)) { print m[1]; } \
       } }' \
    | while read f; do [ -f "src/$$f.cc" ] && echo "$(BLD)/$$f.o"; done); \
    if [ ! -z "$$objs" ]; then echo "$(BIN)/$*:" $$objs >> $@; fi \
  fi

$(BLD)/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CXXSTD) $(CPPFLAGS) $(CXXFLAGS) -c $(filter %.cc,$^) -o $@

$(BIN)/%: $(BLD)/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(filter %.o,$^) -o $@ $(LDLIBS)

endif

clean:
	@rm -rfv $(BLD) $(BIN)

