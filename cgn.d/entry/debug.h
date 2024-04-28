#pragma once
// #include <iostream>
#include "../ninjabuild/src/line_printer.h"

namespace cgn {
    struct LogWrapper {
        void paragraph(const std::string &data) {
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