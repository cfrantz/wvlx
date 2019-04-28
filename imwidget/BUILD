package(default_visibility=["//visibility:public"])

cc_library(
    name = "base",
    hdrs = [
        "debug_console.h",
        "imapp.h",
        "imutil.h",
        "imwidget.h",
    ],
    srcs = [
        "debug_console.cc",
        "imapp.cc",
        "imutil.cc",
    ],
    deps = [
        "//util:fpsmgr",
        "//util:gamecontrollerdb",
        "//util:imgui_sdl_opengl",
        "//util:logging",
        "//util:os",
        "//external:gflags",
        "//external:imgui",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "error_dialog",
    hdrs = ["error_dialog.h"],
    srcs = ["error_dialog.cc"],
    deps = [
        ":base",
        "//external:imgui",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "glbitmap",
    hdrs = ["glbitmap.h"],
    srcs = ["glbitmap.cc"],
    deps = [
        "//external:imgui",
    ],
)

cc_library(
    name = "fft_cache",
    hdrs = ["fft_cache.h"],
    srcs = ["fft_cache.cc"],
    deps = [
        ":glbitmap",
        "//audio:fft_channel",
        "//external:imgui",
    ],
)

cc_library(
    name = "wave_display",
    hdrs = ["wave_display.h"],
    srcs = ["wave_display.cc"],
    deps = [
        ":transport",
        "//external:imgui",
        "//util/sound:file",
    ],
)

cc_library(
    name = "fft_display",
    hdrs = ["fft_display.h"],
    srcs = ["fft_display.cc"],
    deps = [
        ":glbitmap",
        ":fft_cache",
        ":transport",
    ],
)
cc_library(
    name = "transport",
    hdrs = ["transport.h"],
    srcs = ["transport.cc"],
    deps = [
        "//external:imgui",
        "//external:fontawesome",
    ],
)
