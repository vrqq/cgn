#include <cgn>

git("abseil-cpp.git", x) {
    x.repo = "https://github.com/abseil/abseil-cpp.git";
    x.commit_id = "4447c7562e3bc702ade25105912dce503f0c4010";
    x.dest_dir = "repo";
}

cmake("absl", x) {
    x.sources_dir = "repo";
    x.vars["ABSL_ENABLE_INSTALL"] = "ON";
    x.vars["ABSL_BUILD_TESTING"]  = "OFF";
    x.vars["ABSL_PROPAGATE_CXX_STD"] = "ON";

    if (x.cfg["cxx_asan"] != "")
        x.vars["CMAKE_CXX_FLAGS"] += " -DADDRESS_SANITIZER";
    else if (x.cfg["cxx_lsan"] != "")
        x.vars["CMAKE_CXX_FLAGS"] += " -DLEAK_SANITIZER";

    x.outputs = {
        "lib64/libabsl_bad_any_cast_impl.a", 
        "lib64/libabsl_bad_optional_access.a", 
        "lib64/libabsl_bad_variant_access.a", 
        "lib64/libabsl_base.a", 
        "lib64/libabsl_city.a", 
        "lib64/libabsl_civil_time.a", 
        "lib64/libabsl_cord.a", 
        "lib64/libabsl_cord_internal.a", 
        "lib64/libabsl_cordz_functions.a", 
        "lib64/libabsl_cordz_handle.a", 
        "lib64/libabsl_cordz_info.a", 
        "lib64/libabsl_cordz_sample_token.a", 
        "lib64/libabsl_crc32c.a", 
        "lib64/libabsl_crc_cord_state.a", 
        "lib64/libabsl_crc_cpu_detect.a", 
        "lib64/libabsl_crc_internal.a", 
        "lib64/libabsl_debugging_internal.a", 
        "lib64/libabsl_decode_rust_punycode.a", 
        "lib64/libabsl_demangle_internal.a", 
        "lib64/libabsl_demangle_rust.a", 
        "lib64/libabsl_die_if_null.a", 
        "lib64/libabsl_examine_stack.a", 
        "lib64/libabsl_exponential_biased.a", 
        "lib64/libabsl_failure_signal_handler.a", 
        "lib64/libabsl_flags_commandlineflag.a", 
        "lib64/libabsl_flags_commandlineflag_internal.a", 
        "lib64/libabsl_flags_config.a", 
        "lib64/libabsl_flags_internal.a", 
        "lib64/libabsl_flags_marshalling.a", 
        "lib64/libabsl_flags_parse.a", 
        "lib64/libabsl_flags_private_handle_accessor.a", 
        "lib64/libabsl_flags_program_name.a", 
        "lib64/libabsl_flags_reflection.a", 
        "lib64/libabsl_flags_usage.a", 
        "lib64/libabsl_flags_usage_internal.a", 
        "lib64/libabsl_graphcycles_internal.a", 
        "lib64/libabsl_hash.a", 
        "lib64/libabsl_hashtablez_sampler.a", 
        "lib64/libabsl_int128.a", 
        "lib64/libabsl_kernel_timeout_internal.a", 
        "lib64/libabsl_leak_check.a", 
        "lib64/libabsl_log_entry.a", 
        "lib64/libabsl_log_flags.a", 
        "lib64/libabsl_log_globals.a", 
        "lib64/libabsl_log_initialize.a", 
        "lib64/libabsl_log_internal_check_op.a", 
        "lib64/libabsl_log_internal_conditions.a", 
        "lib64/libabsl_log_internal_fnmatch.a", 
        "lib64/libabsl_log_internal_format.a", 
        "lib64/libabsl_log_internal_globals.a", 
        "lib64/libabsl_log_internal_log_sink_set.a", 
        "lib64/libabsl_log_internal_message.a", 
        "lib64/libabsl_log_internal_nullguard.a", 
        "lib64/libabsl_log_internal_proto.a", 
        "lib64/libabsl_log_severity.a", 
        "lib64/libabsl_log_sink.a", 
        "lib64/libabsl_low_level_hash.a", 
        "lib64/libabsl_malloc_internal.a", 
        "lib64/libabsl_periodic_sampler.a", 
        "lib64/libabsl_poison.a", 
        "lib64/libabsl_random_distributions.a", 
        "lib64/libabsl_random_internal_distribution_test_util.a", 
        "lib64/libabsl_random_internal_platform.a", 
        "lib64/libabsl_random_internal_pool_urbg.a", 
        "lib64/libabsl_random_internal_randen.a", 
        "lib64/libabsl_random_internal_randen_hwaes.a", 
        "lib64/libabsl_random_internal_randen_hwaes_impl.a", 
        "lib64/libabsl_random_internal_randen_slow.a", 
        "lib64/libabsl_random_internal_seed_material.a", 
        "lib64/libabsl_random_seed_gen_exception.a", 
        "lib64/libabsl_random_seed_sequences.a", 
        "lib64/libabsl_raw_hash_set.a", 
        "lib64/libabsl_raw_logging_internal.a", 
        "lib64/libabsl_scoped_set_env.a", 
        "lib64/libabsl_spinlock_wait.a", 
        "lib64/libabsl_stacktrace.a", 
        "lib64/libabsl_status.a", 
        "lib64/libabsl_statusor.a", 
        "lib64/libabsl_strerror.a", 
        "lib64/libabsl_str_format_internal.a", 
        "lib64/libabsl_strings.a", 
        "lib64/libabsl_strings_internal.a", 
        "lib64/libabsl_string_view.a", 
        "lib64/libabsl_symbolize.a", 
        "lib64/libabsl_synchronization.a", 
        "lib64/libabsl_throw_delegate.a", 
        "lib64/libabsl_time.a", 
        "lib64/libabsl_time_zone.a", 
        "lib64/libabsl_utf8_for_code_point.a", 
        "lib64/libabsl_vlog_config_internal.a"
    };
} //cmake("absl")

alias("abseil-cpp", x) {
    x.actual_label = ":absl";
}