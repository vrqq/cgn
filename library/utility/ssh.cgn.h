// SSH Transfer
#pragma once
#ifdef _WIN32
    #ifdef CGN_UTILITY_SSH_IMPL
        #define CGN_UTILITY_SSH  __declspec(dllexport)
    #else
        #define CGN_UTILITY_SSH
    #endif
#else
    #define CGN_UTILITY_SSH __attribute__((visibility("default")))
#endif


#include "../../cgn.h"

struct CGN_UTILITY_SSH SCPTransmit
{
    struct context_type : protected cgn::QuickDepContext
    {
        friend class SCPTransmit;

        const std::string &name;

        cgn::Configuration &cfg;
        
        // cat local_file.txt | ssh user@remote "cat > /home/user/file.txt"
        // ssh user@remote "cat /home/user/file.txt" > local_file.txt
        // bool using_ssh_cat = false;

        bool enable_compress = true;

        // file to send;
        cgn::CGNPathArray local_files;

        // "scp://[user@]host[:port][/path]"
        // For example "scp://user1@example.com:22/dir1"
        std::string remote_path;

        // -i identity_file
        // Selects the file from which the identity (private key) for public key authentication is read.
        cgn::CGNPath identity_file;

        cgn::CGNTarget add_order_deps(const std::string &label, cgn::Configuration &cfg) {
            return quick_dep(label, cfg, false);
        }

        cgn::CGNTarget add_files_from_output(const std::string &label, cgn::Configuration &cfg);

        context_type(cgn::CGNTargetOpt *opt) 
        : cgn::QuickDepContext{opt}, name(opt->name), cfg(opt->cfg) {}
    }; //struct context_type


    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/ssh.cgn.cc"};
    }

    CGN_UTILITY_SSH static void interpret(context_type &x);
};

#define scp_transmit(name, x, ...) CGN_RULE_DEFINE(::SCPTransmit, name, x, ## __VA_ARGS__)
