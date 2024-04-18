#include "@cgn.d/library/shell.cgn.h"

// #pragma message "VALUE:" STR(CGN_VAR_PREFIX)
// #pragma message "VALUE: " CGN_ULABEL_PREFIX

// sh_binary("demo", x) {
//     x.main = "echo";
//     x.args = {"hello-world"};
// }

// expand the define below
// =======================
void _tf_demo(shell::ShellBinary::context_type &x);
std::shared_ptr<void> _tfreg_demo = api.bind_target_factory<shell::ShellBinary>(
    "//hello:", "demo", &_tf_demo
);
void _tf_demo(shell::ShellBinary::context_type &x) {
    x.main = "echo";
    x.args = {"hello-world"};
}
