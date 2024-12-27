// This is the basic demo to show how marco expanded
// -------------------------------------------------
#include "@cgn.d/library/general.cgn.bundle/general.cgn.h"
#include "@cgn.d/library/cxx.cgn.bundle/cxx.cgn.h"

// macros to show defines
// ======================
// for linux
// #pragma message "VALUE: " STR(CGN_VAR_PREFIX)
// #pragma message "VALUE: " CGN_ULABEL_PREFIX

// for windows
// #pragma message("VALUE: " STR(CGN_VAR_PREFIX) )
// #pragma message("VALUE: " CGN_ULABEL_PREFIX )

// for msvc expand test
// void CGN_RULE_TITLE(_tf, CGN_VAR_PREFIX, 28)(ShellBinary::context_type&);
//
// std::shared_ptr<void> CGN_RULE_TITLE(_tfreg, CGN_VAR_PREFIX, 28)
//     = api.bind_target_factory<ShellBinary>(CGN_ULABEL_PREFIX "demo", &CGN_RULE_TITLE(_tf, CGN_VAR_PREFIX, 28));
//
// void CGN_RULE_TITLE(_tf, CGN_VAR_PREFIX, 28)(ShellBinary::context_type& CtxD) {
//     CtxD.cmd_build = {"echo", "hello-world"};
// }


// [CASE 1] expand the define below
// ================================
sh_binary("demo", x) {
    if (x.cfg["os"] == "win")
        x.cmd_build = {"cmd.exe", "/c", "echo", "hello-world"};
    else
        x.cmd_build = {"echo", "hello-world"};
}

// void _tf_demo(ShellBinary::context_type &x);
//
// std::shared_ptr<void> _tfreg_demo = api.bind_target_factory<ShellBinary>(
//     "//hello:", "demo", &_tf_demo
// );
//
// void _tf_demo(ShellBinary::context_type &x) {
//     x.cmd_build = {"echo", "hello-world"};
// }


// [CASE 2] expand the define below
// ================================
cxx_executable("cpp0", x) {
    x.srcs = {"hellocc.cpp"};
}

// void _tf_cppdemo(cxx::CxxExecutableContext &x);
//
// std::shared_ptr<void> _tfreg_cppdemo = api.bind_target_factory<
//     cxx::CxxInterpreterIF<cxx::CxxExecutableContext>
// >("//hello:cpp", &_tf_cppdemo);
//
// void _tf_cppdemo(cxx::CxxExecutableContext &x) {
//     x.srcs = {"hellocc.cpp"};
//     using T1 = cxx::CxxExecutableContext;
//     T1 y(x.cfg, cgn::CGNTargetOpt{});
// }
