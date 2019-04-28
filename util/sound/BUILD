package(default_visibility=["//visibility:public"])

cc_library(
    name = "file",
    hdrs = ["file.h"],
    srcs = ["file.cc"],
    linkopts = [
        "-lsndfile",
    ],
    deps = [
        "//util:logging",
        "//util:status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/memory",
    ]
)

cc_library(
    name = "math",
    hdrs = ["math.h"],
)
