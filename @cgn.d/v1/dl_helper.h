#include <string>

#ifdef _WIN32
    #include "../pe_loader/pe_file.h"
    namespace cgnv1 {
    struct DLHelper {
        DLHelper(const std::string &file);
        ~DLHelper();

        bool valid() const { return hnd; }
        operator bool() const { return hnd; }
        
    private:
        GlobalSymbol::DllHandle hnd;
    };
    }

#else

    namespace cgnv1 {
    struct DLHelper {
        DLHelper(const std::string &file);
        ~DLHelper();

        bool valid() const { return m_ptr; }
        operator bool() const { return m_ptr; }
    private:
        void *m_ptr = nullptr;
    };
    }

#endif

