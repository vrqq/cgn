#pragma once
#include <string>
#include "api_export.h"

namespace cgnv1 {
    struct Logger {
        constexpr static const char *BLACK  = "\x1B[30m";
        constexpr static const char *RED    = "\x1B[31m";
        constexpr static const char *GREEN  = "\x1B[32m";
        constexpr static const char *YELLOW = "\x1B[33m";
        constexpr static const char *BLUE   = "\x1B[34m";
        constexpr static const char *PURPLE = "\x1B[35m";
        constexpr static const char *CYAN   = "\x1B[36m";
        constexpr static const char *WHITE  = "\x1B[37m";
        constexpr static const char *RESET  = "\x1B[0m";

        // Format text into specific color
        std::string fmt_color(const std::string &ss, const char *c = GREEN) const;

        template<typename Type1> static std::string 
        fmt_list(const Type1 &ls, const std::string &indent, std::size_t maxlen = 5);

        // Overprints the current line, elides to_print to fit on one line.
        // If in verbose mode, print full text without overprinting previous one.
        void println(const std::string &text) { println("", text); }
        void println(const std::string &title, const std::string &body, 
                     const char *title_color = GREEN);

        // Prints a string on a new line, not overprinting previous output.
        void paragraph(const std::string &text);

        // only print in verbose mode, same as paragraph
        void verbose_paragraph(const std::string &text);
        // void paragraph(const std::string &color_title, const std::string &normal_body);

        // Toggle verbose mode
        void set_verbose(bool enable);
        bool is_verbose() { return _is_verbose; }
        Logger(bool is_verbose = false);
        ~Logger();

    private:
        bool _is_verbose;
        void *line_printer;
    };


    // definition
    namespace logger_detail {
        inline std::string _elem2str(const std::string &in) {
            return in + ", ";
        };
        inline std::string _elem2str(const std::pair<std::string, std::string> &in) {
            return in.first + ": " + in.second + ", ";
        }
    }
    template<typename Type1> std::string Logger::fmt_list(
        const Type1 &ls, const std::string &indent, std::size_t maxlen
    ) {
        using Type = typename std::decay<Type1>::type;
        std::string ss;
        auto iter = ls.begin();
        for (size_t i=0; i<ls.size() && i<maxlen; i++, iter++) {
            if (!ss.empty())
                ss += "\n" + indent;
            ss += logger_detail::_elem2str(*iter);
        }
        ss += "(" + std::to_string(ls.size()) + " element" + (ls.size()>1? "s)":")");
        return ss;
    }
};
