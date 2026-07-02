# Suricata Validation Engine — build system (see docs/ARCHITECTURE.md §7)
#
# All generated artifacts live under build/ (gitignored, never committed):
#   build/gen/         bison + flex output
#   build/obj/<mode>/  object files, one tree per build mode
#   build/bin/         final binary
#
# Modes:
#   make               debug build (default): -g -O0 + UBSan — sanitizers keep
#                      the token/rule ownership contracts honest
#   make release       optimized build
#   make test          debug build + golden-file suite
#   make clean         removes build/ only
#
# Sanitizer selection: SAN=undefined (default), SAN=address,undefined, SAN=
# (none). ASan binaries hang at startup on some macOS/Xcode combinations
# (shadow-memory init spin — verified on this project's dev machine), so it
# is opt-in; the test runner uses `leaks --atExit` on macOS for heap checking
# instead.

CC    ?= cc
FLEX  ?= flex
# Prefer Homebrew bison: macOS system bison is 2.3 (2006) and lacks
# %define api.location.type now and %define parse.error custom (Phase 4).
BISON ?= $(firstword $(wildcard /opt/homebrew/opt/bison/bin/bison \
                                /usr/local/opt/bison/bin/bison) bison)

MODE ?= debug

BUILD := build
GEN   := $(BUILD)/gen
OBJ   := $(BUILD)/obj/$(MODE)
BIN   := $(BUILD)/bin

TARGET := $(BIN)/suricata-validate

CFLAGS_COMMON := -std=c11 -Wall -Wextra -Isrc -I$(GEN)

SAN ?= undefined
ifeq ($(strip $(SAN)),)
  SANFLAGS :=
else
  SANFLAGS := -fsanitize=$(SAN)
endif

ifeq ($(MODE),debug)
  # YYDEBUG compiles in bison's shift/reduce/recovery trace; enable it at
  # runtime with SV_PARSER_TRACE=1 (recovery debugging — see core/validate.c).
  CFLAGS  := $(CFLAGS_COMMON) -g -O0 -DYYDEBUG=1 $(SANFLAGS)
  LDFLAGS := $(SANFLAGS)
else ifeq ($(MODE),release)
  CFLAGS  := $(CFLAGS_COMMON) -O2 -DNDEBUG
  LDFLAGS :=
else
  $(error MODE must be 'debug' or 'release', got '$(MODE)')
endif

# Generator output is compiled with a relaxed warning set — we do not patch
# generated code to silence its warnings.
GEN_CFLAGS := $(CFLAGS) -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare

SRCS     := $(shell find src -name '*.c' | sort)
OBJS     := $(SRCS:src/%.c=$(OBJ)/%.o)
GEN_OBJS := $(OBJ)/gen/parser.tab.o $(OBJ)/gen/lex.yy.o

.PHONY: all release test clean

all: $(TARGET)

release:
	$(MAKE) MODE=release

# Bison first: -d emits the token header the scanner includes.
# -Wcounterexamples makes grammar conflicts debuggable; the zero-conflict
# policy (ARCHITECTURE.md §7) is enforced by treating warnings as errors.
$(GEN)/parser.tab.c $(GEN)/parser.tab.h: src/parser/grammar.y
	@mkdir -p $(GEN)
	$(BISON) -d -Wall -Wcounterexamples -Werror -o $(GEN)/parser.tab.c $<

$(GEN)/lex.yy.c: src/lexer/rules.l $(GEN)/parser.tab.h
	@mkdir -p $(GEN)
	$(FLEX) -o $@ $<

$(OBJ)/gen/%.o: $(GEN)/%.c $(GEN)/parser.tab.h
	@mkdir -p $(dir $@)
	$(CC) $(GEN_CFLAGS) -c -o $@ $<

$(OBJ)/%.o: src/%.c $(GEN)/parser.tab.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS) $(GEN_OBJS)
	@mkdir -p $(BIN)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(GEN_OBJS)

test: all
	bash tests/run_tests.sh

clean:
	rm -rf $(BUILD)
