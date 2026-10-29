# Makefile - LL(1) Parser Generator
#
# Targets:
#   make          - build the ll1 executable
#   make run      - build and run the full demo pipeline on the standard
#                   grammar with demo input strings
#   make test     - run the integration test suite (testcases/run_tests.sh)
#   make clean    - remove build artifacts

CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -O2
CPPFLAGS:= -Iinclude

SRCDIR  := src
OBJDIR  := build
BINDIR  := .

SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
TARGET  := $(BINDIR)/ll1

# Additive JSON mode for the ParseLab web adapter (src/json_export.c is
# plain object code in every build; this target adds the ll1json entry
# point on top of the SAME engine objects).  The default ll1 build is
# unchanged.
JSON_SRC := tools/json_main.c
JSON_BLD := $(OBJDIR)/json_main.o
JSON_TGT := $(BINDIR)/ll1json
# ll1json links every engine object EXCEPT main.o (that file owns ll1's
# own main() entry point).
JSON_OBJS := $(filter-out $(OBJDIR)/main.o,$(OBJS))

.PHONY: all run test clean ll1json web start

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(JSON_TGT): $(JSON_OBJS) $(JSON_BLD)
	$(CC) $(CFLAGS) -o $@ $(JSON_OBJS) $(JSON_BLD)

$(JSON_BLD): $(JSON_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

ll1json: $(JSON_TGT)

web: ll1json
	python web/server.py

start: web

run: $(TARGET)
	./$(TARGET) testcases/valid/expression_valid.txt < testcases/inputs/demo_inputs.txt

test: $(TARGET)
	@sh testcases/run_tests.sh

clean:
	rm -rf $(OBJDIR) $(TARGET)
