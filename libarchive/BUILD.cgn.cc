#include <cgn>
#include <vector>

static const std::string LIBARCHIVE_ROOT = "repo";
static const std::string LIBARCHIVE_SRC = LIBARCHIVE_ROOT + "/libarchive";

// CMake OPTION() defaults (from top-level CMakeLists.txt)
constexpr static bool ENABLE_MBEDTLS         = false; // OFF
constexpr static bool ENABLE_NETTLE          = false; // OFF
constexpr static bool ENABLE_OPENSSL         = true;  // ON
constexpr static bool ENABLE_LIBB2           = false; // (CMakeLists: ON)
constexpr static bool ENABLE_LZ4             = true;  // ON
constexpr static bool ENABLE_LZO             = false; // OFF
constexpr static bool ENABLE_LZMA            = true;  // ON
constexpr static bool ENABLE_ZSTD            = true;  // ON
constexpr static bool ENABLE_ZLIB            = true;  // ON
constexpr static bool ENABLE_BZip2           = true;  // ON
constexpr static bool ENABLE_LIBXML2         = false; // (CMakeLists: ON)
constexpr static bool ENABLE_EXPAT           = true;  // ON
constexpr static bool ENABLE_WIN32_XMLLITE   = true;  // ON
constexpr static bool ENABLE_ICONV           = true;  // ON
constexpr static bool ENABLE_XATTR           = true;  // ON
constexpr static bool ENABLE_ACL             = true;  // ON
constexpr static bool ENABLE_CNG             = true;  // ON


static bool pick_existing_dep(
    const cgn::Configuration &cfg,
    const std::vector<std::string> &labels,
    std::string *picked = nullptr
) {
    for (const auto &label : labels) {
        cgn::CGNTarget probe = api.create_target(label, cfg);
        if (probe.errmsg.empty()) {
            if (picked)
                *picked = label;
            return true;
        }
    }
    return false;
}

// Libarchive 3.8.6
git("libarchive.git", x) {
    x.repo = "https://github.com/libarchive/libarchive.git";
    x.dest_dir = LIBARCHIVE_ROOT;
    x.commit_id = "3a9249b4eeb2a101ca4e0d2b12e4007642bac126";
}

