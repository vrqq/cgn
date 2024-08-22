// PRIVATE third parties
// copied from Android source code
// -------------------------------
#pragma once
#include "common.cgn.h"
#include <cgn>


cxx_sources("libgrpc_third_party_upb_headers", x) {
    x.pub.include_dirs = {repo_dir + "/third_party/upb"};
}

// https://cs.android.com/android/platform/superproject/main/+/main:external/grpc-grpc/third_party/upb/Android.bp
cxx_static("libgrpc_third_party_upb", x) {
    x.cflags = {"-Wno-unused-parameter"};
    x.pub.include_dirs = {repo_dir + "/third_party/upb"};
    x.include_dirs = {repo_dir + "/third_party/upb"};
    x.srcs = add_prefix(repo_dir + "/third_party/upb/", {
        "upb/base/status.c",
        "upb/hash/common.c",
        "upb/json/decode.c",
        "upb/json/encode.c",
        "upb/lex/atoi.c",
        "upb/lex/round_trip.c",
        "upb/lex/strtod.c",
        "upb/lex/unicode.c",
        "upb/mem/alloc.c",
        "upb/mem/arena.c",
        "upb/message/internal/extension.c",
        "upb/message/internal/message.c",
        "upb/message/accessors.c",
        "upb/message/array.c",
        "upb/message/compare.c",
        "upb/message/compat.c",
        "upb/message/copy.c",
        "upb/message/map.c",
        "upb/message/map_sorter.c",
        "upb/message/message.c",
        "upb/message/promote.c",
        "upb/mini_descriptor/internal/base92.c",
        "upb/mini_descriptor/internal/encode.c",
        "upb/mini_descriptor/build_enum.c",
        "upb/mini_descriptor/decode.c",
        "upb/mini_descriptor/link.c",
        "upb/mini_table/internal/message.c",
        "upb/mini_table/compat.c",
        "upb/mini_table/extension_registry.c",
        "upb/mini_table/message.c",
        "upb/reflection/internal/def_builder.c",
        "upb/reflection/internal/strdup2.c",
        "upb/reflection/def_pool.c",
        "upb/reflection/def_type.c",
        "upb/reflection/desc_state.c",
        "upb/reflection/enum_def.c",
        "upb/reflection/enum_reserved_range.c",
        "upb/reflection/enum_value_def.c",
        "upb/reflection/extension_range.c",
        "upb/reflection/field_def.c",
        "upb/reflection/file_def.c",
        "upb/reflection/message.c",
        "upb/reflection/message_def.c",
        "upb/reflection/message_reserved_range.c",
        "upb/reflection/method_def.c",
        "upb/reflection/oneof_def.c",
        "upb/reflection/service_def.c",
        "upb/text/encode.c",
        "upb/wire/decode.c",
        "upb/wire/encode.c",
        "upb/wire/eps_copy_input_stream.c",
        "upb/wire/reader.c"
    });
    x.add_dep(":libgrpc_upb_protos", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_utf8_range", cxx::private_dep);
}

cxx_static("libgrpc_third_party_utf8_range", x) {
    x.pub.include_dirs = {repo_dir + "/third_party/utf8_range"};
    x.include_dirs = {repo_dir + "/third_party/utf8_range"};
    x.srcs = add_prefix(repo_dir + "/third_party/utf8_range/", {
        "naive.c",
        "range2-neon.c",
        "range2-sse.c",
        "utf8_range.c"
    });
}

cxx_static("libgrpc_third_party_xxhash", x) {
    x.pub.include_dirs = {repo_dir + "/third_party/xxhash"};
}

cxx_static("libgrpc_third_party_libaddress_sorting", x) {
    x.pub.include_dirs = {
        repo_dir + "/third_party/address_sorting/include"
    };
    x.include_dirs = {
        repo_dir + "/third_party/address_sorting/include",
        repo_dir + "/third_party/address_sorting/include/address_sorting"
    };
    x.srcs = add_prefix(repo_dir + "/third_party/address_sorting/", {
        "address_sorting.c",
        "address_sorting_posix.c",
        "address_sorting_windows.c"
    });
}
