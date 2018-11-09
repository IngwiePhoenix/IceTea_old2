#include <iostream>
#include "src/filecache.hpp"

using namespace std;

int main() {
    typedef Filecache<string, string> cache_t;
    cache_t* c = new cache_t("cache.bin");
    c->store["foo"] = "bar";
    c->write();
    delete c;
    cout << "---" << endl;
    cache_t* d = new cache_t("cache.bin");
    cout << "Foo is: " << d->store["foo"] << endl;
    //d->unlink();
    return 0;
}
