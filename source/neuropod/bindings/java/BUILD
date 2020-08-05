# Copyright (c) 2020 UATC, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(":java_build_defs.bzl", "JAVACOPTS")

genrule(
    name = "copy_link_jni_md_header",
    srcs = select({
        "@bazel_tools//src/conditions:darwin": ["@bazel_tools//tools/jdk:jni_md_header-darwin"],
        "//conditions:default": ["@bazel_tools//tools/jdk:jni_md_header-linux"],
    }),
    outs = ["jni_md.h"],
    cmd = "cp -f $< $@",
)

genrule(
    name = "copy_link_jni_header",
    srcs = ["@bazel_tools//tools/jdk:jni_header"],
    outs = ["jni.h"],
    cmd = "cp -f $< $@",
)

filegroup(
    name = "backends",
    srcs = [
        "//neuropod/backends/python_bridge:libneuropod_pythonbridge_backend.so",
        "//neuropod/backends/tensorflow:libneuropod_tensorflow_backend.so",
        "//neuropod/backends/torchscript:libneuropod_torchscript_backend.so",
        "@libtorch_repo//:libtorch_libs",
        "@tensorflow_repo//:libtensorflow_libs",
    ],
    visibility = [
        "//neuropod:__subpackages__",
    ],
)

genrule(
    name = "copy_to_dylib",
    srcs = [":libneuropod_jni.so"],
    outs = ["libneuropod_jni.dylib"],
    cmd = "cp -f $< $@",
)

filegroup(
    name = "neuropod_jni",
    srcs = select({
        "@bazel_tools//src/conditions:darwin": [":copy_to_dylib"],
        "//conditions:default": [":libneuropod_jni.so"],
    }),
    visibility = [
        "//neuropod:__subpackages__",
    ],
)

filegroup(
    name = "java_source",
    srcs = glob(["src/main/java/com/uber/neuropod/*.java"]),
    visibility = [
        "//neuropod:__subpackages__",
    ],
)

java_binary(
    name = "neuropod_java_jar",
    srcs = [":java_source"],
    # TODO(weijiad): Find a way to make these native libraries locate at /com/uber/neuropod/native/{OS}/{PLATFORM}
    # in the jar file, instead of locating at the class root path.
    classpath_resources = [
        "//neuropod:libneuropod.so",
        "//neuropod/multiprocess:neuropod_multiprocess_worker",
        ":neuropod_jni",
    ],
    javacopts = JAVACOPTS,
    visibility = ["//visibility:public"],
)

java_library(
    name = "neuropod_java",
    srcs = [":java_source"],
    data = [
        ":backends",
        ":neuropod_jni",
        "//neuropod:libneuropod.so",
        "//neuropod/multiprocess:neuropod_multiprocess_worker",
    ],
    javacopts = JAVACOPTS,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "copy_jni_hdr_lib",
    hdrs = [
        ":copy_link_jni_header",
        ":copy_link_jni_md_header",
    ],
    includes = ["."],
)

cc_binary(
    name = "libneuropod_jni.so",
    srcs = glob(["src/main/native/*.cc"]) + glob(["src/main/native/*.h"]) + [
        "//neuropod:libneuropod.so",
    ],
    includes = select({
        "@bazel_tools//src/conditions:darwin": ["external/local_jdk/include/darwin"],
        "//conditions:default": ["external/local_jdk/include/linux"],
    }),
    linkopts = select({
        "@bazel_tools//src/conditions:darwin": ["-Wl,-rpath,@loader_path"],
        "//conditions:default": [
            "-Wl,-rpath,$$ORIGIN",
        ],
    }),
    linkshared = True,
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":copy_jni_hdr_lib",
        "//neuropod:neuropod_hdrs",
    ],
)

java_test(
    name = "LibraryLoaderTest",
    srcs = ["src/test/java/com/uber/neuropod/LibraryLoaderTest.java"],
    javacopts = JAVACOPTS,
    test_class = "com.uber.neuropod.LibraryLoaderTest",
    deps = [
        ":neuropod_java",
        "@junit",
    ],
)