#include <iostream>
#include "skiplist.h"

int main() {

    SkipList<int, std::string> skipList(6);
    skipList.insert_element(1, "apple", 10);
    skipList.insert_element(2, "banana", 5);
    skipList.insert_element(3, "grape", 4);

    skipList.display_list();

    std::this_thread::sleep_for(std::chrono::seconds(6));

    skipList.display_list();
}
