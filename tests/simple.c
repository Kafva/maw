#include <criterion/criterion.h>
#include <stdbool.h>

Test(misc, failing) {
    cr_assert(true);
}

Test(misc, passing) {
    cr_assert(true);
}
