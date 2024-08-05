#include <iostream>
#include "hello.pb.h"

int main() {
    ns1::MsgA msg;
    msg.set_text("hello_world");
    std::cout<<msg.text()<<std::endl;
    return 0;
}