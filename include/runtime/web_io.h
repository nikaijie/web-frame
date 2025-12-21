#pragma once
#include <vector>


namespace runtime {

    //web_io读阻塞
    ssize_t web_read(int fd, std::vector<char>& buffer);

    //web_read写阻塞
    ssize_t web_write(int fd, const char* data, size_t len);


}
