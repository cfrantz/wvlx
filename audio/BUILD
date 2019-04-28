package(default_visibility=["//visibility:public"])

cc_library(
    name = "fft_channel",
    hdrs = [ "fft_channel.h" ],
    srcs = [ "fft_channel.cc" ],
    deps = [
        "@com_google_absl//absl/memory",
        "//util/sound:file",
    ],
    linkopts = [
        "-lfftw3f",
        "-lm",
    ],
)
