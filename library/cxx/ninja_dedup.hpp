// This a static instance for CxxInterpreter to deduplicate the ninja target
//
// Background
//   When a cxx_source() target has x.cfg['pkg_mode'], the generated ninja build
//   target for compiling .o files is very likely the same as the build.ninja
//   produced without x.cfg['pkg_mode']. (Of course, it can differ if the user
//   removes some source code.)
//   For the same .o file, only the output path differs while everything else is
//   identical. Compiling it twice is wasteful. Therefore, this class uses
//   `obj_regalt()` to register ninja targets that might be reused, and
//   `obj_tryomit()` to attempt matching a previously registered ninja target.
//   It is obvious that the target interpreter calling `obj_tryomit()` must
//   depend on the CGNTarget that calls `obj_regalt()`. The code below does not
//   include extra checks. If checks are enabled, one should add logic like:
//   bool obj_tryomit(caller, alternative, ...) {
//       api.assert_have_adep_edge(early=alternative, late=caller);
//   }
//
// TargetWorker::step31_xxx(), both gcc and msvc
//  gcc case : ppath_out and sect
//  msvc case: gcc case + {field->variable[pdb] has already assigned inside sect so don't input here}
//
#pragma once
#include <cassert>
#include "../../cgn.h"

namespace cxx {

class NinjaDedup {
public:

    // Call from TargetWorker::step31_xxx()
    // @param opt           : the target which do not have cfg["pkg_mode"]
    // @param pvar_path_out : variable 'path_out' in TargetWorker::step31_xxx()
    // @param psect         : variable 'field' in TargetWorker::step31_xxx()
    void obj_regalt(const cgn::CGNTargetOpt *opt, 
        std::string var_path_out, cgn::NinjaFile::BuildSection sect
    ) {
        assert(opt->cfg["pkg_mode"] == "");
        
        ObjDetail detail;
        detail.var_outputs_0 = sect.outputs[0];
        detail.var_variables_pdb = sect.variables["pdb"];
        detail.var_path_out  = var_path_out;
        sect.outputs.clear();
        sect.variables.erase("pdb");
        obj_section_cache[opt->out_prefix + opt->BUILD_ENTRY][sect] = detail;
    } //void obj_regalt()

    // Call from TargetWorker::step31_xxx()
    // @param alternative   : the target may have the object build section we want
    // @param pvar_path_out : variable 'path_out' in TargetWorker::step31_xxx()
    // @param psect         : variable 'field' in TargetWorker::step31_xxx()
    // @return : successful altered for current psect and ppath_out or not
    bool obj_tryomit(const cgn::CGNTarget &alternative, 
        std::string *pvar_path_out, cgn::NinjaFile::BuildSection *psect
    ) {
        assert(alternative.trimmed_cfg["pkg_mode"] == "");
        
        // return false if no alternative target generated
        // there may be happened if user do some judgement like:
        //      do nothing if not pkg_mode, only do compile in pkg_mode
        auto fd1 = obj_section_cache.find(alternative.ninja_entry);
        if (fd1 == obj_section_cache.end())
            return false;

        // Here we known there's some build section record in alternative-target
        // but not sure is it contain the ninja target we want.
        //
        // Find by context of BuildSection, erase field.outputs[] and field.variable['pdb']
        auto fdkey = *psect;
        fdkey.outputs.clear();
        fdkey.variables.erase("pdb");
        auto fd2 = fd1->second.find(fdkey);
        if (fd2 != fd1->second.end()) {
            // complete cgn::NinjaFile::BuildSection then set with var_path_out to user pointer
            auto detail = fd2->second;
            psect->outputs[0] = detail.var_outputs_0;
            if (detail.var_variables_pdb.size())
                psect->variables["pdb"] = detail.var_variables_pdb;
            *pvar_path_out = detail.var_path_out;
            return true;
        }
        return false;
    } // bool obj_tryomit()

private:
    // Used for obj_regalt() and obj_tryomit()
    // map[target.out_prefix][build_section without output] == (string)path_out
    // for sepcific provider:
    //   * cgn::NinjaFile::BuildSection{without outputs[] and variable['pdb']} as index key
    //   * cgn::NinjaFile::BuildSection{value.var_outputs0 and var_pdb_0} + var_path_out as cached value
    struct ObjDetail {
        std::string var_outputs_0;
        std::string var_variables_pdb;
        std::string var_path_out;
    };
    struct BuildSectionHash {
        std::size_t operator()(const cgn::NinjaFile::BuildSection &sect) const {
            if (sect.inputs.size())
                return std::hash<std::string>()(sect.inputs[0]);
            else if (sect.implicit_inputs.size())
                return std::hash<std::string>()(sect.implicit_inputs[0]);
            return std::hash<std::string>()(sect.rule);
        }
    };
    std::unordered_map<std::string, 
        std::unordered_map<cgn::NinjaFile::BuildSection, ObjDetail, BuildSectionHash>
    > obj_section_cache;

}; // class NinjaDedup;

}; //namespace