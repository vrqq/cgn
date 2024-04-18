#include <string>

namespace cgn {

struct DLHelper {
    DLHelper(const std::string &file);
    ~DLHelper();

    bool valid() const { return m_ptr; }
    operator bool() const { return m_ptr; }
private:
    void *m_ptr = nullptr;
};

}
