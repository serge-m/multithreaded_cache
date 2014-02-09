#include <string>
//#include <cstdlib>
#include <mutex>
#include <iostream>
#include <thread>
#include "thread_safe_map.h"
#include <random>

#include "threadsafe_cache_exception.h"


template<typename Key, typename Value>
class worker
{
    ///  онстанты
    static const unsigned percentRead = 50;     // чтений среди всех вызовов. 
    static const unsigned percentAll = 100;

    static const int maxKey = 20;               /// максимальный айдишник в случае если ключ - целочисленный - int

    typedef enum
    {
        ACTION_READ = 0,
        ACTION_WRITE,
        ACTION_EXCEPTION,
    } WorkerAction;


    /// ƒанные
    int id_;                                                                /// номер треда
    threadsafe_cache::threadsafe_lookup_table<Key, Value> &lookuptable_;    /// ссылка на кэш
    std::mutex &cout_mutex_;                                                /// мьютекс дл€ дебажного вывода
    
    std::random_device rd_;                                                 /// пачка переменных дл€ генерации случайного контента
    std::default_random_engine generator_;
    std::uniform_int_distribution<int> distributionActions_;
    std::uniform_int_distribution<int> distributionID_;                     /// нужно дл€ генерации целочисленных ключей 

    

    /// генерируем, что будем делать (читать/писать)
    WorkerAction generate_action()
    {


        int random_number = distributionActions_(generator_);
        //if (random_number < 30) return ACTION_EXCEPTION;
        return (random_number < percentRead) ? ACTION_READ : ACTION_WRITE;
    }

    /// генерируем случайныйи ключ
    Key generate_key();

    /// генерируем случайный текст с заданным допустимым диапазоном длинны
    std::string generate_random_text(const int minPossibleTextLength = 1, const int maxPossibleTextLength = 10 )
    {
        //std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(minPossibleTextLength, maxPossibleTextLength);

        size_t length = distribution(generator_);

        auto randchar = []() -> char
        {
            const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
            const size_t max_index = (sizeof(charset)-1);
            return charset[rand() % max_index];
        };

        std::string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;

    }

public:

    worker() = delete;

    /// запретить копирование
    worker(worker const &) = delete;
    worker const & operator=(worker const &) = delete;

    
    worker( int id, threadsafe_cache::threadsafe_lookup_table<Key, Value> &lookuptable, std::mutex & cout_mutex)
        : id_(id)
        , lookuptable_( lookuptable )
        , cout_mutex_( cout_mutex )
        , rd_()
        , generator_( rd_() )
        , distributionActions_(0, percentAll)
        , distributionID_(0, maxKey )
    {
        /*int s = std::this_thread::get_id();
        generator_.seed(s);*/
    }


    /// выполн€ем работу
    void action()
    {
        WorkerAction action = generate_action();
        Key key = generate_key();
        switch (action)
        {
        case ACTION_READ:
            read_and_process(key);
            break;
        case ACTION_WRITE:
            generate_new_value_and_write(key);
            break;
        default:
            std::string message = "Worker " + std::to_string(get_id()) + " exception";
            {
                std::lock_guard<std::mutex> lock_cout(cout_mutex_);
                std::cout << "thread " << get_id() << " Throwing exception." << std::endl;
            }
            throw threadsafe_cache::threadsafe_cache_exception(message);
            // something goes wrong
            break;
        }
    }

    /// выдает идентификатор данного рабочего
    int get_id()
    {
        //std::this_thread::get_id()
        return id_;
    }

    void read_and_process(Key key)
    {
        Value value = lookuptable_.value_for(key, "");
    }

    void generate_new_value_and_write(Key key)
    {

        const std::string value( generate_random_text() );

        lookuptable_.add_or_update_mapping(key, value);

    }
};


//////////////////////////
/// специализации шаблонов
template<>
int worker<int, std::string>::generate_key()
{
    return distributionID_(generator_);
}

template<>
std::string worker<std::string, std::string>::generate_key()
{
    return generate_random_text(1,3);
}