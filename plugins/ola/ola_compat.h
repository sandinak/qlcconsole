#ifndef OLA_COMPAT_H
#define OLA_COMPAT_H

/*
 * Compatibility header for OLA with C++17
 * 
 * This header provides compatibility for deprecated C++ features
 * that were removed in C++17 but are still used by OLA headers.
 */

#include <memory>
#include <functional>

// Provide std::auto_ptr for C++17 compatibility
// Note: This is a simplified implementation for compatibility only
namespace std {
    #if __cplusplus >= 201703L && !defined(_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR)
    template<typename T>
    class auto_ptr {
    private:
        T* ptr_;
        
    public:
        typedef T element_type;
        
        explicit auto_ptr(T* p = nullptr) : ptr_(p) {}
        
        auto_ptr(auto_ptr& other) : ptr_(other.release()) {}
        
        template<typename U>
        auto_ptr(auto_ptr<U>& other) : ptr_(other.release()) {}
        
        auto_ptr& operator=(auto_ptr& other) {
            if (this != &other) {
                delete ptr_;
                ptr_ = other.release();
            }
            return *this;
        }
        
        template<typename U>
        auto_ptr& operator=(auto_ptr<U>& other) {
            if (this != &other) {
                delete ptr_;
                ptr_ = other.release();
            }
            return *this;
        }
        
        ~auto_ptr() {
            delete ptr_;
        }
        
        T& operator*() const {
            return *ptr_;
        }
        
        T* operator->() const {
            return ptr_;
        }
        
        T* get() const {
            return ptr_;
        }
        
        T* release() {
            T* tmp = ptr_;
            ptr_ = nullptr;
            return tmp;
        }
        
        void reset(T* p = nullptr) {
            if (ptr_ != p) {
                delete ptr_;
                ptr_ = p;
            }
        }
    };
    #endif

    // Provide std::binary_function for C++17 compatibility
    #if __cplusplus >= 201703L && !defined(_LIBCPP_ENABLE_CXX17_REMOVED_BINDERS)
    template<typename Arg1, typename Arg2, typename Result>
    struct binary_function {
        typedef Arg1 first_argument_type;
        typedef Arg2 second_argument_type;
        typedef Result result_type;
    };
    #endif
}

#endif // OLA_COMPAT_H
