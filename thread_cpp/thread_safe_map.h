#ifndef THREAD_SAFE_MAP
#define THREAD_SAFE_MAP

//#include <hash_map>
#include <algorithm>
#include <mutex>
#include <vector>
#include <list>
#include <memory>
#include <map>


///////DEBUG
#include <chrono>
////////////

#include "database.h"


class thread_timeout_exception : public std::exception
{
    int id_;
    std::string message_;
public:
    thread_timeout_exception(std::string const & message)
        : message_(message)
        , id_(0)
    {}

    const char * what() const
    {
        return message_.c_str();
    }
    const int thread_id() const 
    {
        return id_;
    }
    void set_thread_id( int id ) 
    {
        id_ = id;
    }



};

template<typename Key, typename Value, typename Hash = std::hash<Key> >
class threadsafe_lookup_table
{
private:
    ///////////////////////////////////////////////////////////////////////////
    class bucket_type
    {
    private :
        static const int timeForWaitingMax = 2000; /// in milliseconds
        static const int maxProcessingTime = 1000; /// in milliseconds

    private:
        typedef std::pair<Key, Value> bucket_value;
        typedef std::list<bucket_value> bucket_data;
    public:
        typedef typename bucket_data::iterator bucket_iterator;
        typedef typename bucket_data::const_iterator bucket_const_iterator;
        typedef std::timed_mutex bucket_mutex;
    private:
    public: // разобраться, как еще можно. Заменить на private/ Исправить конвертацию в map
        bucket_data data;
        mutable bucket_mutex mutex;
        Database<Key, Value> &database_;
        std::mutex & cout_mutex_;
        std::size_t const idx_;

        
    
    private:    
        /// Пробегаем по списку и ищем элемент с заданным ключом
        bucket_const_iterator find_entry_for(Key const & key) const
        {
            return std::find_if(data.begin(), data.end(),
                [&](bucket_value const & item){ return item.first == key;  });
        }

        bucket_iterator find_entry_for(Key const & key)
        {
            return std::find_if(data.begin(), data.end(),
                [&](bucket_value const & item){ return item.first == key;  });
        }
    public:

        bucket_type(std::size_t const idx, Database<Key, Value> & database, std::mutex & cout_mutex)
            : idx_(idx)
            , database_(database)
            , cout_mutex_(cout_mutex)

        {}

        void EmulateDelay( Key const & key )
        {
            ///////////////// pause, emulating delay/////////////////////////////////////////
            {
                std::uniform_int_distribution<int> distribution(0, maxProcessingTime);
                std::random_device rd;
                std::default_random_engine e1(rd());
                int sleeptime = distribution( e1 );
                /*{
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    cout << "Key " + to_string(key) + " sleep start " << sleeptime << " milliseconds" << endl;
                }*/
                std::this_thread::sleep_for(std::chrono::milliseconds( sleeptime ));
                /*{
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    cout << "Key " + to_string(key) + " sleep finish" << endl;
                }*/
            }
            ///////////////// pause, emulating delay/////////////////////////////////////////
        }

        //unsigned __int64 millis_since_midnight()
        //{
        //    // current time
        //    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

        //    // get midnight
        //    time_t tnow = std::chrono::system_clock::to_time_t(now);
        //    tm *date = localtime(&tnow);
        //    date->tm_hour = 0;
        //    date->tm_min = 0;
        //    date->tm_sec = 0;
        //    auto midnight = std::chrono::system_clock::from_time_t(mktime(date));

        //    // number of milliseconds between midnight and now, ie current time in millis
        //    // The same technique can be used for time since epoch
        //    return std::chrono::duration_cast<std::chrono::milliseconds>(now - midnight).count();
        //}

        void echo_mutex_start_waiting(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            cout << "Key " << key << ", bucket " << idx_ << ": mutex start waiting" << endl;
        }

        void echo_mutex_finish_waiting(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            cout << "Key " << key << ", bucket " << idx_ << ": mutex acquired" << endl;
        }

        void echo_mutex_failed_waiting(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            cout << "Key " << key << ", bucket " << idx_ << ": mutex timeout!!!!!!!!!!!!" << endl;
        }

        void echo_mutex_released(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            cout << "Key " << key << ", bucket " << idx_ << ": mutex released" << endl;
        }
        /// Возвращаем хранимое значение по ключу, если такой есть
        /// Иначе возвращаем значение по умолчанию
        Value value_for(Key const &key, Value const &default_value) 
        {
            //////////////////////////////////////////mutex//////////////////////////////////
            /// Старый вариант без таймаута 
            //std::lock_guard<bucket_mutex> lock(mutex); // Блокируем данный слот

            /// Новый вариант
            echo_mutex_start_waiting(key);
            if (!mutex.try_lock_for(std::chrono::milliseconds(timeForWaitingMax)))
            {
                echo_mutex_failed_waiting(key);
                stringstream ss;
                ss << " Timeout exception. Target key: " << key;
                throw thread_timeout_exception( ss.str() );
            }
            std::lock_guard<bucket_mutex> lock(mutex, std::adopt_lock);
            echo_mutex_finish_waiting(key);
            //////////////////////////////////////////mutex//////////////////////////////////

            EmulateDelay(key);
           

            bucket_const_iterator const found_entry = find_entry_for(key);
            if (found_entry != data.end())
            {
                echo_mutex_released(key);
                return found_entry->second;
            }
            else
            {
                Value value;
                
                if (!database_.LoadDataByKey(key, value)) 
                {
                    value = default_value;
                    /// Можно сразу записывать в заду пустое значение. Но не понятно, зачем
                    //database_.WriteData(key, default_value); 
                }
                data.push_back(bucket_value(key, value));
                echo_mutex_released(key);
                return value;
            }
        }

