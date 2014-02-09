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
#include "bucket_type.h"

namespace threadsafe_cache
{

    

    template<typename Key, typename Value, typename Hash = std::hash<Key> >
    class threadsafe_lookup_table
    {
    private:
        ///////////////////////////////////////////////////////////////////////////
        typedef bucket_type<Key, Value> current_bucket_type;
        ///////////////////////////////////////////////////////////////////////////

        std::vector<std::unique_ptr<current_bucket_type> > buckets; // здесь храним кластеры
        Hash hasher;                                                /// Для вычисления хэша
        database_connector<Key, Value> & database_;                           /// Адаптер базы данных
        std::mutex & cout_mutex_;                                   /// Мьютекс для дебажного вывода

        std::size_t const get_bucket_index(Key const &key) const
        {
            return hasher(key) % buckets.size();
        }

        current_bucket_type &get_bucket(Key const &key) const // находим кластер по ключу
        {
            std::size_t const bucket_index = get_bucket_index(key);
            return *buckets[bucket_index];
        }

    public:

        threadsafe_lookup_table(
            database_connector<Key, Value> & database,
            std::mutex & cout_mutex,
            unsigned num_buckets = 19,
            Hash const &hasher_ = Hash()
            )
            : buckets(num_buckets)
            , hasher(hasher_)
            , database_(database)
            , cout_mutex_(cout_mutex)
        {
            for (unsigned int i = 0; i < num_buckets; ++i)
            {
                buckets[i].reset(new current_bucket_type(i, database_, cout_mutex_)); // выделяем память под кластеры
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

        /// Добавляем элемент или обновляем существующий
        void add_or_update_mapping(Key const &key, Value const &value)
        {
            get_bucket(key).add_or_update_mapping(key, value);
        }

        std::map<Key, Value> get_map() const
        {
            std::vector<std::unique_lock<current_bucket_type::bucket_mutex> > locks;
            for (unsigned int i = 0; i < buckets.size(); ++i)
            {
                locks.push_back(std::unique_lock<current_bucket_type::bucket_mutex>(buckets[i]->mutex));
            }

            std::map<Key, Value> res;
            for (unsigned int i = 0; i < buckets.size(); ++i)
            {
                for (current_bucket_type::bucket_iterator it = buckets[i]->data.begin(); it != buckets[i]->data.end(); ++it)
                {
                    res.insert(*it);
                }
            }
            return res;
        }

        void threadsafe_lookup_table::save_to_database()
        {
            std::vector<std::unique_lock<current_bucket_type::bucket_mutex> > locks;
            for (unsigned int i = 0; i < buckets.size(); ++i)
            {
                locks.push_back(std::unique_lock<current_bucket_type::bucket_mutex>(buckets[i]->mutex));
            }

            std::map<Key, Value> res;
            for (unsigned int i = 0; i < buckets.size(); ++i)
            {
                for (current_bucket_type::bucket_iterator it = buckets[i]->data.begin(); it != buckets[i]->data.end();)
                {
                    database_.save_data(it->first, it->second);
                    it = buckets[i]->data.erase(it);
                }
            }
        }


        /*std::map<Key, Value> threadsafe_lookup_table::get_map_from_database()
        {
            return database_.get_map();
        }*/

        void threadsafe_lookup_table::show_db()
        {
            return database_.show_db();
        }
    };

};


#endif // !THREAD_SAFE_MAP
