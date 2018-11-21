package(default_visibility = ["//visibility:public"])

# proto rules
load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")
load("@org_pubref_rules_protobuf//python:rules.bzl", "py_proto_library")

# docker image rules
load("@io_bazel_rules_docker//cc:image.bzl", "cc_image")
load("@io_bazel_rules_docker//python:image.bzl", "py_image")

cc_proto_library(
    name = "cc_seckill_proto",
    protos = ["seckill.proto"],
    with_grpc = True,
)

cc_binary(
    name = "seckill_server",
    srcs = ["seckill_server.cc"],
    deps = [
        ":cc_seckill_proto",
        ":mysqlDriver",
        ":hiredisDriver",
        ":Md5Util"
    ],
    copts = ["-Isrc/include"],
)



cc_library(
    name = "mysqlDriver",
    srcs = ["libmysqlclient.so","libmysqlclient_r.so"],
    hdrs = glob(["include/**/*.h"]),
)

cc_library(
    name = "hiredisDriver",
    srcs = ["libhiredis.so","libhiredis.a"],
    hdrs = glob(["include/**/*.h"]),
)



cc_image(
    name = "seckill_server_image",
    srcs = ["seckill_server.cc"],
    deps = [
        ":cc_seckill_proto",
    ],
)

cc_binary(
    name = "seckill_client",
    srcs = ["seckill_client.cc"],
    deps = [
        ":cc_seckill_proto",
        ":Md5Util"
    ],
)

cc_library(
    name = "Md5Util",
    srcs = ["Md5.cc"],
    hdrs = ["Md5.h"],
)


py_proto_library(
    name = "py_seckill_proto",
    protos = ["seckill.proto"],
    with_grpc = True,
)

py_binary(
    name = "py_seckill_client",
    srcs = ["seckill_client.py"],
    main = "seckill_client.py",
    deps = [
        ":py_seckill_proto"
    ]
)
