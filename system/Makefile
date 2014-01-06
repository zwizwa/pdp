
include ../Makefile.config

all: $(PDP_TARGET) pdp.o

common:
	make -C net
	make -C png
	make -C type
	make -C kernel
	make -C image $(PDP_TARGET)
	make -C zl

linux_mmx: common
	make -C mmx

linux_gcc_mmx: common

linux: common

darwin: common

clean:
	rm -f *~
	rm -f *.o
	make -C mmx clean
	make -C net clean
	make -C png clean
	make -C image clean
	make -C type clean
	make -C kernel clean
	make -C zl clean

