#ifndef THREADING_H
#define THREADING_H

#include "tinythread.h"
#include "fast_mutex.h"

#include <iostream>
#include <list>
#include <vector>

using namespace std;
using namespace tthread;

/*
    This is a work queue worker class inspired via: http://vichargrave.com/multithreaded-work-queue-in-c/
    Basically, I am exchanging all pthread calls with tthread. o.o
*/

// Just a short typedef...
typedef void (*ThreadFunction)(void*);

template<typename T> class WorkQueue {
private:
    vector<tthread::thread*> threads;
    int count;
    list<T> queue;
    tthread::thread::id myID;
    bool doStop;

public:
    // Synchronisation
    tthread::mutex               mutex;
    tthread::condition_variable  cond;

    WorkQueue(int amt, ThreadFunction func) : count(amt), myID(tthread::this_thread::get_id()), doStop(false) {
        for(int i=0; i<amt; i++) {
            threads.push_back(new tthread::thread(func, (void*)this));
        }
    }
    ~WorkQueue() {
        for(int i=0; i<count; i++) {
            delete threads[i];
        }
    }
    bool stillGoing() {
        if(myID != tthread::this_thread::get_id()) return true;
        bool isGoing=false;
        for(int i=0; i<count; i++) {
            isGoing=threads[i]->joinable();
        }
        return isGoing;
    }
    void joinAll() {
        if(myID != tthread::this_thread::get_id()) return;
        for(int i=0; i<count; i++) {
            threads[i]->join();
        }
    }
    void detachAll() {
        if(myID != tthread::this_thread::get_id()) return;
        for(int i=0; i<count; i++) {
            threads[i]->detach();
        }
    }
    void drain() {
        if(myID != tthread::this_thread::get_id()) return;
        // Just a stub to wait for the data list to flail out again.
        while(size() != 0) { cond.notify_one(); }
    }
    void stop() {
        doStop = true;
        cond.notify_all();
    }

    // Add item to list's end.
    void add(T item) {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        queue.push_back(item);
        cond.notify_one();
    }

    // Get item from list's front.
    bool remove(T& item) {
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

    int size() {
        tthread::lock_guard<tthread::mutex> guard(mutex);
        int size = queue.size();
        return size;
    }
};

#endif
