//
// 修改与拓展后的跳表实现（KV 存储引擎）
// 作者：Chen（修改版本）
// 日期：2025-03-15
//

#ifndef SKIP_H
#define SKIP_H

#include <fstream>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <mutex>
#include <chrono>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <random>
#include <sstream>
#include <string>
#include <utility>

// 定义存储文件路径
#define STORE_FILE ".\\store\\dumpFile"

// 使用 ":" 作为字段分隔符（注意：key 和 value 中不允许包含 ":"）
std::string delimiter = ":";

// 模板类：节点，包含 key、value 以及指向各层下一个节点的指针数组
template<typename K, typename V>
class Node {
public:
    Node() {}
    // 构造函数：level 为节点层数，ttl 为生存时间（秒），ttl<=0 表示永不过期
    Node(K k, V v, int level, int ttl = -1);
    ~Node();
    K get_key() const;
    V get_value() const;
    void set_value(V);
    bool is_expired() const;
    Node **forward;    // 指针数组，保存各层的下一个节点指针
    int node_level;
    // 过期时间，使用 system_clock（便于持久化）
    std::chrono::system_clock::time_point expire_time;
private:
    K key;
    V value;
};

template<typename K, typename V>
Node<K, V>::Node(const K k, const V v, int level, int ttl) {
    this->key = k;
    this->value = v;
    this->node_level = level;
    // 分配指针数组，数组大小为 level+1（下标 0 ~ level）
    this->forward = new Node<K, V>*[level+1];
    memset(this->forward, 0, sizeof(Node<K, V>*) * (level+1));
    if(ttl > 0) {
        this->expire_time = std::chrono::system_clock::now() + std::chrono::seconds(ttl);
    } else {
        this->expire_time = std::chrono::system_clock::time_point::max(); // 永不过期
    }
}

template<typename K, typename V>
Node<K, V>::~Node() {
    delete []forward;
}

template<typename K, typename V>
K Node<K, V>::get_key() const {
    return key;
}

template<typename K, typename V>
V Node<K, V>::get_value() const {
    return value;
}

template<typename K, typename V>
void Node<K, V>::set_value(V value) {
    this->value = value;
}

template<typename K, typename V>
bool Node<K, V>::is_expired() const {
    return std::chrono::system_clock::now() > expire_time;
}

// 模板类：跳表
template <typename K, typename V>
class SkipList {
public:
    SkipList(int max_level);
    ~SkipList();
    int get_random_level();
    Node<K, V>* create_node(K, V, int, int ttl = -1);
    int insert_element(K, V, int ttl = -1);
    void display_list();
    bool search_element(K);
    void delete_element(K);
    void dump_file();
    void load_file();
    void clear();
    bool update_ttl(K key, int new_ttl);
    int get_remaining_ttl(K key);
    int size();
    // 新增范围查询接口，返回 [start, end] 范围内所有未过期的键值对
    std::vector<std::pair<K, V>> range_query(const K& start, const K& end);
private:
    mutable std::shared_mutex mtx;
    // 解析文件中一行数据，提取 key、value 与 expireTime（Unix 时间戳）
    bool parse_line(const std::string& line, std::string* key, std::string* value, time_t* expire_time);
    bool is_valid_string(const std::string& str);
    void expired_cleanup(); // 后台清理线程
    // 采用迭代方式清空跳表，避免递归释放节点造成栈溢出
    void clear_iterative();
private:
    int _max_level;
    int _skip_list_level;
    Node<K, V> *_header;
    std::ofstream _file_writer;
    std::ifstream _file_reader;
    int _element_count;
    // 随机数生成器，用于随机生成节点层数
    std::mt19937 rng;
    std::uniform_real_distribution<double> dis;
};

// 后台线程：每 5 秒扫描一次跳表，清理所有过期节点
template<typename K, typename V>
void SkipList<K, V>::expired_cleanup() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::vector<K> expired_keys;
        {
            // 使用独占锁遍历跳表，收集过期的 key
            std::unique_lock<std::shared_mutex> lock(mtx);
            Node<K, V>* node = _header->forward[0];
            while(node) {
                if(node->is_expired()){
                    expired_keys.push_back(node->get_key());
                }
                node = node->forward[0];
            }
        }
        // 对收集到的每个过期 key 进行删除（delete_element 内部会获取锁）
        for(const auto& key : expired_keys){
            delete_element(key);
        }
    }
}

// 更新 TTL，更新成功返回 true，否则返回 false
template<typename K, typename V>
bool SkipList<K, V>::update_ttl(K key, int new_ttl) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    Node<K,V>* current = _header;
    for(int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if(current && current->get_key() == key) {
        current->expire_time = std::chrono::system_clock::now() + std::chrono::seconds(new_ttl);
        return true;
    }
    return false;
}

