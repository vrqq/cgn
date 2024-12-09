#include "logger.h"
#include "../ninjabuild/src/line_printer.h"

namespace cgnv1 {

std::string Logger::fmt_color(const std::string &ss, const char *c) const
{
    LinePrinter *printer = (LinePrinter*)line_printer;
    return printer->supports_color()? (c + ss + RESET) : ss;
}

void Logger::println(const std::string &title, const std::string &body, const char *title_color)
{
    std::string fulltxt = (title.size()?fmt_color(title, title_color):"") + body;
    if (_is_verbose)
        return paragraph(fulltxt);

    LinePrinter *printer = (LinePrinter*)line_printer;
    printer->Print(fulltxt, printer->ELIDE);
    // printer->Print( fulltxt, (_is_verbose? printer->FULL : printer->ELIDE));
}

void Logger::paragraph(const std::string &text)
{
    LinePrinter *printer = (LinePrinter*)line_printer;
    if (text.empty())
        return ;
    printer->PrintOnNewLine(text + (text.back() != '\n'? "\n":""));
}

void Logger::verbose_paragraph(const std::string &text)
{
    if (_is_verbose)
        paragraph(text);
}

void Logger::set_verbose(bool enable)
{
    LinePrinter *printer = (LinePrinter*)line_printer;
    printer->set_smart_terminal(!(_is_verbose = enable));
}

Logger::Logger(bool _is_verbose) {
    line_printer = new LinePrinter();
    set_verbose(_is_verbose);
}

Logger::~Logger() { delete (LinePrinter*)line_printer; }

} //namespace
