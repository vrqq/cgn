#include <string>

struct DLHelper {
    DLHelper(const std::string &file);
    ~DLHelper();

    operator bool() { return m_ptr; }
private:
    void *m_ptr = nullptr;
};
