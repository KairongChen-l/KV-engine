//
// Created by Chen on 25-3-13.
//

#ifndef SKIP_H
#define SKIP_H
#include <fstream>
#include "skiplist.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <mutex>
#include <chrono>
#include <shared_mutex>
#include <thread>
#define STORE_FILE ".\\store\\dumpFile"
//std::mutex mtx;     // mutex for critical section
std::string delimiter = ":";

//Class template to implement node
template<typename K, typename V>
class Node {
public:
    Node() {}
    Node(K k, V v, int ,int=-1);
    ~Node();
    K get_key() const;
    V get_value() const;
    void set_value(V);
    bool is_expired() const;
    Node **forward;    // Linear array to hold pointers to next node of different level
    int node_level;
    std::chrono::steady_clock::time_point expire_time;

private:
    K key;
    V value;
};
template<typename K, typename V>
Node<K, V>::Node(const K k, const V v,  int level, int ttl) {
    this->key = k;
    this->value = v;
    this->node_level = level;
    // level + 1, because array index is from 0 - level
    this->forward = new Node<K, V>*[level+1];
    // Fill forward array with 0(nullptr)
    memset(this->forward, 0, sizeof(Node<K, V>*)*(level+1));
    if(ttl > 0) {
        this->expire_time = std::chrono::steady_clock::now() + std::chrono::seconds(ttl);
    }else {
        this->expire_time = std::chrono::steady_clock::time_point::max(); //永不过期
    }
};

template<typename K, typename V>
Node<K, V>::~Node() {
    delete []forward;
};

template<typename K, typename V>
K Node<K, V>::get_key() const {
    return key;
};

template<typename K, typename V>
V Node<K, V>::get_value() const {
    return value;
};

template<typename K, typename V>
void Node<K, V>::set_value(V value) {
    this->value=value;
};

template<typename K, typename V>
bool Node<K, V>::is_expired() const {
    return std::chrono::steady_clock::now() > expire_time;
}





// Class template for Skip list
template <typename K, typename V>
class SkipList {

public:
    SkipList(int);
    ~SkipList();
    int get_random_level();
    Node<K, V>* create_node(K, V, int,int=-1);
    int insert_element(K, V,int);
    void display_list();
    bool search_element(K);
    void delete_element(K);
    void dump_file();
    void load_file();
    void clear(Node<K,V>*);
    bool update_ttl(K key,int new_ttl);
    int get_remaining_ttl(K key);
    int size();

private:
    mutable std::shared_mutex mtx;
    void get_key_value_from_string(const std::string& str, std::string* key, std::string* value);
    bool is_valid_string(const std::string& str);
    void expired_cleanup();//过期清理线程

private:
    // Maximum level of the skip list
    int _max_level;

    // current level of skip list
    int _skip_list_level;

    // pointer to header node
    Node<K, V> *_header;

    // file operator
    std::ofstream _file_writer;
    std::ifstream _file_reader;

