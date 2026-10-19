#include <nekoproto/reflect.hpp>
#include "module_shared.hpp"
int header_increment(Shared& shared, SharedTags) {
    return ++nekoproto::Reflect<Shared>::value<0>(shared);
}
