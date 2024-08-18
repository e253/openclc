OPENCLC:=../out/x86_64-linux-musl-x86_64/bin/openclc

vadd: vec_add.cl
	$(OPENCLC) vec_add.cl -o vadd

vadd_raw: vec_add_raw_opencl.c
	zig cc vec_add_raw_opencl.c -lOpenCL -o vadd_raw