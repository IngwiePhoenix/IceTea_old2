#ifndef THREADING_H
#define THREADING_H

#include "tinythread.h"
#include "fast_mutex.h"

#include <iostream>
#include <list>
#include <map>
#include <vector>

using namespace std;
using namespace tthread;

/**
    @file
    @brief A Threadpool based on Tinythreads++
*/

/// The function to be run across threads.
typedef void (*ThreadFunction)(void*);

/**
    @brief The actual thread pool

    This class requires one template argument and has a second, optional one.

    It is based off TinyThreads++. Upon instantiation, all the threads will be started.
*/
template<typename T, typename ST=void*> class WorkQueue {
public:
    typedef bool (*compare_f)(T,T,WorkQueue<T,ST>*);
private:
    vector<tthread::thread*> threads;
    int count;
    list<T> queue;
    tthread::thread::id myID;
    bool doStop;
    int itemsEverAdded;
    // Misc...
    ST data;
    compare_f compare;

    static bool compare_default(T lh, T rh, WorkQueue<T,ST>* me) { return false; }

public:
    // Synchronisation
    tthread::mutex               mutex;
    tthread::condition_variable  cond;


    inline WorkQueue(int amt, ThreadFunction func, ST udata=NULL, compare_f comp=NULL)
        : count(amt),
          myID(tthread::this_thread::get_id()),
          doStop(false),
          itemsEverAdded(0),
          data(udata) {
        for(int i=0; i<amt; i++) {
            threads.push_back(new tthread::thread(func, (void*)this));
        }
        if(comp == NULL) {
            compare = compare_default;
        } else {
            compare = comp;
        }
    }
    inline ~WorkQueue() {
        for(int i=0; i<count; i++) {
            delete threads[i];
        }
    }
    inline bool stillGoing() {
        if(myID != tthread::this_thread::get_id()) return true;
        bool isGoing=false;
        for(int i=0; i<count; i++) {
            isGoing=threads[i]->joinable();
        }
        return isGoing;
    }
    inline void joinAll() {
        if(myID != tthread::this_thread::get_id()) return;
        for(int i=0; i<count; i++) {
            threads[i]->join();
        }
    }
    inline void detachAll() {
        if(myID != tthread::this_thread::get_id()) return;
        for(int i=0; i<count; i++) {
            threads[i]->detach();
        }
    }
    inline void drain() {
        if(myID != tthread::this_thread::get_id()) return;
        // Just a stub to wait for the data list to flail out again.
        while(size() != 0 && stillGoing()) {
            cond.notify_all();
            //tthread::this_thread::sleep_for(tthread::chrono::seconds(1));
        }
    }
    inline void stop() {
        doStop = true;
        cond.notify_all();
    }
    inline bool hasToStop() {
        // This might end up in a race condition...I am not sure.
        return doStop;
    }

    // Add item to list's end.
    inline void add(T item) {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        bool canAdd=true;
        // Equality check.
        for(typename list<T>::iterator it=queue.begin(); it!=queue.end(); ++it) {
            if(this->compare(*it, item, this)) {
                queue.erase(it);
            }
        }
        queue.push_back(item);
        itemsEverAdded++;
        cond.notify_all();
    }

    // Get item from list's front.
    inline bool remove(T& item) {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        if(doStop) return false;
        //cond.wait<tthread::mutex>(mutex);

        if(!queue.empty()) {
            item = queue.front();
            queue.pop_front();
            return true;
        } else {
            item = NULL;
            return false;
        }
    }

    inline int size() {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        int size = (int)queue.size();
        return size;
    }
    inline int countAll() {
        return itemsEverAdded+1;
    }
    inline void step() {
        cond.notify_one();
    }
    inline ST getData() { return data; }
};

#endif
