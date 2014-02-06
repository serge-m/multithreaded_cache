#include <string>
//#include <cstdlib>
#include <mutex>
#include <iostream>
#include <thread>
#include "thread_safe_map.h"
#include <random>
class Worker
{
    /// Константы
    static const unsigned percentRead = 50;
    static const unsigned percentAll = 100;

    static const int maxKey = 20;
    static const int maxSleepTime = 1000;

    typedef enum
    {
        ACTION_READ = 0,
        ACTION_WRITE,
    } WorkerAction;


    /// Данные
    int id_; // номер треда
    threadsafe_lookup_table<int, std::string> &lookuptable_;
    std::mutex &cout_mutex_;
    std::random_device rd_;
    std::default_random_engine generator_;
    std::uniform_int_distribution<int> distributionActions_;
    std::uniform_int_distribution<int> distributionID_;

    


    WorkerAction GenAction()
    {


        int random_number = distributionActions_(generator_);
        return (random_number < percentRead) ? ACTION_READ : ACTION_WRITE;
    }

    int GenID()
    {
        return distributionID_(generator_);
    }

    std::string GenerateRandomText()
    {
        const int maxPossibleTextLength = 10;
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, maxPossibleTextLength);

        size_t length = distribution(generator);

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

    Worker() = delete;

    /// запретить копирование
    Worker(Worker const &) = delete;
    Worker const & operator=(Worker const &) = delete;

    
    Worker( int id, threadsafe_lookup_table<int, std::string> &lookuptable, std::mutex & cout_mutex)
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



    void Action()
    {
        WorkerAction action = GenAction();
        int id = GenID();
        switch (action)
        {
        case ACTION_READ:
            ReadAndProcess(id);
            break;
        case ACTION_WRITE:
            GenerateNewValueAndWrite(id);
            break;
        default:
            // something goes wrong
            break;
        }
    }

    int GetID()
    {
        //std::this_thread::get_id()
        return id_;
    }

    void ReadAndProcess(int id)
    {
        std::string value = lookuptable_.value_for(id, "");
        int time = rand() % maxSleepTime;

        /// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            std::cout << "thread " << GetID() << " read and processing id: " << id << " Data: " << value << std::endl;
            //std::cout << "thread " << std::this_thread::get_id() << " sleeps " << time << " milliseconds" << std::endl;
        }

        /// sleep 
        std::this_thread::sleep_for(std::chrono::milliseconds( time ));

        /// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            //std::cout << "thread " << std::this_thread::get_id() << " finished sleep" << std::endl;
        }

    }

    void GenerateNewValueAndWrite(int id)
    {
        int time = rand() % maxSleepTime;

        const std::string value( GenerateRandomText() );

        /// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            std::cout << "thread " << GetID() << " Generating data. ID: " << id << std::endl;
            //std::cout << "thread " << std::this_thread::get_id() << " sleeps " << time << " milliseconds" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds( time ));

        /// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            std::cout << "thread " << GetID() << " Result: " << value << std::endl;
        }

        lookuptable_.add_or_update_mapping(id, value);
        

        

        /// sleep 
        std::this_thread::sleep_for(std::chrono::milliseconds());
    }
};