#ifndef THREAD_SAFE_MAP
#define THREAD_SAFE_MAP

//#include <hash_map>
#include <algorithm>
#include <mutex>
#include <vector>
#include <list>
#include <memory>
#include <map>
#include "database.h"

template<typename Key, typename Value, typename Hash = std::hash<Key> >
class threadsafe_lookup_table
{
private:
    ///////////////////////////////////////////////////////////////////////////
    class bucket_type
    {
    private:
        typedef std::pair<Key, Value> bucket_value;
        typedef std::list<bucket_value> bucket_data;
    public:
        typedef typename bucket_data::iterator bucket_iterator;
        typedef typename bucket_data::const_iterator bucket_const_iterator;

    private:
    public: // разобраться, как еще можно. Заменить на private/ Исправить конвертацию в map
        bucket_data data;
        mutable std::mutex mutex;
        Database<Key, Value> &database_;

        
    
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

        bucket_type(Database<Key,Value> & database)
            : database_(database)
        {}

        /// Возвращаем хранимое значение по ключу, если такой есть
        /// Иначе возвращаем значение по умолчанию
        Value value_for(Key const &key, Value const &default_value) 
        {
            std::lock_guard<std::mutex> lock(mutex); // Блокируем данный слот
            bucket_const_iterator const found_entry = find_entry_for(key);
            if (found_entry != data.end())
            {
                return found_entry->second;
            }
            else
            {
                Value value;
                if (!database_.LoadDataByKey(key, value))
                {
                    value = default_value;
                    database_.WriteData(key, default_value); // тут стоит бросать исключение
                }
                data.push_back(bucket_value(key, value));
                return value;
            }
        }

        void add_or_update_mapping(Key const &key, Value const & value)
        {
            std::unique_lock<std::mutex> lock(mutex); // Блокируем данный слот
            bucket_iterator found_entry = find_entry_for(key);
            if (found_entry == data.end())
            {
                data.push_back(bucket_value(key, value));
            }
            else
            {
                found_entry->second = value;
            }
        }

        void remove_mapping(Key const &key) // не используется
        {
            std::unique_lock<std::mutex> lock(mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry != data.end())
            {
                data.erase(found_entry);
            }
        }
    }; //class bucket_type
    ///////////////////////////////////////////////////////////////////////////

    std::vector<std::unique_ptr<bucket_type> > buckets; // здесь храним кластеры
    Hash hasher;
    Database<Key, Value> database_;

    bucket_type &get_bucket(Key const &key) const // находим кластер по ключу
    {
        std::size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }

public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef Hash hash_type;

    threadsafe_lookup_table(
        unsigned num_buckets = 19,
        Hash const &hasher_ = Hash()
        )
        : buckets(num_buckets)
        , hasher(hasher_)
    {
        for (unsigned int i = 0; i < num_buckets; ++i)
        {
            buckets[i].reset(new bucket_type(database_)); // выделяем память под кластеры
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
        std::vector<std::unique_lock<std::mutex> > locks;
        for (unsigned int i = 0; i < buckets.size(); ++i)
        {
            locks.push_back(std::unique_lock<std::mutex>(buckets[i]->mutex));
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
        std::vector<std::unique_lock<std::mutex> > locks;
        for (unsigned int i = 0; i < buckets.size(); ++i)
        {
            locks.push_back(std::unique_lock<std::mutex>(buckets[i]->mutex));
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


};


#endif // !THREAD_SAFE_MAP
