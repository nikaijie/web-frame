#include "src/test/mysql.h"

#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <sys/resource.h>
#include <unistd.h>

#include "src/db/db.h"
#include "runtime/scheduler.h"
#include "runtime/netpoller.h"

int main() {
    mysql_test();
    return 0;
}