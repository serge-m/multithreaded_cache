//////////////////////////////////////////////////////////////////////////////////////////
//> >> Имеем базу данных с единственной таблицей :
//> >> > > > records
//> >> > > > {
//> >> > > >     key : varchar
//> >> > > >     data : text
//> >> > > > }
//> >> > > > Имеем многопоточное приложение, которое работает с записями в этой
//> >> > > таблице.
//> >> > > > Необходимо реализовать класс - прослойку(кэш) между приложением и базой
//> >> > > > данных со следующими свойствами :
//> >> > > >  -Одновременно с одной записью в кэше должен работать только один поток
//> >> > > в
//> >> > > > приложении.
//> >> > > >  -Несколько потоков должны иметь возможность работать с несколькими
//> >> > > > разными записями параллельно.
//> >> > > >  -Прослойка должна кэшировать записи и сохранять изменения в базу
//> >> > > данных с
//> >> > > > заданной периодичностью.
//> >> > > >  -Если поток запросил запись, которая занята другим потоком, то он
//> >> > > должен
//> >> > > > ждать записи некоторое время, по истечению которого выкинуть исключение.
//> >> > > >  -Если поток запросил запись, которой нет в кэше, то кэш должен
//> >> > > подгрузить
//> >> > > > её из базы.
//> >> > > >  -Если поток запросил запись, которой нет в базе данных, то кэш должен
//> >> > > > создать пустую запись в базе с запрашиваемым ключом.
///////////////////////////////////////////////////////////////////////////////////////////

#include "worker.h"

#include <iostream>
#include <chrono>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <string>
#include <sstream>
#include "thread_safe_map.h"



using namespace std;

std::mutex g_lock;
std::mutex cout_lock;

bool g_finish = false;
bool g_need_drop_table = true;
Database<string, string> database(g_need_drop_table);
threadsafe_cache::threadsafe_lookup_table<string, string> g_lookup_table(database, cout_lock);

const int g_autosave_timeout = 2000;

////////////////////////////////////////////////////////////////////////////////////
std::mutex                       g_exception_mutex;
std::vector<std::exception_ptr>  g_exceptions;
std::condition_variable          g_queuecheck;
////////////////////////////////////////////////////////////////////////////////////

void threadFunction()
{
    while (!g_finish)
    {

        std::lock_guard<mutex> lock(cout_lock);
        std::cout << "entered thread " << std::this_thread::get_id() << std::endl;
        //std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 4000));
        std::cout << "leaving thread " << std::this_thread::get_id() << std::endl;


    }

}

template<typename Type>
void PrintResultingTable(const Type &m)
{
    for (auto i = m.begin(); i != m.end(); ++i)
    {
        cout << i->first << " " << i->second << "\n";
    }
    cout << "\n";
}

void threadWorkerFunction( int id )
{
    Worker < std::string, std::string> worker(id, g_lookup_table, cout_lock);
    while (!g_finish)
    {
        try
        {
            worker.Action();
        }
        catch (const std::exception & e)
        {
            threadsafe_cache::thread_timeout_exception e_new( "Thread " + to_string(id) + " error." + e.what() );
            std::lock_guard<std::mutex> lock(g_exception_mutex);
            g_exceptions.push_back(make_exception_ptr(e_new));
            g_queuecheck.notify_one();
        }
        /*catch (...)
        {
            std::lock_guard<std::mutex> lock(g_exception_mutex);
            g_exceptions.push_back(std::current_exception());
            g_queuecheck.notify_one();
        }*/

    }
    g_queuecheck.notify_one(); // на всяякий случай уведомляем обработчик исключений.
}

void threadTimeoutSaver()
{
    const bool needDebug = true;
    while (!g_finish)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(g_autosave_timeout));

        if (needDebug)
        {
            auto m = g_lookup_table.get_map();
            std::lock_guard<mutex> lock(cout_lock);
            g_lookup_table.show_db();
            //PrintResultingTable(g_lookup_table.get_map_from_database());
            std::cout << "Cache before save:" << std::endl;
            PrintResultingTable(m);
        }

        try
        {
            g_lookup_table.save_to_database();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(g_exception_mutex);
            g_exceptions.push_back(std::current_exception());
            g_queuecheck.notify_one();
        }

        if (needDebug)
        {
            auto m = g_lookup_table.get_map();
            std::lock_guard<mutex> lock(cout_lock);
            std::cout << "database after save:" << std::endl;
            g_lookup_table.show_db();
            //PrintResultingTable(g_lookup_table.get_map_from_database());

            std::cout << "Cache after save:" << std::endl;
            PrintResultingTable(m);
        }

    }

}

/// Не знаю, кто должен обрабатывать исключения о таймауте в потоках.
/// Пусть это будет отдельный процесс.
/// Он например, будет прост опечатать информацию о исключении
void threadExceptionHandler()
{

    // до тех пор, пока не будет получен сигнал
    while (!g_finish)
    {
        stringstream ss;

        {
            std::unique_lock<std::mutex> locker(g_exception_mutex);
            while (!g_finish && g_exceptions.empty()) // от ложных пробуждений
                g_queuecheck.wait(locker);
            
            

            /// Если нужно остановиться после первого же исключения
            //g_finish = true; 

            // если есть ошибки в очереди, обрабатывать их
            for (auto &e : g_exceptions)
            {
                try
                {
                    if (e != nullptr)
                        std::rethrow_exception(e);
                }
                catch (const std::exception &e)
                {
                    ss << "Processed exception:" << e.what() << std::endl;
                }
            }
            g_exceptions.clear();
        }


        {
            std::lock_guard<std::mutex> locker_cout(cout_lock);
            cout << ss.str();
        }
        //g_notified = false;
    }

    
}

struct Data
{
    bool isChanged;
    bool isLocked;

};



int main()
{
    //srand((unsigned int)time(0));

    int numWorkers = 3;

    string input = "";
    cout << "Enter command (show|exit|save)" << endl;


    std::vector<std::thread> threads;
    threads.push_back(std::thread(threadExceptionHandler));

    for (int i = 0; i < numWorkers; ++i)
        threads.push_back(std::thread(threadWorkerFunction, i + 1) );
    threads.push_back(std::thread(threadTimeoutSaver));

    while (true)
    {
        bool ended = !getline(cin, input);


        {
            std::lock_guard<mutex> lock(cout_lock);
            cout << "Command: " << input << "\n";
        }


        if (ended || input == "exit")
        {
            break;
        }
        else if (input == "show")
        {
            PrintResultingTable(g_lookup_table.get_map());
        }
        else if (input == "save")
        {
            g_lookup_table.save_to_database();
        }
        else
        {
            std::lock_guard<mutex> lock(cout_lock);
            cout << "ERROR: Unknown command: " << input << "\n";
        }
    }

    g_finish = true;
    g_queuecheck.notify_one(); // уведомляем обработчик исключений.

    for (auto &t : threads)
        t.join();

    PrintResultingTable(g_lookup_table.get_map());
    cout << "-----------------------" << "\n";
    PrintResultingTable(g_lookup_table.get_map_from_database());
    g_lookup_table.save_to_database();
    cout << "-----------------------" << "\n";
    PrintResultingTable(g_lookup_table.get_map_from_database());
    cout << "-----------------------" << "\n";



    return 0;
}