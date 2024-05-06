#pragma once
// #include <iostream>
#include "../ninjabuild/src/line_printer.h"

namespace cgn {
    struct LogWrapper {
        constexpr static const char *RED    = "31m";
        constexpr static const char *GREEN  = "32m";
        constexpr static const char *YELLOW = "33m";
        constexpr static const char *CYAN   = "36m";
        std::string color(const std::string &ss, const char *c = GREEN) const {
            return printer.supports_color()?
                (std::string{"\x1B["} + c + ss + "\x1B[0m") : ss;
        }
        void paragraph(const std::string &data) {
            // if(data.size() && data.back() != '\n')
            //     data.push_back('\n');
            printer.PrintOnNewLine(data);
        };

        void print(const std::string &data) {
            if (verbose)
                printer.Print(data, printer.FULL);
            else
                printer.Print(data, printer.ELIDE);
        }

        LinePrinter printer;
        bool verbose = false;
    };
    inline LogWrapper logger;
};

// inline auto &logout = std::cout;