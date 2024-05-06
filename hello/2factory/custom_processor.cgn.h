#pragma once
#include <cgn>

struct MyStruct {
    std::string cppsrc;
    bool use_custom_define;
};

inline std::vector<std::shared_ptr<void>> my_processor(
    const std::string name1, const std::string name2,
    std::function<void(MyStruct &)> my_factory
) {
    std::string ulabel = name1 + name2;

    // factory 1
    auto cppexe1 = [my_factory](cxx::CxxExecutableContext &x) {
        MyStruct prep;
        my_factory(prep);
        
        x.defines += {"THIS_IS_CPP1"};
        if (prep.use_custom_define)
            x.defines += {"WE_DEFINE_IT"};
        x.srcs = {prep.cppsrc};
    };

    auto cppexe2 = [my_factory](cxx::CxxExecutableContext &x) {
        MyStruct prep;
        my_factory(prep);
        
        x.defines += {"THIS_IS_CPP2"};
        if (prep.use_custom_define)
            x.defines += {"WE_DEFINE_IT"};
        x.srcs = {prep.cppsrc};
    };

    return {
        api.bind_target_factory<cxx::CxxExecutableInterpreter>(
            ulabel, ".1", cppexe1
        ),
        api.bind_target_factory<cxx::CxxExecutableInterpreter>(
            ulabel, ".2", cppexe2
        )
    };
} //my_processor()

#define my_rule(name, x) CGN_FAST_PROCESSOR(my_processor, MyStruct, name, x)