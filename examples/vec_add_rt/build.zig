const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    if (target.result.os.tag == .linux and target.result.abi == .musl) {
        @panic("**-linux-musl builds don't work for this example :(");
    }

    compileCL(b, &.{"vec_add.cl"});

    // this is the opencl driver loader
    const opencl = b.dependency("opencl", .{
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "vadd",
        .target = target,
        .optimize = optimize,
    });
    exe.addCSourceFiles(.{
        .files = &.{
            "vec_add.c",
            "../../runtime/openclc_rt.c",
            "kernels.c",
        },
        .flags = &.{
            "-DCL_TARGET_OPENCL_VERSION=300",
        },
    });
    exe.addIncludePath(b.path("../../runtime"));
    exe.linkLibrary(opencl.artifact("OpenCL"));
    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}

/// Compile OpenCL sources to a `spvbin.c`, `kernels.c`, and `kernels.h`
fn compileCL(b: *std.Build, sources: []const []const u8) void {
    const openclc_cmd = b.addSystemCommand(&.{"../../out/x86_64-linux-musl-x86_64/bin/openclc"});
    for (sources) |source| {
        openclc_cmd.addFileArg(b.path(source));
    }
    b.getInstallStep().dependOn(&openclc_cmd.step);
}
