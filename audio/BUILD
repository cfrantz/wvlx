package(default_visibility = ["//visibility:public"])

cc_library(
    name = "fft_channel",
    srcs = ["fft_channel.cc"],
    hdrs = ["fft_channel.h"],
    linkopts = [
        "-lfftw3f",
        "-lm",
    ],
    deps = [
        "//util:os",
        "//util/sound:file",
        "@com_google_absl//absl/log",
    ],
)

cc_library(
    name = "fft_image_cache",
    srcs = ["fft_image_cache.cc"],
    hdrs = ["fft_image_cache.h"],
    linkopts = [
        "-lm",
    ],
    deps = [
        ":fft_channel",
        "//imwidget:glbitmap",
        "@com_github_implot//:implot",
        "@com_google_absl//absl/flags:flag",
    ],
)
