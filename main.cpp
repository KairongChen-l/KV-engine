#include <iostream>
#include "skiplist.h"

int main() {
    //std::cout << "Hello, World!" << std::endl;
    SkipList<int,std::string> skipList(6);
    skipList.insertElement(3, "a");
    skipList.insertElement(6, "b");
    skipList.insertElement(7, "c");
    skipList.insertElement(9, "sun");
    skipList.insertElement(12, "xiu");
    skipList.insertElement(19, "yang");

    skipList.displayList();
    skipList.searchElement(9);
    skipList.searchElement(18);

    return 0;
}
