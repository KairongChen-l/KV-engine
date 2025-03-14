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

    skipList.dumFile();
    std::string path = ".\\store\\dumpFile";
    skipList.loadFile(path);
    skipList.displayList();
    return 0;
}
