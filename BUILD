package(default_visibility=["//visibility:public"])
load("@mxebzl//tools/windows:rules.bzl", "pkg_winzip")

config_setting(
    name = "windows",
    values = {
        "crosstool_top": "@mxebzl//tools/windows:toolchain",
    }
)

genrule(
    name = "make_version",
    outs = ["version.h"],
    cmd = """sed -e 's/ / "/g' -e 's/$$/"\\n/g' -e "s/^/#define /g" bazel-out/volatile-status.txt > $@""",
    stamp = 1,
)

cc_library(
    name = "app",
    linkopts = [
        "-lSDL2main",
        "-lSDL2",
        "-lSDL2_image",
        "-lSDL2_mixer",
        "-lSDL2_gfx",
    ],
    hdrs = [
        "app.h",
        "version.h",
    ],
    srcs = [
        "app.cc",
    ],
    deps = [
        "//audio:fft_channel",
        "//imwidget:base",
        "//imwidget:error_dialog",
        "//imwidget:wave_display",
        "//imwidget:fft_cache",
        "//imwidget:fft_display",
        "//imwidget:transport",
        "//util:browser",
        "//util:fpsmgr",
        "//util:imgui_sdl_opengl",
        "//util:os",
        "//util:logging",
        "//external:gflags",

        # TODO(cfrantz): on ubuntu 16 with MIR, there is a library conflict
        # between MIR (linked with protobuf 2.6.1) and this program,
        # which builds with protbuf 3.x.x.  A temporary workaround is to
        # not link with nfd (native-file-dialog).
        "//external:nfd",
        "@com_google_absl//absl/memory",
    ],
)

filegroup(
    name = "content",
    srcs = glob(["content/*.textpb"]),
)

genrule(
    name = "make_zelda2_config",
    srcs = [
        "zelda2.textpb",
        ":content",
    ],
    outs = ["zelda2_config.h"],
    cmd = "$(location //tools:pack_config) --config $(location zelda2.textpb)" +
          " --symbol kZelda2Cfg > $(@)",
    tools = ["//tools:pack_config"],
)

cc_binary(
    name = "wvlx",
    linkopts = select({
        ":windows": [
            "-lpthread",
            "-lm",
            "-lopengl32",
            "-ldinput8",
            "-ldxguid",
            "-ldxerr8",
            "-luser32",
            "-lgdi32",
            "-lwinmm",
            "-limm32",
            "-lole32",
            "-loleaut32",
            "-lshell32",
            "-lversion",
            "-luuid",

        ],
        "//conditions:default": [
            "-lpthread",
            "-lm",
            "-lGL",
        ],
    }),
    srcs = [
        "main.cc",
    ],
    deps = [
        ":app",
        "//util:config",
        "//util/sound:file",
        "//external:gflags",
    ],
)

cc_binary(
    name = "wavefile",
    srcs = ["wavefile.cc"],
    deps = [
        "//util/sound:file",
        "//util/sound:math",
    ],
)

pkg_winzip(
    name = "application-windows",
    files = [
        ":application",
    ],
)
