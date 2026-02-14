#define CGN_LIBRARY_PERL_TESTMOD_IMPL
#include <fstream>
#include <sstream>
#include "test_module.cgn.h"

namespace perl {

void ModuleTestInterpreter::interpret(ModuleTestInterpreter::context_type &x)
{
    // find perl.exe
    cgn::QuickDepContext qdep{x.opt};

    auto perl_target = qdep.quick_dep_namedcfg("@cgn.d//library/perl:host_exe", "host_release", true);
    if (perl_target.outputs.size() == 0)
        return x.opt->set_fail("Perl exe not found");

    // confirm
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    if (!mk->file_unchanged) {
        // generate modtest.pm
        // modtest.pm is input, when it changed, the ninja target would dirty and rebuild.
        // Here we test this case twice:
        //  * first in current cgn.cc file, rewrite modtest.pm if changed
        //  * then in perl script, rewrite .phony stamp file if ${hint} changed.
        std::string hint;
        std::stringstream pmout;
        for (auto name : x.module_names) {
            pmout<<"use " + name + ";\n";
            hint += " " + name;
        }
        pmout<<"\nmy $file = q(" + mk->ninja_entry + ");\n"
             <<"my $str = q(" + hint + ");\n"
             <<R"(
# Step 1: read file if it exists
my $old = "";
if (-e $file) {
    open my $in, '<', $file or die "Cannot open $file: $!";
    local $/;           # slurp mode: read entire file into one scalar
    $old = <$in>;
    close $in;
}

# Step 2: compare content
if ($old eq $str) {
    # print "Content is the same, skipping write (mtime preserved)\n";
} else {
    # print "Content differs, writing new content...\n";
    open my $out, '>', $file or die "Cannot write $file: $!";
    print $out $str;
    close $out;
}
)" "\n";

        std::string modtest_pm_filepath = api.locale_path(mk->out_prefix + "modtest.pm");
        api.write_file_content_if_changed(modtest_pm_filepath, pmout.str());
        mk->ninja_file_appendix += {modtest_pm_filepath};

        //generate ninja entry
        mk->ninja->append_include(api.get_filepath("@cgn.d//library/utility/quick_run.ninja"));
        auto *phony = mk->ninja->append_build();
        phony->rule = "run";
        phony->variables["exe"] = cgn::NinjaFile::escape_path(api.shell_escape(perl_target.outputs[0]));
        phony->variables["desc"] = "Perl module test:" + hint;
        phony->variables["restat"] = "1";
        phony->inputs  = {mk->ninja->escape_path(modtest_pm_filepath)};
        phony->outputs = {mk->ninja->escape_path(api.locale_path(mk->ninja_entry))};
        phony->order_only = {mk->ninja->escape_path(perl_target.ninja_entry)};
    } //endif(!file_unchanged)

    // This is a "test target" and does not affect the "compile target" that depends on it.
    // Current target take effect only if perl exec failed.
    // mk->result.ninja_dep_level = opt->result.NINJA_LEVEL_DYNDEP;
}

} //namespace