package(default_visibility = ["//visibility:public"])

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    linkopts = [
        "-lsndfile",
    ],
    deps = [
        "//proto:sound",
        "@com_github_nitnelave_lru_cache//:lru_cache",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "convert",
    srcs = ["convert.cc"],
    deps = [":file"],
)
