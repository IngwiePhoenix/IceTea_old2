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

/*
    This is a work queue worker class inspired via: http://vichargrave.com/multithreaded-work-queue-in-c/
    Basically, I am exchanging all pthread calls with tthread. o.o
*/

// Just a short typedef...
typedef void (*ThreadFunction)(void*);

template<typename T, typename ST=void*> class WorkQueue {
private:
    vector<tthread::thread*> threads;
    int count;
    list<T> queue;
    tthread::thread::id myID;
    bool doStop;
    int itemsEverAdded;
    // Misc...
    ST data;

public:
    // Synchronisation
    tthread::mutex               mutex;
    tthread::condition_variable  cond;

    inline WorkQueue(int amt, ThreadFunction func, ST udata=NULL)
        : count(amt),
          myID(tthread::this_thread::get_id()),
          doStop(false),
          itemsEverAdded(0),
          data(udata) {
        for(int i=0; i<amt; i++) {
            threads.push_back(new tthread::thread(func, (void*)this));
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
        while(size() != 0) { cond.notify_one(); }
    }
    inline void stop() {
        doStop = true;
        cond.notify_all();
    }
    inline bool hasToStop() {
        // This might end up in a race condition...I am nto sure.
        return doStop;
    }

    // Add item to list's end.
    inline void add(T item) {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        queue.push_back(item);
        itemsEverAdded++;
        cond.notify_one();
    }

    // Get item from list's front.
    inline bool remove(T& item) {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        while(queue.empty() && doStop != true) {
            cond.wait<tthread::mutex>(mutex);
        }

        if(doStop) return false;

        //if(!queue.empty()) {
            item = queue.front();
            queue.pop_front();
            return true;
        //} else return false;
    }

    inline int size() {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        int size = queue.size();
        return size;
    }
    inline int countAll() {
        return itemsEverAdded;
    }
    inline ST getData() { return data; }
};

#endif
