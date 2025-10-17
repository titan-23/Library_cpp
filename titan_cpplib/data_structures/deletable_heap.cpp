template<typename T>
class DeletableMinHeap {
private:
    priority_queue<T, vector<T>, greater<T>> hq, lazy;

public:
    DeletableMinHeap() {}
    DeletableMinHeap(vector<T> a) {
        hq = priority_queue<T, vector<T>, greater<T>>(a.begin(), a.end());
    }

    void push(T key) {
        hq.push(key);
    }

    T pop_min() {
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        T key = hq.top();
        hq.pop();
        return key;
    }

    T get_min() {
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        return hq.top();
    }

    void erase(T key) {
        lazy.push(key);
    }

    int len() const {
        return hq.size() - lazy.size();
    }
};


template<typename T>
class DeletableMaxHeap {
private:
    priority_queue<T> hq, lazy;

public:
    DeletableMaxHeap() {}
    DeletableMaxHeap(vector<T> a) {
        hq = priority_queue<T>(a.begin(), a.end());
    }

    void push(T key) {
        hq.push(key);
    }

    T pop_max() {
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        T key = hq.top();
        hq.pop();
        return key;
    }

    T get_max() {
        while (!lazy.empty() && lazy.top() == hq.top()) {
            hq.pop(); lazy.pop();
        }
        return hq.top();
    }

    void erase(T key) {
        lazy.push(key);
    }

    int len() const {
        return hq.size() - lazy.size();
    }
};