// 获取指定 key 剩余的 TTL（秒），若 key 不存在或已过期返回 -1
template<typename K, typename V>
int SkipList<K, V>::get_remaining_ttl(K key) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    Node<K,V>* current = _header;
    for(int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if(current && current->get_key() == key) {
        if(current->is_expired()){
            lock.unlock();
            delete_element(key);
            return -1;
        }
        return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(current->expire_time - std::chrono::system_clock::now()).count());
    }
    return -1;
}

// 创建新节点
template<typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int ttl, int level) {
    Node<K, V>* n = new Node<K, V>(k, v, level, ttl);
    return n;
}

// 插入元素：若 key 存在则更新，返回 1 表示更新，0 表示插入成功
template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value, int ttl) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    Node<K, V>* current = _header;
    Node<K, V>* update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*) * (_max_level+1));
    // 从最高层开始寻找插入位置
    for(int i = _skip_list_level; i >= 0; i--){
        while(current->forward[i] != nullptr && current->forward[i]->get_key() < key){
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if(current != nullptr && current->get_key() == key) {
        std::cout << "key: " << key << " 已存在，更新其 value" << std::endl;
        current->set_value(value);
        if(ttl > 0) {
            current->expire_time = std::chrono::system_clock::now() + std::chrono::seconds(ttl);
        }
        return 1;
    }
    // 新节点插入
    int random_level = get_random_level();
    if(random_level > _skip_list_level) {
        for (int i = _skip_list_level+1; i <= random_level; i++) {
            update[i] = _header;
        }
        _skip_list_level = random_level;
    }
    Node<K, V>* inserted_node = create_node(key, value, ttl, random_level);
    for (int i = 0; i <= random_level; i++){
        inserted_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = inserted_node;
    }
    std::cout << "成功插入 key: " << key << ", value: " << value << std::endl;
    _element_count++;
    return 0;
}

// 显示跳表中所有节点（逐层打印）
template<typename K, typename V>
void SkipList<K, V>::display_list() {
    std::cout << "\n***** 跳表 *****\n";
    for(int i = 0; i <= _skip_list_level; i++){
        Node<K, V>* node = _header->forward[i];
        std::cout << "Level " << i << ": ";
        while(node != nullptr){
            std::cout << node->get_key() << ":" << node->get_value() << "; ";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

// 将跳表数据写入文件（覆盖写），格式为 key:value:expireTime（Unix 时间戳）
template<typename K, typename V>
void SkipList<K, V>::dump_file() {
    std::cout << "开始 dump_file -----------------" << std::endl;
    _file_writer.open(STORE_FILE, std::ios::trunc);
    if(!_file_writer.is_open()){
        std::cerr << "错误: 无法打开文件写入!" << std::endl;
        return;
    }
    std::shared_lock<std::shared_mutex> lock(mtx);
    Node<K, V>* node = _header->forward[0];
    while(node != nullptr){
        time_t expire_ts = std::chrono::system_clock::to_time_t(node->expire_time);
        _file_writer << node->get_key() << delimiter << node->get_value() << delimiter << expire_ts << "\n";
        node = node->forward[0];
    }
    _file_writer.flush();
    _file_writer.close();
}

// 从文件中加载数据，重建跳表结构
template<typename K, typename V>
void SkipList<K, V>::load_file() {
    _file_reader.open(STORE_FILE);
    std::cout << "开始 load_file -----------------" << std::endl;
    if(!_file_reader.is_open()){
        std::cerr << "错误: 无法打开文件读取!" << std::endl;
        return;
    }
    std::string line;
    std::string key;
    std::string value;
    time_t expire_ts;
    while(getline(_file_reader, line)){
        if(!parse_line(line, &key, &value, &expire_ts)){
            continue;
        }
        // 计算剩余 TTL（秒）
        std::chrono::system_clock::time_point expire_time_point = std::chrono::system_clock::from_time_t(expire_ts);
        int ttl = 0;
        if(expire_time_point == std::chrono::system_clock::time_point::max()){
            ttl = -1;
        } else {
            ttl = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(expire_time_point - std::chrono::system_clock::now()).count());
        }
        if(ttl <= 0 && expire_time_point != std::chrono::system_clock::time_point::max()){
            // 过期数据跳过
            continue;
        }
        insert_element(key, value, ttl);
        std::cout << "加载 key: " << key << ", value: " << value << std::endl;
    }
    _file_reader.close();
}

// 判断字符串是否有效，必须包含分隔符
template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {
    return !str.empty() && str.find(delimiter) != std::string::npos;
}

// 解析文件中一行，格式应为 key:value:expireTime
template<typename K, typename V>
bool SkipList<K, V>::parse_line(const std::string& line, std::string* key, std::string* value, time_t* expire_time) {
    if(!is_valid_string(line)) return false;
    size_t pos1 = line.find(delimiter);
    if(pos1 == std::string::npos) return false;
    *key = line.substr(0, pos1);
    size_t pos2 = line.find(delimiter, pos1 + 1);
    if(pos2 == std::string::npos) return false;
    *value = line.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string expire_str = line.substr(pos2 + 1);
    std::istringstream iss(expire_str);
    iss >> *expire_time;
    return true;
}

// 删除指定 key 对应的节点
template<typename K, typename V>
void SkipList<K, V>::delete_element(K key) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    Node<K, V>* current = _header;
    Node<K, V>* update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*) * (_max_level+1));
    for(int i = _skip_list_level; i >= 0; i--){
        while(current->forward[i] != nullptr && current->forward[i]->get_key() < key){
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if(current != nullptr && current->get_key() == key){
        for(int i = 0; i <= _skip_list_level; i++){
            if(update[i]->forward[i] != current)
                break;
            update[i]->forward[i] = current->forward[i];
        }
        while(_skip_list_level > 0 && _header->forward[_skip_list_level] == nullptr){
            _skip_list_level--;
        }
        delete current;
        _element_count--;
        std::cout << "成功删除 key: " << key << std::endl;
    }
}

// 搜索 key 对应的节点，若找到且未过期则返回 true，否则返回 false
template<typename K, typename V>
bool SkipList<K, V>::search_element(K key) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    std::cout << "开始搜索 key: " << key << std::endl;
    Node<K, V>* current = _header;
    for(int i = _skip_list_level; i >= 0; i--){
        while(current->forward[i] && current->forward[i]->get_key() < key){
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if(current && current->get_key() == key){
        if(current->is_expired()){
            lock.unlock();
            delete_element(key);
            std::cout << "key: " << key << " 已过期" << std::endl;
            return false;
        }
        std::cout << "找到 key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }
    std::cout << "未找到 key: " << key << std::endl;
    return false;
}

// 范围查询接口：返回 [start, end] 范围内所有未过期的键值对
template<typename K, typename V>
std::vector<std::pair<K, V>> SkipList<K, V>::range_query(const K& start, const K& end) {
    std::vector<std::pair<K, V>> result;
    std::shared_lock<std::shared_mutex> lock(mtx);
    Node<K, V>* current = _header;
    // 定位到第一个 key >= start 的节点
    for(int i = _skip_list_level; i >= 0; i--){
        while(current->forward[i] && current->forward[i]->get_key() < start){
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    while(current && current->get_key() <= end){
        if(!current->is_expired()){
            result.push_back({current->get_key(), current->get_value()});
        }
        current = current->forward[0];
    }
    return result;
}

// 返回跳表中当前节点数量
template<typename K, typename V>
int SkipList<K, V>::size() {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return _element_count;
}

// 采用迭代方式清空跳表，避免递归调用导致栈溢出
template<typename K, typename V>
void SkipList<K, V>::clear_iterative() {
    std::unique_lock<std::shared_mutex> lock(mtx);
    Node<K, V>* current = _header->forward[0];
    while(current){
        Node<K, V>* temp = current->forward[0];
        delete current;
        current = temp;
    }
    for (int i = 0; i <= _max_level; i++) {
        _header->forward[i] = nullptr;
    }
    _skip_list_level = 0;
    _element_count = 0;
}

// 公共接口：清空整个跳表
template<typename K, typename V>
void SkipList<K, V>::clear() {
    clear_iterative();
}

// 析构函数：关闭文件流、清空跳表并删除头结点
template<typename K, typename V>
SkipList<K, V>::~SkipList() {
    if(_file_writer.is_open()){
        _file_writer.close();
    }
    if(_file_reader.is_open()){
        _file_reader.close();
    }
    clear_iterative();
    delete _header;
}

// 构造函数：初始化各项参数，并启动后台清理线程
template<typename K, typename V>
SkipList<K, V>::SkipList(int max_level) : _max_level(max_level), _skip_list_level(0), _element_count(0),
    rng(std::random_device{}()), dis(0.0, 1.0)
{
    _header = new Node<K, V>(K(), V(), _max_level);
    // 启动后台清理线程（detach 后独立运行）
    std::thread cleaner(&SkipList<K,V>::expired_cleanup, this);
    cleaner.detach();
}

// 使用随机数引擎生成节点的随机层数（概率 0.5）
template<typename K, typename V>
int SkipList<K, V>::get_random_level() {
    int level = 1;
    while(dis(rng) < 0.5 && level < _max_level){
        level++;
    }
    return level;
}

#endif // SKIP_H
