# CS360 - Assignment #10 - Dick Lang
#
# Makefile
# CC = compiler; CFLAGS = compiler flags; OBJECTS = object files
# TARGET = output file (executable); CLIB = extra libraries

CC = gcc
CFLAGS = -Wall -pedantic -g -ansi -std=gnu99
OPT = -O2
OBJECTS1 = mftp.o
OBJECTS2 = mftpserve.o
HEADER = mftp.h
TARGET1 = bin/mftp
TARGET2 = bin/mftpserve

.SUFFIXES: .o .h

all: prebuild1 main1 postbuild1 prebuild2 main2 postbuild2

prebuild1:
	@echo "---\tPrebuild for $(TARGET1) makefile."
	@mkdir -p bin

postbuild1:
	@echo "---\tPostbuild for $(TARGET1) makefile."

main1 : $(OBJECTS1) $(HEADER)
	$(CC) $(CFLAGS) $(OPT) $(OBJECTS1) -o $(TARGET1)

prebuild2:
	@echo "---\tPrebuild for $(TARGET2) makefile."

postbuild2:
	@echo "---\tPostbuild for $(TARGET2) makefile."

main2 : $(OBJECTS2) $(HEADER)
	$(CC) $(CFLAGS) $(OPT) $(OBJECTS2) -o $(TARGET2)

# makes .o files for each .c file
%.o : %.c
	$(CC) $(CFLAGS) $(OPT) -c -o $@ $< $(CLIBS)

# makes .o files for each .h file
%.o : %.h
	$(CC) $(CFLAGS) $(OPT) -c -o $@ $< $(CLIBS)

# clean removes the executable and the .obj folder.
# .PHONY prevents clean from not running if the file clean exists
# (consider clean to be a reserved filename)
.PHONY : clean
clean :
	@echo
	@$(RM) $(TARGET1) $(TARGET2)
	@$(RM) $(OBJECTS1) $(OBJECTS2)
