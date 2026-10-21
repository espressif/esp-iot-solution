/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "private/elf_symbol.h"

namespace loader_test {
int plain(int n)
{
    return n + 1;
}
int overloaded(int n)
{
    return n + 2;
}
int overloaded(double n)
{
    return static_cast<int>(n) + 3;
}
struct Widget {
    static int run(int n)
    {
        return n + 4;
    }
};
}

using IntFn = int (*)(int);
using DoubleFn = int (*)(double);

static const struct esp_elfsym exports[] = {
    ESP_ELFSYM_EXPORT_NAMED("_ZN11loader_test5plainEi", loader_test::plain),
    ESP_ELFSYM_EXPORT_CPP_OVERLOAD("_ZN11loader_test10overloadedEi", IntFn, loader_test::overloaded),
    ESP_ELFSYM_EXPORT_CPP_OVERLOAD("_ZN11loader_test10overloadedEd", DoubleFn, loader_test::overloaded),
    ESP_ELFSYM_EXPORT_NAMED("_ZN11loader_test6Widget3runEi", loader_test::Widget::run),
    ESP_ELFSYM_END,
};

int main()
{
    assert(std::strcmp(exports[0].name, "_ZN11loader_test5plainEi") == 0);
    assert(exports[0].sym == reinterpret_cast<const void *>(&loader_test::plain));
    assert(exports[1].sym == reinterpret_cast<const void *>(static_cast<IntFn>(&loader_test::overloaded)));
    assert(exports[2].sym == reinterpret_cast<const void *>(static_cast<DoubleFn>(&loader_test::overloaded)));
    assert(exports[3].sym == reinterpret_cast<const void *>(&loader_test::Widget::run));
    assert(exports[4].name == nullptr && exports[4].sym == nullptr);
    assert(loader_test::plain(1) == 2);
    assert(reinterpret_cast<IntFn>(const_cast<void *>(exports[1].sym))(1) == 3);
    assert(reinterpret_cast<DoubleFn>(const_cast<void *>(exports[2].sym))(1.5) == 4);
    return 0;
}
