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
CFLAGS = -O2 \
	 -fno-omit-frame-pointer \
	 -ffreestanding \
	 -fno-stack-protector \
	 -fno-strict-aliasing \
         -fno-strict-overflow \
	 -fno-delete-null-pointer-checks \
         -fno-exceptions \
	 -mno-red-zone \
	 -mcmodel=large \
         -fno-common \
         -Wstrict-overflow=5

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

CC = gcc -g3 -static-pie -static-libgcc -fPIC -Wall -I$(NAUTILUS_DIR)/include -DNAUTILUS_EXE
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
	# ld -T framework.lds -L/usr/lib/x86_64-linux-gnu/ -static -dp -z max-page-size=0x1000 $(TARGET:.exe=.o) framework.o framework_low_level.o -lc glue.o -lcrt -o $(TARGET) 

### GCC
	ld -static -pie --no-dynamic-linker \
-T framework.lds -dp -z max-page-size=0x1000 \
-z relro \
-z now \
/usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu/rcrt1.o \
/usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu/crti.o \
/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o \
-L/usr/lib/gcc/x86_64-linux-gnu/9 -L/usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu \
-L/usr/lib/gcc/x86_64-linux-gnu/9/../../../../lib -L/lib/x86_64-linux-gnu -L/lib/../lib \
-L/usr/lib/x86_64-linux-gnu \
-L/usr/lib/../lib \
-L/usr/lib/gcc/x86_64-linux-gnu/9/../../.. \
hello.o glue.o framework.o framework_low_level.o  \
--start-group -lgcc  -lc -lrt --end-group \
/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o \
/usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu/crtn.o \
-o $(TARGET)

### GCC compiled without indirect functions
# 	ld -static \
# -T framework.lds -dp -z max-page-size=0x1000 \
# -z relro \
# -z now \
# /home/aaron/tmp/gcc/install/usr/local/lib/gcc/x86_64-pc-linux-gnu/11.0.0/Scrt1.o \
# /home/aaron/tmp/gcc/install/usr/local/lib/gcc/x86_64-pc-linux-gnu/11.0.0/crti.o \
# /home/aaron/tmp/gcc/install/usr/local/lib/gcc/x86_64-pc-linux-gnu/11.0.0/crtbeginT.o \
# -L/home/aaron/tmp/gcc/install/usr/local/lib/gcc/x86_64-pc-linux-gnu/11.0.0/ \
# -L./lib
# hello.o glue.o framework.o framework_low_level.o  \
# --start-group -lgcc  -lc --end-group \
# /home/aaron/tmp/gcc/install/usr/local/lib/gcc/x86_64-pc-linux-gnu/11.0.0/crtend.o \
# /home/aaron/tmp/gcc/install/usr/local/lib/gcc/x86_64-pc-linux-gnu/11.0.0/crtn.o \
# -o $(TARGET)

### GCC NO MULTIBOOT
# 	ld -static \
# -dp -z max-page-size=0x1000 \
# -z relro \
# -z now \
# /usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu/crt1.o \
# /usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu/crti.o \
# /usr/lib/gcc/x86_64-linux-gnu/9/crtbeginT.o \
# -L/usr/lib/gcc/x86_64-linux-gnu/9 -L/usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu \
# -L/usr/lib/gcc/x86_64-linux-gnu/9/../../../../lib -L/lib/x86_64-linux-gnu -L/lib/../lib \
# -L/usr/lib/x86_64-linux-gnu \
# -L/usr/lib/../lib \
# -L/usr/lib/gcc/x86_64-linux-gnu/9/../../.. \
# hello.o glue.o framework.o  \
# --start-group -lgcc  -lc --end-group \
# /usr/lib/gcc/x86_64-linux-gnu/9/crtend.o \
# /usr/lib/gcc/x86_64-linux-gnu/9/../../../x86_64-linux-gnu/crtn.o \
# -o $(TARGET)

### MUSL
# 	ld -static \
# -T framework.lds -dp -z max-page-size=0x1000 \
# -z relro \
# -L./lib \
# ./lib/Scrt1.o ./lib/crti.o \
# hello.o glue.o framework.o framework_low_level.o  \
# -lc \
# -o $(TARGET)

#-lgcc_eh

clean: 
	rm -f $(TARGET) *.o

