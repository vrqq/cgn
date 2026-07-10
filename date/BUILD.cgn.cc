#include <cgn>

// master Commits on Jan 11, 2026
git("date.git", x) {
    x.repo = "https://github.com/HowardHinnant/date.git";
    x.commit_id = "45d45413ab89eb23b980aceab4c843fcc0cb9d4a";
    x.dest_dir = "repo";
}

cxx_prebuilt("dlheader", x) {
    x.pub.include_dirs = {"repo/include"};
    x.pub.defines = {"AUTO_DOWNLOAD=0", "HAS_REMOTE_API=0", 
        //  "USE_OS_TZDB=0", "USE_BUNDLE_BUF"
    };
    if (x.cfg["os"] != "win")
        x.pub.defines += {"USE_OS_TZDB=1"};
}

void set_data_config(cxx::CxxSourcesContext &x, bool define_dll_export) {
    x.add_dep(":dlheader", cxx::inherit);
    x.include_dirs = {"repo/include"};
    x.defines = {"NOMINMAX"};
    if (define_dll_export) {
        x.defines += {"DATE_BUILD_DLL"};
    
        // -- bad english comment --
        // If we use DATE_USE_DLL here, the binary target who depend on current 
        // one with whatever PRIVATE or INHERIT would seen 'dllimport 
        // date::function()', but the binary (dll/exe) in windows would found 
        // the 'dllexport date::function()' in tz.cpp instead of 'dllexport'. 
        // in other word, in windows, 'dllimport' as here we make define 
        // 'DATA_USE_DLL' should apply outside the BINARY who depend on this target.
        //
        // -- good English comment --
        // If DATE_USE_DLL is set here, a binary (from cxx_shared/executable)
        // depending on this target sees dllimport but still exports tz.cpp.
        // This mismatch happens only on Windows. Define DATE_USE_DLL only in
        // code outside that binary.
        // Note that on Windows, a function w/o dllimport may resolve in current 
        // binary or an external DLL/EXE.
        // x.pub.defines += {"DATE_USE_DLL"};
    }
     
    x.srcs = {"repo/src/tz.cpp"};
}

cxx_sources("dlobject", x) { set_data_config(x, true); }

cxx_sources("static", x) {
    set_data_config(x, false);
}

cxx_shared("shared", x) {
    x.add_dep(":dlheader", cxx::inherit);
    x.add_dep(":dlobject", cxx::private_dep);
}

alias("date", x) {
    x.actual_label = ":static";
}