    // skipList current element count
    int _element_count;
};
//后台清理线程
template<typename K, typename V>
void SkipList<K, V>::expired_cleanup() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::unique_lock<std::shared_mutex> lock(mtx); //获取写锁
        Node<K, V> *node = _header->forward[0];
        while(node) {
            if(node->is_expired()) {
                K key = node->get_key();
                lock.unlock();//先释放锁才能删除
                delete_element(key);
                lock.lock();
            }
            node = node->forward[0];//具体的删除策略在delete_element中实现
        }
    }
}
//更新TTL
template<typename K, typename V>
bool SkipList<K, V>::update_ttl(K key, int new_ttl) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    Node<K,V> *current = _header;

    for(int i=_skip_list_level;i>=0;i--) {
        while (current->forward[i] && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if(current && current->key == key) {
        current->expire_time = std::chrono::steady_clock::now()+std::chrono::seconds(new_ttl);
        return true;
    }
    return false;
}

//获取Key剩余TTL
template<typename K, typename V>
int SkipList<K, V>::get_remaining_ttl(K key) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    Node<K, V> *current = _header;
    for(int i=_skip_list_level;i>=0;i--) {
        while (current->forward[i] && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if(current && current->key == key) {
        if(current->is_expired()) {
            delete_element(key);
            return -1;
        }
        return std::chrono::duration_cast<std::chrono::seconds>(current->expire_time-std::chrono::steady_clock::now()).count();
    }
    return -1;
}

// create new node
template<typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int ttl, int level) {
    Node<K, V> *n = new Node<K, V>(k, v, level,ttl);
    return n;
}

// Insert given key and value in skip list
// return 1 means element exists
// return 0 means insert successfully
template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value,int ttl) {
    std::unique_lock<std::shared_mutex> lock(mtx); //独占锁
    Node<K, V> *current = this->_header;
    // create update array and initialize it
    // update is array which put node that the node->forward[i] should be operated later
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));

    // start form highest level of skip list
    for(int i = _skip_list_level; i >= 0; i--) {
        while(current->forward[i] != nullptr && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    // reached level 0 and forward pointer to right node, which is desired to insert key.
    current = current->forward[0];

    // if current node have key equal to searched key, we get it
    if (current != nullptr && current->get_key() == key) {
        std::cout << "key: " << key << ", exists" << std::endl;
        current->set_value(value);
        if(ttl > 0) {
            current->expire_time == std::chrono::steady_clock::now()+std::chrono::seconds(ttl);
        }
        return 1;
    }

    // if current is nullptr that means we have reached to end of the level
    // if current's key is not equal to key that means we have to insert node between update[0] and current node
    if (current == nullptr || current->get_key() != key ) {
        // Generate a random level for node
        int random_level = get_random_level();
        // If random level is greater thar skip list's current level, initialize update value with pointer to header
        if (random_level > _skip_list_level) {
            for (int i = _skip_list_level+1; i < random_level+1; i++) {
                update[i] = _header;
            }
            _skip_list_level = random_level;
        }

        // create new node with random level generated
        Node<K, V>* inserted_node = create_node(key, value, ttl, random_level);

        // insert node
        for (int i = 0; i <= random_level; i++) {
            inserted_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = inserted_node;
        }
        std::cout << "Successfully inserted key:" << key << ", value:" << value << std::endl;
        _element_count ++;
        return 0;
    }
}

// Display skip list
template<typename K, typename V>
void SkipList<K, V>::display_list() {

    std::cout << "\n*****Skip List*****"<<"\n";
    for (int i = 0; i <= _skip_list_level; i++) {
        Node<K, V> *node = this->_header->forward[i];
        std::cout << "Level " << i << ": ";
        while (node != nullptr) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

// Dump data in memory to file
template<typename K, typename V>
void SkipList<K, V>::dump_file() {

    std::cout << "dump_file-----------------" << std::endl;
    _file_writer.open(STORE_FILE,std::ios::app); //开启追加模式
    if(!_file_writer.is_open()) {
        std::cerr << "Error: Cannot open file for writing!" << std::endl;
        return;
    }
    Node<K, V> *node = this->_header->forward[0];
    while (node != nullptr) {
        _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        node = node->forward[0];
    }

    _file_writer.flush();
    _file_writer.close();
}

// Load data from disk
template<typename K, typename V>
void SkipList<K, V>::load_file() {

    _file_reader.open(STORE_FILE);
    std::cout << "load_file-----------------" << std::endl;
    std::string line;
    std::string* key = new std::string();
    std::string* value = new std::string();
    while (getline(_file_reader, line)) {
        get_key_value_from_string(line, key, value);
        if (key->empty() || value->empty()) {
            continue;
        }
        insert_element(*key, *value);
        // std::stringstream ss1(*key);
        // K k;
        // ss1 >> k;
        // std::stringstream ss2(*value);
        // V v;
        // ss2 >> v;
        // insert_element(k, v);
        std::cout << "key:" << *key << "value:" << *value << std::endl;
    }
    delete key;
    delete value;
    _file_reader.close();
}

// Get current SkipList size
template<typename K, typename V>
int SkipList<K, V>::size() {
    return _element_count;
}

template<typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string& str, std::string* key, std::string* value) {

    if(!is_valid_string(str)) {
        return;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter)+1, str.length());
}

template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
}

// Delete element from skip list
template<typename K, typename V>
void SkipList<K, V>::delete_element(K key) {

    std::unique_lock<std::shared_mutex> lock(mtx); //RAII机制还不太熟
    Node<K, V> *current = this->_header;
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] !=nullptr && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];
    if (current != nullptr && current->get_key() == key) {
        // start for lowest level and delete the current node of each level
        for (int i = 0; i <= _skip_list_level; i++) {
            // if at level i, next node is not target node, break the loop.
            if (update[i]->forward[i] != current)
                break;
            update[i]->forward[i] = current->forward[i];
        }
        // Remove levels which have no elements
        while (_skip_list_level > 0 && _header->forward[_skip_list_level] == 0) {
            _skip_list_level --;
        }
        delete current;
        _element_count --;
        std::cout << "Successfully deleted key "<< key << std::endl;
    }
}

// Search for element in skip list
template<typename K, typename V>
bool SkipList<K, V>::search_element(K key) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    std::cout << "search_element-----------------" << std::endl;
    Node<K, V> *current = _header;

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }

    //reached level 0 and advance pointer to right node, which we search
    current = current->forward[0];

    //查询时检查是否过期了，过期了就删除这个键
    if (current and current->get_key() == key) {
        if(current->is_expired()) {
            lock.unlock();//先释放共享锁,否则无法删除
            delete_element(key);
            std::cout << "Key has expired:" << key << std::endl;
            return false;
        }
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }
    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}

// construct skip list
template<typename K, typename V>
SkipList<K, V>::SkipList(int max_level) {
    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;
    this->_header = new Node<K, V>(K(), V(), _max_level);

    //启动后台清理线程
    std::thread cleaner(&SkipList<K,V>::expired_cleanup,this);
    cleaner.detach();//让线程独立运行，不受主线程控制
};

template<typename K, typename V>
SkipList<K, V>::~SkipList() {

    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }
    if(_header->forward[0]!=nullptr) {
        clear(_header->forward[0]);
    }
    delete _header;
}
template<typename K, typename V>
void SkipList<K, V>::clear(Node<K, V> *cur) {
    if(cur->forward[0]!=nullptr) {
        clear(cur->forward[0]);
    }
    delete cur;
}

template<typename K, typename V>
int SkipList<K, V>::get_random_level(){

    int k = 1;
    while ((rand() % 2)) {
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
};

#endif //SKIP_H
