#include <string>



#ifdef _WIN32

    #ifndef NOMINMAX
        #define NOMINMAX
        #define NOMINMAX_DEFINED_OUTSIDE_DLLLOADER
    #endif
    #include <Libloaderapi.h>
    #include <errhandlingapi.h>
    // #include <winbase.h>
    #ifndef NOMINMAX_DEFINED_OUTSIDE_DLLLOADER
        #undef NOMINMAX
    #endif

    namespace cgn {
    struct DLHelper {
        DLHelper(const std::string &file);
        ~DLHelper();

        bool valid() const { return m_ptr != 0; }
        operator bool() const { return m_ptr != 0; }
        
    private:
        HMODULE m_ptr = 0;
    };
    }

#else

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

#endif

