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

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

run: $(TARGET)
	./$(TARGET) testcases/valid/expression_valid.txt < testcases/inputs/demo_inputs.txt

test: $(TARGET)
	@sh testcases/run_tests.sh

clean:
	rm -rf $(OBJDIR) $(TARGET)
