#include <string>
//#include <cstdlib>
#include <mutex>
#include <iostream>
#include <thread>
#include "thread_safe_map.h"
#include <random>

class WorkerException : public std::exception
{
    std::string message_;
public:
    WorkerException(std::string const & message)
        : message_(message)
    {}

    const char * what() const
    {
        return message_.c_str();
    }



};

template<typename Key, typename Value>
class Worker
{
    /// ���������
    static const unsigned percentRead = 50;
    static const unsigned percentAll = 100;

    static const int maxKey = 20;
    static const int maxSleepTime = 4000;

    typedef enum
    {
        ACTION_READ = 0,
        ACTION_WRITE,
        ACTION_EXCEPTION,

    } WorkerAction;


    /// ������
    int id_; // ����� �����
    threadsafe_lookup_table<Key, Value> &lookuptable_;
    std::mutex &cout_mutex_;
    std::random_device rd_;
    std::default_random_engine generator_;
    std::uniform_int_distribution<int> distributionActions_;
    std::uniform_int_distribution<int> distributionID_;

    


    WorkerAction GenAction()
    {


        int random_number = distributionActions_(generator_);
        //if (random_number < 30) return ACTION_EXCEPTION;
        return (random_number < percentRead) ? ACTION_READ : ACTION_WRITE;
    }

    Key GenID();
    

    std::string GenerateRandomText(const int minPossibleTextLength = 1, const int maxPossibleTextLength = 10 )
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

    Worker() = delete;

    /// ��������� �����������
    Worker(Worker const &) = delete;
    Worker const & operator=(Worker const &) = delete;

    
    Worker( int id, threadsafe_lookup_table<Key, Value> &lookuptable, std::mutex & cout_mutex)
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
        Key key = GenID();
        switch (action)
        {
        case ACTION_READ:
            ReadAndProcess(key);
            break;
        case ACTION_WRITE:
            GenerateNewValueAndWrite(key);
            break;
        default:
            std::string message = "Worker " + std::to_string(GetID()) + " exception";
            {
                std::lock_guard<std::mutex> lock_cout(cout_mutex_);
                std::cout << "thread " << GetID() << " Throwing exception." << std::endl;
            }
            throw WorkerException(message);
            // something goes wrong
            break;
        }
    }

    int GetID()
    {
        //std::this_thread::get_id()
        return id_;
    }

    void ReadAndProcess(Key key)
    {
        /*/// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            std::cout << "thread " << GetID() << " read id: " << id << std::endl;
        }*/

        Value value = lookuptable_.value_for(key, "");
        //int time = rand() % maxSleepTime;

        ///// output 
        //{
        //    std::unique_lock<std::mutex> lock(cout_mutex_);
        //    std::cout << "thread " << GetID() << " read id: " << id << " result: '" << value << "'" << std::endl;
        //    //std::cout << "thread " << std::this_thread::get_id() << " sleeps " << time << " milliseconds" << std::endl;
        //}

        /// sleep, processing emulation
        //std::this_thread::sleep_for(std::chrono::milliseconds( time ));

        /// output 
        //{
        //    std::unique_lock<std::mutex> lock(cout_mutex_);
        //    //std::cout << "thread " << std::this_thread::get_id() << " finished sleep" << std::endl;
        //}

    }

    void GenerateNewValueAndWrite(Key key)
    {
        //int time = rand() % maxSleepTime;

        const std::string value( GenerateRandomText() );

        ///// output 
        //{
        //    std::unique_lock<std::mutex> lock(cout_mutex_);
        //    std::cout << "thread " << GetID() << " Generating data. ID: " << id << std::endl;
        //    //std::cout << "thread " << std::this_thread::get_id() << " sleeps " << time << " milliseconds" << std::endl;
        //}

        ///// sleep, processing emulation
        //std::this_thread::sleep_for(std::chrono::milliseconds(time));

        /*/// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            std::cout << "thread " << GetID() << " Data generated. ID: " << id << " Result: '" << value << "'" << std::endl;
        }*/

        lookuptable_.add_or_update_mapping(key, value);

        /*/// output 
        {
            std::unique_lock<std::mutex> lock(cout_mutex_);
            std::cout << "thread " << GetID() << " Data saved. ID: " << id << " Result: '" << value << "'" << std::endl;
        }*/
        

    }
};


//////////////////////////

template<>
int Worker<int, std::string>::GenID()
{
    return distributionID_(generator_);
}

template<>
std::string Worker<std::string, std::string>::GenID()
{
    return GenerateRandomText(1,3);
}