        void add_or_update_mapping(Key const &key, Value const & value)
        {
            //////////////////////////////////////////mutex//////////////////////////////////
            /// Старый вариант без таймаута 
            //std::lock_guard<bucket_mutex> lock(mutex); // Блокируем данный слот

            /// Новый вариант
            echo_mutex_start_waiting(key);
            if (!mutex.try_lock_for(std::chrono::milliseconds(timeForWaitingMax)))
            {
                echo_mutex_failed_waiting(key);
                stringstream ss;
                ss << " Timeout exception. Target key: " << key;
                throw thread_timeout_exception(ss.str());
            }
            std::lock_guard<bucket_mutex> lock(mutex, std::adopt_lock);
            //////////////////////////////////////////mutex//////////////////////////////////
            echo_mutex_finish_waiting(key);

            EmulateDelay(key);


            bucket_iterator found_entry = find_entry_for(key);
            if (found_entry == data.end())
            {
                data.push_back(bucket_value(key, value));
            }
            else
            {
                found_entry->second = value;
            }
            echo_mutex_released(key);
        }

        /// не актуальный код
        //void remove_mapping(Key const &key) // не используется
        //{
        //    //////////////////////////////////////////mutex//////////////////////////////////
        //    /// Старый вариант без таймаута 
        //    //std::lock_guard<bucket_mutex> lock(mutex); // Блокируем данный слот

        //    /// Новый вариант
        //    if (!mutex.try_lock_for(std::chrono::milliseconds(timeForWaitingMax)))
        //    {
        //        throw thread_timeout_exception("Timeout exception. Target key: " + key);
        //    }
        //    std::lock_guard<bucket_mutex> lock(mutex, std::adopt_lock);
        //    //////////////////////////////////////////mutex//////////////////////////////////


        //    bucket_iterator const found_entry = find_entry_for(key);
        //    if (found_entry != data.end())
        //    {
        //        data.erase(found_entry);
        //    }
        //}
    }; //class bucket_type
    ///////////////////////////////////////////////////////////////////////////

    std::vector<std::unique_ptr<bucket_type> > buckets; // здесь храним кластеры
    Hash hasher;
    Database<Key, Value> database_;
    std::mutex & cout_mutex_;

    std::size_t const get_bucket_index(Key const &key) const
    {
        return hasher(key) % buckets.size();
    }

    bucket_type &get_bucket(Key const &key) const // находим кластер по ключу
    {
        std::size_t const bucket_index = get_bucket_index(key);
        return *buckets[bucket_index];
    }

public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef Hash hash_type;

    threadsafe_lookup_table(
        std::mutex & cout_mutex,
        unsigned num_buckets = 19,
        Hash const &hasher_ = Hash()
        )
        : buckets(num_buckets)
        , hasher(hasher_)
        , cout_mutex_(cout_mutex)
    {
        for (unsigned int i = 0; i < num_buckets; ++i)
        {
            buckets[i].reset(new bucket_type(i, database_, cout_mutex_)); // выделяем память под кластеры
        }
    }

    // запрещаем копирование
    threadsafe_lookup_table(threadsafe_lookup_table const &) = delete;
    threadsafe_lookup_table &operator=(threadsafe_lookup_table const &) = delete;

    /// Ищем элемент по ключу
    Value value_for(Key const &key, Value const &default_value = Value()) const
    {
        return get_bucket(key).value_for(key, default_value);
    }

    void add_or_update_mapping(Key const &key, Value const &value)
    {
        get_bucket(key).add_or_update_mapping(key, value);
    }

    void remove_mapping(Key const &key)
    {
        get_bucket(key).remove_mapping(key);
    }


    std::map<Key, Value> threadsafe_lookup_table::get_map() const
    {
        std::vector<std::unique_lock<bucket_type::bucket_mutex> > locks;
        for (unsigned int i = 0; i < buckets.size(); ++i)
        {
            locks.push_back(std::unique_lock<bucket_type::bucket_mutex>(buckets[i]->mutex));
        }

        std::map<Key, Value> res;
        for (unsigned int i = 0; i < buckets.size(); ++i)
        {
            for (bucket_type::bucket_iterator it = buckets[i]->data.begin(); it != buckets[i]->data.end(); ++it)
            {
                res.insert(*it);
            }
        }
        return res;
    }

    void threadsafe_lookup_table::save_to_database()
    {
        std::vector<std::unique_lock<bucket_type::bucket_mutex> > locks;
        for (unsigned int i = 0; i < buckets.size(); ++i)
        {
            locks.push_back(std::unique_lock<bucket_type::bucket_mutex>(buckets[i]->mutex));
        }

        std::map<Key, Value> res;
        for (unsigned int i = 0; i < buckets.size(); ++i)
        {
            for (bucket_type::bucket_iterator it = buckets[i]->data.begin(); it != buckets[i]->data.end(); )
            {
                database_.WriteData(it->first, it->second);
                it = buckets[i]->data.erase(it);
            }
        }
    }


    std::map<Key, Value> threadsafe_lookup_table::get_map_from_database()
    {
        return database_.get_map();
    }

    void threadsafe_lookup_table::show_db()
    {
        return database_.show_db();
    }
};


#endif // !THREAD_SAFE_MAP
