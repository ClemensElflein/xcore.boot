# List of all the board related files.
BOARDSRC = ./boards/ST_NUCLEO144_H723ZG/board.c

# Required include directories
BOARDINC = ./boards/ST_NUCLEO144_H723ZG

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
