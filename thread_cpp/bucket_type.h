#include <random>
#include <mutex>

namespace threadsafe_cache
{
    template<typename Key, typename Value>
    class bucket_type
    {
    private:
        static const int timeForWaitingMax = 2000; /// ������������ �����, ������� ���� ����� ����� ��������, in milliseconds
        static const int maxProcessingTime = 500; /// ������������ �����, ������� ���� "������������" ������ (��������),in milliseconds

    private:
        typedef std::pair<Key, Value> bucket_value;                 /// ��� ��� ���� <key, value>
        typedef std::list<bucket_value> bucket_data;                /// ������ �� ���  <key, value>
    public:
        typedef typename bucket_data::iterator bucket_iterator;             /// �������� ��� ������
        typedef typename bucket_data::const_iterator bucket_const_iterator; /// ����������� �������� ��� ������
        typedef std::timed_mutex bucket_mutex;                              /// ��� �������� ��� ������ ������
    private:

    public: // �����������, ��� ��� �����. �������� �� private/ ��������� ����������� � map
        bucket_data data;                   /// ������ <���� ��������>
        mutable bucket_mutex mutex;         /// ������� ��� ������ ������ 
        database_connector<Key, Value> &database_;    /// ������ �� ���� ������, ����� ������ ������ ������
        std::mutex & cout_mutex_;           /// ������� ��� ������ �� �����
        std::size_t const idx_;



    private:
        /// ��������� �� ������ � ���� ������� � �������� ������
        // ����������� ������ 
        //bucket_const_iterator find_entry_for(Key const & key) const
        //{
        //    return std::find_if(data.begin(), data.end(),
        //        [&](bucket_value const & item){ return item.first == key;  });
        //}

        // ������������� ������ 
        bucket_iterator find_entry_for(Key const & key)
        {
            return std::find_if(data.begin(), data.end(),
                [&](bucket_value const & item){ return item.first == key;  });
        }
    public:

        bucket_type(std::size_t const idx, database_connector<Key, Value> & database, std::mutex & cout_mutex)
            : idx_(idx)
            , database_(database)
            , cout_mutex_(cout_mutex)

        {}

        void EmulateDelay(Key const & key)
        {
            ///////////////// pause, emulating delay/////////////////////////////////////////
            {
                std::uniform_int_distribution<int> distribution(0, maxProcessingTime);
                std::random_device rd;
                std::default_random_engine e1(rd());
                int sleeptime = distribution(e1);
                /*{
                std::lock_guard<std::mutex> lock(cout_mutex_);
                cout << "Key " + to_string(key) + " sleep start " << sleeptime << " milliseconds" << endl;
                }*/
                std::this_thread::sleep_for(std::chrono::milliseconds(sleeptime));
                /*{
                std::lock_guard<std::mutex> lock(cout_mutex_);
                cout << "Key " + to_string(key) + " sleep finish" << endl;
                }*/
            }
            ///////////////// pause, emulating delay/////////////////////////////////////////
        }

        void echo_mutex_start_waiting(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            std::cout << "Key " << key << ", bucket " << idx_ << ": mutex start waiting" << endl;
        }

        void echo_mutex_finish_waiting(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            std::cout << "Key " << key << ", bucket " << idx_ << ": mutex acquired" << endl;
        }

        void echo_mutex_failed_waiting(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            std::cout << "Key " << key << ", bucket " << idx_ << ": mutex timeout!!!!!!!!!!!!" << endl;
        }

        void echo_mutex_released(Key const &key)
        {
            //auto time = millis_since_midnight();
            std::lock_guard<std::mutex> lock(cout_mutex_);
            //cout << time << " ";
            std::cout << "Key " << key << ", bucket " << idx_ << ": mutex released" << endl;
        }

        void wait_for_mutex(Key const &key)
        {
            echo_mutex_start_waiting(key);
            if (!mutex.try_lock_for(std::chrono::milliseconds(timeForWaitingMax)))
            {
                echo_mutex_failed_waiting(key);
                std::stringstream ss;
                ss << " Timeout exception. Target key: " << key;
                throw threadsafe_cache_exception(ss.str());
            }
        }
        /// ���������� �������� �������� �� �����, ���� ����� ����
        /// ����� ���������� �������� �� ���������
        Value value_for(Key const &key, Value const &default_value)
        {
            //////////////////////////////////////////mutex//////////////////////////////////
            wait_for_mutex(key);
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

                if (!database_.load_data_by_key(key, value))
                {
                    value = default_value;
                    /// ����� ����� ���������� � ���� ������ ��������. �� �� �������, �����
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
            wait_for_mutex(key);
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


    }; //class bucket_type


} // namespace threadsafe_cache
