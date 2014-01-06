.PHONY: all clean run_test
all: run_test
clean:
	rm -f *.o test *~

H = $(wildcard *.h)
C = $(wildcard *.c)
O = $(C:.c=.o)
CFLAGS := -O3 -fPIC -ffast-math -I.. -DHAVE_LIBV4L1_VIDEODEV_H -Wall #-Werror
%.o: %.c $(H)
	$(CC) $(CFLAGS) -c $< -o $@


# 2013/10/16 : Still in the process of merging PDP and libprim's dependencies on zl.
# Test is only for a subset of the objects:

TEST_O := v4l.o xwindow.o xv.o glx.o 3Dcontext_common.o 3Dcontext_glx.o test.o
TEST_LIBS := -lpthread -L/usr/X11R6/lib -lX11 -lXv -lXext -lGL -lGLU #-llua
test: $(TEST_O)
	$(CC) -o test $(TEST_O) $(TEST_LIBS)

run_test: test
	./test

