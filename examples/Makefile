OPENCLC := ../out/x86_64-linux-musl-x86_64/bin/openclc
CC := zig cc

.PHONY:=all

all: vadd vadd_raw

vadd: vec_add.cl
	$(OPENCLC) vec_add.cl -o vadd

vadd_raw: vec_add_raw_opencl.c
	$(CC) vec_add_raw_opencl.c -lOpenCL -o vadd_raw

clean:
	rm -f vadd vadd_raw	