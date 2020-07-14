#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace Queue {

    template <typename T> class InternalQueue {
   
    public:
        InternalQueue() = default;

        InternalQueue(int size) : _maxSize(size) {}

        void sendMsg(T msg) {
            std::unique_lock <std::mutex> guard(_mutex);
            int n = _queue.size();
            if (n >= _maxSize) {
                _fullCond.wait(guard);
            }
            _queue.emplace(std::move(msg));
            
            if (n == 0) {
                _emptyCond.notify_one();
                guard.unlock();
            }
        }

        T rcvMsg() {
            std::unique_lock <std::mutex> guard(_mutex);
            int n = _queue.size();
            if (n == 0) {
                _emptyCond.wait(guard);
            }
            T t = _queue.front();
            _queue.pop();
            
            if (n == _maxSize) {
                _fullCond.notify_one();
                guard.unlock();
            }
            return t;
        }

        void rcvAsyncMsg() {
            std::unique_lock <std::mutex> guard(_mutex);
            int n = _queue.size();
            if (n == 0) {
                _emptyCond.wait(guard, std::chrono::milliseconds(500));
            }
            T t;
            if (_queue.size() > 0) {
                t = _queue.front();
                _queue.pop();

                if (n == _maxSize) {
                    _fullCond.notify_one();
                    guard.unlock();
                }
            }
            return t;
        } 

        bool empty() const {
            std::lock_guard <std::mutex> guard(_mutex);
            return _queue.empty();
        }
    
        size_t capacity() const {
            std::lock_guard<std::mutex> guard(_mutex);
            return _maxSize;
        }
    
        size_t size() {
            std::lock_guard <std::mutex> guard(_mutex);
            return _queue.size();
        }

    private:
        std::mutex _mutex;
    
        std::condition_variable _fullCond;
        
        std::condition_variable _emptyCond;

        std::queue <T> _queue;

        int _maxSize {1000};
    };

    template <typename T> class GlobalQueue {
    public:
        static GlobalQueue & getInstance() {
            static GlobalQueue globalQueue;
            return globalQueue;
        }

        void createQueue(std::string qName, int num) {
            std::lock_guard <std::mutex> guard(_mutex);

            typename std::map<std::string, InternalQueue<T> *>::iterator it = qMap.find(qName);
            if (it == qMap.end()) {
                qMap[qName] = new InternalQueue <T>(num);
            }
        }

        void deleteQueue(std::string qName) {
            std::lock_guard <std::mutex> guard(_mutex);
            typename std::map<std::string, InternalQueue<T> *>::iterator it = qMap.find(qName);
            if (it != qMap.end()) {
                delete (it->second);
                qMap.erase(it->first);
            }
        }

        void sendMsg(std::string qName, T msg) {
            typename std::map<std::string, InternalQueue<T> *>::iterator it = qMap.find(qName);
            if (it != qMap.end()) {
                (static_cast <InternalQueue<T> *> (it->second))->sendMsg(msg);
            }
        }

        T rcvMsg(std::string qName) {
            typename std::map<std::string, InternalQueue<T> *>::iterator it = qMap.find(qName);
            if (it != qMap.end()) {
                return (static_cast <InternalQueue<T> *>(it->second))->rcvMsg();
            }
            return T();
        }

        size_t size(std::string qName) {
            typename std::map<std::string, InternalQueue<T> *>::iterator it = qMap.find(qName);
            if (it != qMap.end()) {
                return (static_cast <InternalQueue<T> *>(it->second))->size();
            }
            return -1;
            
        }

    private:
        GlobalQueue() = default;

    private:
        std::mutex _mutex;
        std::map<std::string, InternalQueue<T> *> qMap;
    };
}

