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
#include <string>
#include "thread_safe_map.h"



using namespace std;

std::mutex g_lock;
std::mutex cout_lock;

bool g_finish = false;

threadsafe_lookup_table<int, string> g_lookup_table;

const int g_autosave_timeout = 2000;

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
    Worker worker( id, g_lookup_table, cout_lock );
    while (!g_finish)
    {
        //int time = rand() % 1000;
        /*{
            std::lock_guard<mutex> lock(cout_lock);
            std::cout << "thread " << std::this_thread::get_id() << " start " << std::endl;

        }*/

        //std::this_thread::sleep_for(std::chrono::milliseconds( time ));
        //string s = g_lookup_table.value_for(1, "");
        //g_lookup_table.add_or_update_mapping(2, "asdasd");
        worker.Action();

        /*{
            std::lock_guard<mutex> lock(cout_lock);
            std::cout << "thread " << std::this_thread::get_id() << " end" << std::endl;
        }*/


    }

}

void threadTimeoutSaver()
{
    const bool needDebug = true;
    while (!g_finish)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(g_autosave_timeout));
        
        if (needDebug)
        {
            {
                std::lock_guard<mutex> lock(cout_lock);
                std::cout << "database before save:" << std::endl;
            }
            PrintResultingTable(g_lookup_table.get_map_from_database());

            {
                std::lock_guard<mutex> lock(cout_lock);
                std::cout << "Cache before save:" << std::endl;
            }
            PrintResultingTable(g_lookup_table.get_map());
        }
        
        
        g_lookup_table.save_to_database();
        
        if (needDebug)
        {
            {
                std::lock_guard<mutex> lock(cout_lock);
                std::cout << "database after save:" << std::endl;
            }
            PrintResultingTable(g_lookup_table.get_map_from_database());

            {
                std::lock_guard<mutex> lock(cout_lock);
                std::cout << "Cache after save:" << std::endl;
            }
            PrintResultingTable(g_lookup_table.get_map());
        }

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

    int numWorkers = 20;

    std::vector<std::thread> threads;
    for (int i = 0; i < numWorkers; ++i)
        threads.push_back(std::thread(threadWorkerFunction, i + 1) );
    threads.push_back(std::thread(threadTimeoutSaver));

    string input = "";
    cout << "Enter command (show|exit)";
    while (true)
    {
        bool ended = !getline(cin, input);

        std::lock_guard<mutex> lock(cout_lock);
        cout << "Command: " << input << "\n";


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

        /*if (input.length() == 1)
        {
        myChar = input[0];
        break;
        }*/

        //cout << "Invalid character, please try again" << endl;
    }

    g_finish = true;

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