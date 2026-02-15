#include "../utility/custom_command.cgn.h"

// Perl Interpreter
// for windows os: using "@third_party//perl:perl_win"
// otherwise: using os internal "perl"
custom_command("host_exe", x) {
    auto hostcfg = api.query_config("host_release");
    if (!hostcfg.second)
        return x.opt->set_fail("config 'host_release' not found");
    x.cfg = hostcfg.first;

    std::string perl_exe = std::string{"perl"} + (x.cfg["host_os"] == "win"? ".exe" : "");
    if (x.cfg["perl_interpreter"] == "" || x.cfg["perl_interpreter"] == "auto") {
        if (x.cfg["host_os"] == "win") {
            cgn::CGNTarget perlwin = x.quick_dep_namedcfg("@third_party//perl:perl_win", "host_release", false);
            x.ninja_entry_targets = {perlwin.ninja_entry};
            perl_exe = perlwin.outputs[0];
        }
    }
    else if (x.cfg["perl_interpreter"] != "host")
        return x.opt->set_fail("Unsupported config 'perl_interpreter' : " 
            + x.cfg["perl_interpreter"].string());
    
    
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (mk)
        mk->outputs = {perl_exe};
}

// custom_command("host_exe", x) {
//     cgn::CGNPath perl_exe;
//     auto hostcfg = api.query_config("host_release");
//     if (!hostcfg.second) {
//         x.opt_confirm_error("config 'host_release' not found");
//         return ;
//     }
//     x.cfg = hostcfg.first;
//     x.add_dep(hostcfg.second);

//     if (x.cfg["perl_interpreter"] == "" || x.cfg["perl_interpreter"] == "auto") {
//         perl_exe = cgn::make_path_base_working("perl");
//         if (x.cfg["host_os"] == "win") {
//             cgn::CGNTarget perlwin = x.add_dep("@third_party//perl:perl_win", "host_release");
//             x.watch_inputs = {perl_exe = cgn::make_path_base_working(perlwin.outputs[0])};
//         }
//     }
//     else if (x.cfg["perl_interpreter"] == "host") {
//         perl_exe = cgn::make_path_base_working(std::string{"perl"} 
//                     + (x.cfg["host_os"]=="win"?".exe":""));
//     }
//     else {
//         x.opt_confirm_error("Unsupported config 'perl_interpreter' : " 
//             + x.cfg["perl_interpreter"].string());
//         return ;
//     }
    
//     x.analysis_outputs = {perl_exe};
// }