cxx_static("archive_static", x) {
    x.pub.include_dirs = {LIBARCHIVE_SRC};
    x.include_dirs = {LIBARCHIVE_SRC, "."};

    x.defines = {
        "PLATFORM_CONFIG_H=\"libarchive_cgn_config.h\""
    };

    // Core sources (libarchive/CMakeLists.txt: libarchive_SOURCES)
    x.srcs = {
        LIBARCHIVE_SRC + "/archive_acl.c",
        LIBARCHIVE_SRC + "/archive_check_magic.c",
        LIBARCHIVE_SRC + "/archive_cmdline.c",
        LIBARCHIVE_SRC + "/archive_cryptor.c",
        LIBARCHIVE_SRC + "/archive_digest.c",
        LIBARCHIVE_SRC + "/archive_entry.c",
        LIBARCHIVE_SRC + "/archive_entry_copy_stat.c",
        LIBARCHIVE_SRC + "/archive_entry_link_resolver.c",
        LIBARCHIVE_SRC + "/archive_entry_sparse.c",
        LIBARCHIVE_SRC + "/archive_entry_stat.c",
        LIBARCHIVE_SRC + "/archive_entry_strmode.c",
        LIBARCHIVE_SRC + "/archive_entry_xattr.c",
        LIBARCHIVE_SRC + "/archive_hmac.c",
        LIBARCHIVE_SRC + "/archive_match.c",
        LIBARCHIVE_SRC + "/archive_options.c",
        LIBARCHIVE_SRC + "/archive_pack_dev.c",
        LIBARCHIVE_SRC + "/archive_parse_date.c",
        LIBARCHIVE_SRC + "/archive_pathmatch.c",
        LIBARCHIVE_SRC + "/archive_ppmd8.c",
        LIBARCHIVE_SRC + "/archive_ppmd7.c",
        LIBARCHIVE_SRC + "/archive_random.c",
        LIBARCHIVE_SRC + "/archive_rb.c",
        LIBARCHIVE_SRC + "/archive_read.c",
        LIBARCHIVE_SRC + "/archive_read_add_passphrase.c",
        LIBARCHIVE_SRC + "/archive_read_append_filter.c",
        LIBARCHIVE_SRC + "/archive_read_data_into_fd.c",
        LIBARCHIVE_SRC + "/archive_read_disk_entry_from_file.c",
        LIBARCHIVE_SRC + "/archive_read_disk_posix.c",
        LIBARCHIVE_SRC + "/archive_read_disk_set_standard_lookup.c",
        LIBARCHIVE_SRC + "/archive_read_extract.c",
        LIBARCHIVE_SRC + "/archive_read_extract2.c",
        LIBARCHIVE_SRC + "/archive_read_open_fd.c",
        LIBARCHIVE_SRC + "/archive_read_open_file.c",
        LIBARCHIVE_SRC + "/archive_read_open_filename.c",
        LIBARCHIVE_SRC + "/archive_read_open_memory.c",
        LIBARCHIVE_SRC + "/archive_read_set_format.c",
        LIBARCHIVE_SRC + "/archive_read_set_options.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_all.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_by_code.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_bzip2.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_compress.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_gzip.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_grzip.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_lrzip.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_lz4.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_lzop.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_none.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_program.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_rpm.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_uu.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_xz.c",
        LIBARCHIVE_SRC + "/archive_read_support_filter_zstd.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_7zip.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_all.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_ar.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_by_code.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_cab.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_cpio.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_empty.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_iso9660.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_lha.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_mtree.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_rar.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_rar5.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_raw.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_tar.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_warc.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_xar.c",
        LIBARCHIVE_SRC + "/archive_read_support_format_zip.c",
        LIBARCHIVE_SRC + "/archive_string.c",
        LIBARCHIVE_SRC + "/archive_string_sprintf.c",
        LIBARCHIVE_SRC + "/archive_time.c",
        LIBARCHIVE_SRC + "/archive_util.c",
        LIBARCHIVE_SRC + "/archive_version_details.c",
        LIBARCHIVE_SRC + "/archive_virtual.c",
        LIBARCHIVE_SRC + "/archive_write.c",
        LIBARCHIVE_SRC + "/archive_write_disk_posix.c",
        LIBARCHIVE_SRC + "/archive_write_disk_set_standard_lookup.c",
        LIBARCHIVE_SRC + "/archive_write_open_fd.c",
        LIBARCHIVE_SRC + "/archive_write_open_file.c",
        LIBARCHIVE_SRC + "/archive_write_open_filename.c",
        LIBARCHIVE_SRC + "/archive_write_open_memory.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_b64encode.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_by_name.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_bzip2.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_compress.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_grzip.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_gzip.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_lrzip.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_lz4.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_lzop.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_none.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_program.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_uuencode.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_xz.c",
        LIBARCHIVE_SRC + "/archive_write_add_filter_zstd.c",
        LIBARCHIVE_SRC + "/archive_write_set_format.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_7zip.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_ar.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_by_name.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_cpio.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_cpio_binary.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_cpio_newc.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_cpio_odc.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_filter_by_ext.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_gnutar.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_iso9660.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_mtree.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_pax.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_raw.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_shar.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_ustar.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_v7tar.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_warc.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_xar.c",
        LIBARCHIVE_SRC + "/archive_write_set_format_zip.c",
        LIBARCHIVE_SRC + "/archive_write_set_options.c",
        LIBARCHIVE_SRC + "/archive_write_set_passphrase.c",
        LIBARCHIVE_SRC + "/filter_fork_posix.c",
        LIBARCHIVE_SRC + "/xxhash.c",
    };

    // Windows-specific sources (libarchive/CMakeLists.txt: WIN32 AND NOT CYGWIN)
    if (x.cfg["os"] == "win") {
        x.srcs += {
            LIBARCHIVE_SRC + "/archive_entry_copy_bhfi.c",
            LIBARCHIVE_SRC + "/archive_read_disk_windows.c",
            LIBARCHIVE_SRC + "/archive_windows.c",
            LIBARCHIVE_SRC + "/archive_write_disk_windows.c",
            LIBARCHIVE_SRC + "/filter_fork_windows.c"
        };
    }

    // ENABLE_LIBB2: use system libb2 if found; else fall back to bundled blake2
    // (libarchive/CMakeLists.txt: IF(ARCHIVE_BLAKE2) -> add blake2sp_ref + blake2s_ref)
    bool archive_blake2 = true;
    if (ENABLE_LIBB2) {
        std::string picked;
        if (pick_existing_dep(x.cfg,
                {"@third_party//libb2:b2", "@third_party//libb2"},
                &picked)) {
            x.add_dep(picked, cxx::private_dep);
            x.defines += {"HAVE_LIBB2=1", "HAVE_BLAKE2_H=1"};
            archive_blake2 = false;
        }
    }
    if (archive_blake2) {
        x.defines += {"ARCHIVE_BLAKE2=1"};
        x.srcs += {
            LIBARCHIVE_SRC + "/archive_blake2sp_ref.c",
            LIBARCHIVE_SRC + "/archive_blake2s_ref.c"
        };
    }

    auto add_optional_dep = [&](bool enabled,
                                const std::vector<std::string> &labels,
                                const std::vector<std::string> &defs) {
        if (!enabled) return;
        std::string picked;
        if (pick_existing_dep(x.cfg, labels, &picked)) {
            x.add_dep(picked, cxx::private_dep);
            x.defines += defs;
        }
    };

    // ENABLE_ZLIB (CMake: FIND_PACKAGE(ZLIB) -> HAVE_LIBZ, HAVE_ZLIB_H)
    add_optional_dep(
        ENABLE_ZLIB,
        {"@third_party//zlib:z_static", "@third_party//zlib:z"},
        {"HAVE_LIBZ=1", "HAVE_ZLIB_H=1"}
    );
    // ENABLE_BZip2 (CMake: FIND_PACKAGE(BZip2) -> HAVE_LIBBZ2, HAVE_BZLIB_H)
    add_optional_dep(
        ENABLE_BZip2,
        {"@third_party//bzip2:bz2_static", "@third_party//bzip2:bz2"},
        {"HAVE_LIBBZ2=1", "HAVE_BZLIB_H=1"}
    );
    // ENABLE_LZMA (CMake: FIND_PACKAGE(LibLZMA) -> HAVE_LIBLZMA, HAVE_LZMA_H)
    add_optional_dep(
        ENABLE_LZMA,
        {"@third_party//xz:lzma_static", "@third_party//xz:lzma"},
        {"HAVE_LIBLZMA=1", "HAVE_LZMA_H=1"}
    );
    // ENABLE_ZSTD (CMake: FIND_PACKAGE(ZSTD) -> HAVE_LIBZSTD, HAVE_ZSTD_H)
    add_optional_dep(
        ENABLE_ZSTD,
        {"@third_party//zstd:zstd_static", "@third_party//zstd"},
        {"HAVE_LIBZSTD=1", "HAVE_ZSTD_H=1"}
    );
    // ENABLE_LZ4 (CMake: FIND_LIBRARY(LZ4) -> HAVE_LIBLZ4, HAVE_LZ4_H)
    add_optional_dep(
        ENABLE_LZ4,
        {"@third_party//lz4:lz4_static", "@third_party//lz4"},
        {"HAVE_LIBLZ4=1", "HAVE_LZ4_H=1"}
    );
    // ENABLE_LIBXML2 preferred over ENABLE_EXPAT (CMake: try libxml2 first, fall back to expat)
    {
        bool xml_found = false;
        if (ENABLE_LIBXML2) {
            std::string picked;
            if (pick_existing_dep(x.cfg,
                    {"@third_party//libxml2:xml2", "@third_party//libxml2"},
                    &picked)) {
                x.add_dep(picked, cxx::private_dep);
                x.defines += {
                    "HAVE_LIBXML2=1",
                    "HAVE_LIBXML_XMLREADER_H=1",
                    "HAVE_LIBXML_XMLWRITER_H=1"
                };
                xml_found = true;
            }
        }
        if (!xml_found && ENABLE_EXPAT) {
            std::string picked;
            if (pick_existing_dep(x.cfg,
                    {"@third_party//libexpat:expat_static", "@third_party//libexpat"},
                    &picked)) {
                x.add_dep(picked, cxx::private_dep);
                x.defines += {"HAVE_LIBEXPAT=1", "HAVE_EXPAT_H=1"};
            }
        }
    }
    // ENABLE_OPENSSL (CMake: FIND_PACKAGE(OpenSSL) -> HAVE_LIBCRYPTO + crypto defines)
    add_optional_dep(
        ENABLE_OPENSSL,
        {"@third_party//openssl:openssl3_static", "@third_party//openssl:openssl3_shared", "@third_party//openssl"},
        {
            "HAVE_LIBCRYPTO=1",
            "HAVE_OPENSSL_EVP_H=1",
            "HAVE_OPENSSL_OPENSSLV_H=1",
            "ARCHIVE_CRYPTO_MD5_OPENSSL=1",
            "ARCHIVE_CRYPTO_RMD160_OPENSSL=1",
            "ARCHIVE_CRYPTO_SHA1_OPENSSL=1",
            "ARCHIVE_CRYPTO_SHA256_OPENSSL=1",
            "ARCHIVE_CRYPTO_SHA384_OPENSSL=1",
            "ARCHIVE_CRYPTO_SHA512_OPENSSL=1"
        }
    );
}

alias("libarchive", x) {
    x.actual_label = ":archive_static";
}

cxx_executable("smoke", x) {
    x.srcs = {"smoke.c"};
    x.add_dep(":archive_static", cxx::private_dep);
}

