//
// Created by Chen on 25-3-13.
//

#ifndef SKIP_H
#define SKIP_H
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <mutex>

std::mutex mtx;  //关键区全局锁

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
    Node<K, V>* createNode(K,V,int);
    void insertElement(K,V);
    void displayList();
    bool searchElement(K);

private:
    //跳表的最大层数
    int MAX_LEVEL;
    //目前跳表的层数
    int skipListLevel;

    Node<K, V> *header;
};

template<typename K, typename V>
//返回一个Node*
Node<K, V>* SkipList<K, V>::createNode(const K k,const V v, int level) {
    Node<K, V> *node = new Node<K, V>(k,v,level);
    return node;
}

template<typename K, typename V>
bool SkipList<K, V>::searchElement(K key) {
    Node<K, V> *current = header;
    //从最顶层开始搜索
    for(int i = skipListLevel;i>=0;i--) {
        //往每一层的前向指针前进
        while(current->forward[i]!=NULL && current->forward[i]->getKey() < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];

    if(current != NULL && current->getKey() == key) {
        std::cout << "Found key:"<<key<<", value:"<<current->getValue()<<std::endl;
        return true;
    }
    std::cout << "Not found key:"<<key<<std::endl;
    return false;
}


template<typename K, typename V>
void SkipList<K, V>::insertElement(const K key,const V value) {
    Node<K, V> *current = this->header;

    Node<K,V> *update[MAX_LEVEL+1];
    memset(update,0,sizeof(Node<K,V>*)*(MAX_LEVEL+1));

    //从最高层的跳表开始,逐层向下找插入位置，每一层沿着前向指针移动，直到找到第一个键大于待插入键的位置，然后记录在update中
    for(int i = skipListLevel; i >= 0; i--) {
        while(current->forward[i] != NULL && current->forward[i]->getKey() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    //到达0层，forward指向右节点
    current = current->forward[0];

    if(current == NULL || current->getKey() != key) {
        int rLevel = randomLevel();
        if(rLevel >= skipListLevel) {
            for(int i = skipListLevel+1; i <= rLevel; i++) {
                update[i] = header;
            }
            skipListLevel = rLevel;
        }

        Node<K, V> *insertNode = createNode(key, value, rLevel);

        for(int i = 0; i<= rLevel;i++) {
            insertNode->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = insertNode;
        }
        std::cout<<"Successfully inserted key:"<<key<<", value:"<<value<<std::endl;
    }
}

template<typename K, typename V>
void SkipList<K, V>::displayList() {
    std::cout<<"\n*****Skip List*****"<<std::endl;
    //打印每一层的节点
    for(int i = 0; i <= skipListLevel; i++) {
        Node<K, V> *current = this->header->forward[i];
        std::cout<<"Level"<<i<<":";
        while(current != NULL) {
            std::cout<<current->getKey()<<"->"<<current->getValue()<<";";
            current = current->forward[i];
        }
        std::cout<<std::endl;
    }
}


template<typename K, typename V>
SkipList<K, V>::SkipList(int MAX_LEVEL) {
    this->MAX_LEVEL=MAX_LEVEL;
    this->skipListLevel=0;

    //创建头结点，初始化key和value
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
