#define CGN_UTILITY_IMPL
#include <fstream>
#include <sstream>
#include "custom_command.cgn.h"

void CustomInterpreter::interpret(context_type &x)
{
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    
    if (mk->file_unchanged)
        return ;
    
    // write build.ninja
    cgn::NinjaFile::BuildSection phony;
    phony.outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};
    if (x.ninja_entry_targets.size()) {
        phony.rule = "phony";
        phony.inputs = cgn::NinjaFile::escape_path(x.ninja_entry_targets);
    }
    else {
        std::string rulepath = api.get_filepath("@cgn.d//library/utility/quick_run.ninja");
        mk->ninja->append_include(rulepath);
        phony.rule = (x.cfg["host_os"]=="win")?"win_stamp":"unix_stamp";
    }
    mk->ninja->append_build(phony);
}
