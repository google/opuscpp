# C++ wrapper around the libopus C interface
cc_library(
    name = "opus_wrapper",
    srcs = ["opus_wrapper.cc"],
    hdrs = ["opus_wrapper.h"],
    visibility = ["//visibility:public"],
    linkopts = ["-lopus"],
    copts = ["-Wextra"],
    deps = [
        "@glog//:glog",
    ]
)

cc_test(
    name = "opus_wrapper_test",
    srcs = ["opus_wrapper_test.cc"],
    deps = [
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        ":opus_wrapper",
    ],
)
