#include <cgn>

static const std::string XZ_ROOT = "repo";

// XZ Utils 5.8.3 https://tukaani.org/xz/
git("xz.git", x) {
    x.repo = "https://codeberg.org/tukaani/xz.git";
    x.dest_dir = XZ_ROOT;
    x.commit_id = "4b73f2ec19a99ef465282fbce633e8deb33691b3";
}

cxx_static("lzma", x) {
    x.pub.include_dirs = {XZ_ROOT + "/src/liblzma/api"};
    x.include_dirs = {
        XZ_ROOT,
        XZ_ROOT + "/src/liblzma/api",
        XZ_ROOT + "/src/liblzma/common",
        XZ_ROOT + "/src/liblzma/check",
        XZ_ROOT + "/src/liblzma/lz",
        XZ_ROOT + "/src/liblzma/lzma",
        XZ_ROOT + "/src/liblzma/rangecoder",
        XZ_ROOT + "/src/liblzma/delta",
        XZ_ROOT + "/src/liblzma/simple",
        XZ_ROOT + "/src/common"
    };

    x.pub.ldflags = {"-lpthread"};
    x.defines = {
        "HAVE_CONFIG_H",
        "TUKLIB_SYMBOL_PREFIX=lzma_"
    };

    x.srcs = {
        XZ_ROOT + "/src/common/tuklib_physmem.c",
        XZ_ROOT + "/src/common/tuklib_cpucores.c",

        XZ_ROOT + "/src/liblzma/common/common.c",
        XZ_ROOT + "/src/liblzma/common/block_util.c",
        XZ_ROOT + "/src/liblzma/common/easy_preset.c",
        XZ_ROOT + "/src/liblzma/common/filter_common.c",
        XZ_ROOT + "/src/liblzma/common/hardware_physmem.c",
        XZ_ROOT + "/src/liblzma/common/hardware_cputhreads.c",
        XZ_ROOT + "/src/liblzma/common/index.c",
        XZ_ROOT + "/src/liblzma/common/stream_flags_common.c",
        XZ_ROOT + "/src/liblzma/common/string_conversion.c",
        XZ_ROOT + "/src/liblzma/common/vli_size.c",
        XZ_ROOT + "/src/liblzma/common/outqueue.c",
        XZ_ROOT + "/src/liblzma/common/alone_encoder.c",
        XZ_ROOT + "/src/liblzma/common/block_buffer_encoder.c",
        XZ_ROOT + "/src/liblzma/common/block_encoder.c",
        XZ_ROOT + "/src/liblzma/common/block_header_encoder.c",
        XZ_ROOT + "/src/liblzma/common/easy_buffer_encoder.c",
        XZ_ROOT + "/src/liblzma/common/easy_encoder.c",
        XZ_ROOT + "/src/liblzma/common/easy_encoder_memusage.c",
        XZ_ROOT + "/src/liblzma/common/filter_buffer_encoder.c",
        XZ_ROOT + "/src/liblzma/common/filter_encoder.c",
        XZ_ROOT + "/src/liblzma/common/filter_flags_encoder.c",
        XZ_ROOT + "/src/liblzma/common/index_encoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_buffer_encoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_encoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_flags_encoder.c",
        XZ_ROOT + "/src/liblzma/common/vli_encoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_encoder_mt.c",
        XZ_ROOT + "/src/liblzma/common/microlzma_encoder.c",
        XZ_ROOT + "/src/liblzma/common/alone_decoder.c",
        XZ_ROOT + "/src/liblzma/common/auto_decoder.c",
        XZ_ROOT + "/src/liblzma/common/block_buffer_decoder.c",
        XZ_ROOT + "/src/liblzma/common/block_decoder.c",
        XZ_ROOT + "/src/liblzma/common/block_header_decoder.c",
        XZ_ROOT + "/src/liblzma/common/easy_decoder_memusage.c",
        XZ_ROOT + "/src/liblzma/common/file_info.c",
        XZ_ROOT + "/src/liblzma/common/filter_buffer_decoder.c",
        XZ_ROOT + "/src/liblzma/common/filter_decoder.c",
        XZ_ROOT + "/src/liblzma/common/filter_flags_decoder.c",
        XZ_ROOT + "/src/liblzma/common/index_decoder.c",
        XZ_ROOT + "/src/liblzma/common/index_hash.c",
        XZ_ROOT + "/src/liblzma/common/stream_buffer_decoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_decoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_flags_decoder.c",
        XZ_ROOT + "/src/liblzma/common/vli_decoder.c",
        XZ_ROOT + "/src/liblzma/common/stream_decoder_mt.c",
        XZ_ROOT + "/src/liblzma/common/microlzma_decoder.c",
        XZ_ROOT + "/src/liblzma/common/lzip_decoder.c",

        XZ_ROOT + "/src/liblzma/check/check.c",
        XZ_ROOT + "/src/liblzma/check/crc32_fast.c",
        XZ_ROOT + "/src/liblzma/check/crc64_fast.c",
        XZ_ROOT + "/src/liblzma/check/sha256.c",

        XZ_ROOT + "/src/liblzma/lz/lz_encoder.c",
        XZ_ROOT + "/src/liblzma/lz/lz_encoder_mf.c",
        XZ_ROOT + "/src/liblzma/lz/lz_decoder.c",

        XZ_ROOT + "/src/liblzma/lzma/lzma_encoder_presets.c",
        XZ_ROOT + "/src/liblzma/lzma/lzma_encoder.c",
        XZ_ROOT + "/src/liblzma/lzma/lzma_encoder_optimum_fast.c",
        XZ_ROOT + "/src/liblzma/lzma/lzma_encoder_optimum_normal.c",
        XZ_ROOT + "/src/liblzma/lzma/fastpos_table.c",
        XZ_ROOT + "/src/liblzma/lzma/lzma_decoder.c",
        XZ_ROOT + "/src/liblzma/lzma/lzma2_encoder.c",
        XZ_ROOT + "/src/liblzma/lzma/lzma2_decoder.c",

        XZ_ROOT + "/src/liblzma/rangecoder/price_table.c",

        XZ_ROOT + "/src/liblzma/delta/delta_common.c",
        XZ_ROOT + "/src/liblzma/delta/delta_encoder.c",
        XZ_ROOT + "/src/liblzma/delta/delta_decoder.c",

        XZ_ROOT + "/src/liblzma/simple/simple_coder.c",
        XZ_ROOT + "/src/liblzma/simple/simple_encoder.c",
        XZ_ROOT + "/src/liblzma/simple/simple_decoder.c",
        XZ_ROOT + "/src/liblzma/simple/x86.c",
        XZ_ROOT + "/src/liblzma/simple/powerpc.c",
        XZ_ROOT + "/src/liblzma/simple/ia64.c",
        XZ_ROOT + "/src/liblzma/simple/arm.c",
        XZ_ROOT + "/src/liblzma/simple/armthumb.c",
        XZ_ROOT + "/src/liblzma/simple/arm64.c",
        XZ_ROOT + "/src/liblzma/simple/sparc.c",
        XZ_ROOT + "/src/liblzma/simple/riscv.c"
    };
}

