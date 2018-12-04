package(default_visibility = ["//visibility:public"])

#licenses(["notice"])

#load("@org_pubref_rules_protobuf", "cc_proto_library")
# proto rules org_pubref_rules_protobuf
#load("@bazel_tools//tools/build_defs/repo:git.bzl", "cc_proto_library")

#load("@bazel_tools//tools/build_defs/repo:git.bzl", "cc_proto_library","cc_binary","cc_library")

#load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

#with_grpc = True,cc_proto_library

#load("//tools/build_rules:grpc.bzl", "cc_grpc_library")

#load("@org_pubref_rules_protobuf", "cc_proto_library")

#load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")

cc_proto_library(
    name = "cc_seckill_proto",
    protos = ["seckill.proto"],
    with_grpc = True,
)

cc_binary(
    name = "seckill_server",
    srcs = ["seckill_server.cc"],
    deps = [
        ":Worker",
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



cc_library(
    name = "Worker",
    srcs = ["Worker.cc"],
    hdrs = ["Worker.h"],
    deps = [
        ":cc_seckill_proto",
        ":mysqlDriver",
        ":hiredisDriver",
        ":Md5Util",
        ":mysqlpool",
        ":redispool",
    ],
    copts = ["-Isrc/include"],
)

cc_binary(
    name = "seckill_client",
    srcs = ["seckill_client.cc"],
    deps = [
        ":cc_seckill_proto",
        ":Md5Util",
        ":mysqlpool",
    ],
)
cc_library(
    name = "mysqlpool",
    srcs = ["mysqlpool.cc"],
    hdrs = ["mysqlpool.h"],
    deps = [
        ":mysqlDriver",
    ],
    copts = ["-Isrc/include"],
)

cc_library(
    name = "redispool",
    srcs = ["redispool.cc"],
    hdrs = ["redispool.h"],
    deps = [
        ":hiredisDriver",
    ],
    copts = ["-Isrc/include"],
)

cc_library(
    name = "Md5Util",
    srcs = ["Md5.cc"],
    hdrs = ["Md5.h"],
)





