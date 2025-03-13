//
// Created by Chen on 25-3-13.
//

#ifndef SKIP_H
#define SKIP_H
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstring>

template<typename K,typename V>
class Node {
public:
    Node() {}

    Node(K k,V v,int);

    ~Node();

    K getKey() const;

    V getValue() const;

    Node<K,V> **forward;

    int nodeLevel;
private:
    K key;
    V value;
};

//类内声明，类外实现
template<typename K,typename V>
Node<K,V>::Node(const K k,const V v,int level) {
    std::cout<<"test22222"<<std::endl;
    this->key=k;
    this->value=v;
    this->nodeLevel=level;

    this->forward = new Node<K,V>*[level+1]; //?不懂这个操作

    //把forward的数组填充为0
    memset(this->forward,0,sizeof(Node<K,V>*)*(level+1));
};

template<typename K, typename V>
Node<K, V>::~Node() {
    delete []forward; //有一个疑问，如果forwar存的是指向另外内存的指针会不会内存泄露
}

template<typename K, typename V>
K Node<K, V>::getKey() const {
    return key;
}

template<typename K, typename V>
V Node<K, V>::getValue() const {
    return value;
}

template<typename K, typename V>
class SkipList {
public:
    SkipList(int);
    ~SkipList();
    int randomLevel();

private:
    //跳表的最大层数
    int MAX_LEVEL;
    //目前跳表的层数
    int currentLevel;

    Node<K, V> *header;
};

template<typename K, typename V>
SkipList<K, V>::SkipList(int MAX_LEVEL) {
    this->MAX_LEVEL=MAX_LEVEL;
    this->currentLevel=0;

    //创建头结点，初始化key和value
    std::cout<<"test3333"<<std::endl;
    K k;
    V v;
    this->header=new Node<K,V>(k,v,MAX_LEVEL);
}
template<typename K, typename V>
SkipList<K, V>::~SkipList() {
    //delete header;
}
template<typename K, typename V>
int SkipList<K, V>::randomLevel() {
    int k = 1;
    while(rand()%2) {
        k++;
    }
    k = (k<=MAX_LEVEL ? k : MAX_LEVEL);
    return k;
}


#endif //SKIP_H
