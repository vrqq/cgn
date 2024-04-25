#include "@cgn.d/library/shell.cgn.h"
#include "@cgn.d/library/cxx.cgn.bundle/cxx.cgn.h"

// #pragma message "VALUE: " STR(CGN_VAR_PREFIX)
// #pragma message "VALUE: " CGN_ULABEL_PREFIX

sh_binary("demo", x) {
    x.main = "echo";
    x.args = {"hello-world"};
}

// expand the define below
// =======================
// void _tf_demo(shell::ShellBinary::context_type &x);
// std::shared_ptr<void> _tfreg_demo = api.bind_target_factory<shell::ShellBinary>(
//     "//hello:", "demo", &_tf_demo
// );
// void _tf_demo(shell::ShellBinary::context_type &x) {
//     x.main = "echo";
//     x.args = {"hello-world"};
// }

cxx_executable("cpp0", x) {
    x.srcs = {"cpp0/hellocc.cpp"};
}

// expand the define below
// =======================
// void _tf_cppdemo(cxx::CxxExecutableContext &x);

// std::shared_ptr<void> _tfreg_cppdemo = api.bind_target_factory<
//     cxx::CxxInterpreterIF<cxx::CxxExecutableContext>
// >("//hello:", "cpp", &_tf_cppdemo);

// void _tf_cppdemo(cxx::CxxExecutableContext &x) {
//     x.srcs = {"hellocc.cpp"};
//     using T1 = cxx::CxxExecutableContext;
//     T1 y(x.cfg, cgn::CGNTargetOpt{});
// }