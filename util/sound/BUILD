package(default_visibility = ["//visibility:public"])

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    linkopts = [
        "-lsndfile",
    ],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
