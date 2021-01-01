# 
#  Copyright (c) 2017 Peter Dinda 
#
# Framework code for building "user" executables for Nautilus
#

NAUTILUS_DIR = ../..


TARGET = hello.exe

USER_OBJ = hello.o

BLOB_OBJ = blob.o
BLOB_BC = blob.bc
BLOB_LL = blob.ll
BLOB_KBC = blob.kbc.bc
BLOB_KOBJ = blob.kobj.o

//CC = gcc

CC = gcc
FINALCC = clang
LD = ld
NLD = ./nld
NSIGN = ./nsign
EXTRACTBC = get-bc
LLVMDIS   = llvm-dis

KARAT_TRANS = cp

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

CFLAGS = -O2 -mno-red-zone -fPIC -Wall

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

all: $(TARGET) $(BLOB_LL)

framework_low_level.o: framework_low_level.S
	$(CC) $(CFLAGS) -c framework_low_level.S

framework.o : framework.c
	$(CC) $(CFLAGS)  -I../../include -c framework.c

glue.o : glue.c
	$(CC) $(CFLAGS) -c glue.c

$(BLOB_BC) : $(BLOB_OBJ)
	$(EXTRACTBC) $(BLOB_OBJ)
	mv $(BLOB_OBJ).bc $(BLOB_BC)

$(BLOB_LL) : $(BLOB_BC)
	$(LLVMDIS) $(BLOB_BC)

$(BLOB_KBC) : $(BLOB_BC)
	$(KARAT_TRANS) $(BLOB_BC) $(BLOB_KBC)

$(BLOB_KOBJ) : $(BLOB_KBC)
	$(FINALCC) $(CFLAGS) -c $(BLOB_KBC) -o $(BLOB_KOBJ)

$(BLOB_OBJ) : $(TARGET:.exe=.o) framework.o framework_low_level.o glue.o
	$(LD) $^ -r -o $(BLOB_OBJ)

$(TARGET:.exe=.unsigned) : $(BLOB_KOBJ)
	$(NLD) $^ -o $(TARGET:.exe=.unsigned)

$(TARGET) : $(TARGET:.exe=.unsigned)
	$(NSIGN) $(TARGET:.exe=.unsigned) $(TARGET)


clean: 
	rm -f $(TARGET) *.o *.bc *.ll 

