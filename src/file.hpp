#ifndef DWM_FILE_HPP
#define DWM_FILE_HPP


#include <unistd.h>
#include <ut/resource/resource.hpp>

#include <cstdio>
#include <memory>

struct FileCloser {
    void operator()(FILE *fp) {
        (void)fclose(fp);
    }
};

struct FDCloser {
    void operator()(int fd) {
        close(fd);
    }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;
using FDPtr = ut::Resource<int, FDCloser>;

#endif  // DWM_FILE_HPP
