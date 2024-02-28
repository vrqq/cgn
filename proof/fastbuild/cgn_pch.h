#include "@cgn.d/library/shell.cgn.h"

template<> std::shared_ptr<void> CGN::bind_target_factory<shell::ShellBinary>(
    const std::string &label_prefix, const std::string &name,
    void(*factory)(shell::ShellBinary::context_type&)
);

// template class std::unordered_set<std::string>;

inline void dummy_function() {
    std::string var0 = "echo";
    std::unordered_set<std::string> var = {"dummy"};
}