cxx_executable("xz", x) {
    x.defines = {
        "HAVE_CONFIG_H",
        "PACKAGE_NAME=\"XZ Utils\"",
        "PACKAGE_BUGREPORT=\"xz@tukaani.org\"",
        "PACKAGE_URL=\"https://tukaani.org/xz/\"",
        "ASSUME_RAM=128"
    };
    x.include_dirs = {
        XZ_ROOT,
        XZ_ROOT + "/src/common",
        XZ_ROOT + "/src/liblzma/api"
    };
    x.srcs = {
        XZ_ROOT + "/src/xz/args.c",
        XZ_ROOT + "/src/xz/coder.c",
        XZ_ROOT + "/src/xz/file_io.c",
        XZ_ROOT + "/src/xz/hardware.c",
        XZ_ROOT + "/src/xz/list.c",
        XZ_ROOT + "/src/xz/main.c",
        XZ_ROOT + "/src/xz/message.c",
        XZ_ROOT + "/src/xz/mytime.c",
        XZ_ROOT + "/src/xz/options.c",
        XZ_ROOT + "/src/xz/sandbox.c",
        XZ_ROOT + "/src/xz/signals.c",
        XZ_ROOT + "/src/xz/suffix.c",
        XZ_ROOT + "/src/xz/util.c",
        XZ_ROOT + "/src/common/tuklib_progname.c",
        XZ_ROOT + "/src/common/tuklib_exit.c",
        XZ_ROOT + "/src/common/tuklib_mbstr_fw.c",
        XZ_ROOT + "/src/common/tuklib_mbstr_width.c",
        XZ_ROOT + "/src/common/tuklib_mbstr_nonprint.c",
        XZ_ROOT + "/src/common/tuklib_mbstr_wrap.c",
        XZ_ROOT + "/src/common/tuklib_open_stdxxx.c"
    };
    x.ldflags = {"-lpthread"};
    x.add_dep(":lzma", cxx::private_dep);
}


alias("lzma_static", x) {
    x.actual_label = ":lzma";
}