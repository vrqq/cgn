#include <chrono>
#include <thread>
#include "../ninjabuild/src/line_printer.h"
#include "../cgn.h"

int dev_helper()
{
    LinePrinter p;
    p.set_smart_terminal(true);

    p.Print("Hello world!", p.ELIDE);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    p.Print("Hello world!!!!", p.ELIDE);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    p.PrintOnNewLine("NewLine Data\n");
    p.PrintOnNewLine("NewLine Data2\n");
    p.PrintOnNewLine("NewLine Data3\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    p.PrintOnNewLine("Second Line Dataaaaaaa\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    p.Print("Normal print", p.ELIDE);


    return 0;
}