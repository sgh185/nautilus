# 
#  Copyright (c) 2017 Peter Dinda 
#
# Framework code for building "user" executables for Nautilus
#

NAUTILUS_DIR = ../..


TARGET = hello.exe

USER_OBJ = hello.o

LD = ld

# Flags must be compatible with the NK kernel flags
# CFLAGS = -O2 \
# 	 -fno-omit-frame-pointer \
# 	 -ffreestanding \
# 	 -fno-stack-protector \
# 	 -fno-strict-aliasing \
#          -fno-strict-overflow \
# 	 -fno-delete-null-pointer-checks \
#          -fno-exceptions \
# 	 -mno-red-zone \
# 	 -mcmodel=large \
#          -fno-common \
#          -Wstrict-overflow=5

CLAGS = -O2 -mno-red-zone -I../../include

# For C++ add
#         -fno-rtti \

# Add these for more recent compilers to avoid having
# the compiler insert surprise ud2 instructions for you should
# you derefence address zero
#
#                           -fno-isolate-erroneous-paths-attribute \
#                           -fno-isolate-erroneous-paths-dereference \
#
#

CC = gcc -g3 -static-pie -mno-red-zone -I../../include #-static-libgcc -fPIC -Wall -I$(NAUTILUS_DIR)/include -DNAUTILUS_EXE
# CC = /home/aaron/tmp/gcc/install/usr/local/bin/gcc -g3 -static -static-libgcc -fPIC -Wall -I$(NAUTILUS_DIR)/include -DNAUTILUS_EXE
# CC = musl-gcc -g3 -static -fPIC -Wall -I$(NAUTILUS_DIR)/include -DNAUTILUS_EXE

all: $(TARGET)

$(TARGET:.exe=.o) : $(TARGET:.exe=.c) 
	$(CC) $(CFLAGS) -c $(TARGET:.exe=.c)

framework_low_level.o: framework_low_level.S
	$(CC) $(CFLAGS) -c framework_low_level.S

framework.o : framework.c
	$(CC) $(CFLAGS) -c framework.c

glue.o : glue.c
	$(CC) $(CFLAGS) -c glue.c

$(TARGET) : $(TARGET:.exe=.o) framework.o framework_low_level.o glue.o framework.lds
	./nld $(TARGET:.exe=.o) -o $(TARGET)


clean: 
	rm -f $(TARGET) *.o

