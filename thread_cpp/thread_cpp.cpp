//////////////////////////////////////////////////////////////////////////
/// ������� ������
//////////////////////////////////////////////////////////////////////////
// ����� ���� ������ � ������������ �������� :
// records
// {
//     key : varchar
//     data : text
// }
// ����� ������������� ����������, ������� �������� � �������� � ���� �������.
// ���������� ����������� ����� - ���������(���) ����� ����������� � �����
// ������ �� ���������� ���������� :
// -������������ � ����� ������� � ���� ������ �������� ������ ���� �����
//  � ����������.
// -��������� ������� ������ ����� ����������� �������� � �����������
//  ������� �������� �����������.
// -��������� ������ ���������� ������ � ��������� ��������� � ����
//  ������ � �������� ��������������.
// -���� ����� �������� ������, ������� ������ ������ �������, �� ��
//  ������ ����� ������ ��������� �����, �� ��������� �������� �������� ����������.
// -���� ����� �������� ������, ������� ��� � ����, �� ��� ������
//  ���������� � �� ����.
// -���� ����� �������� ������, ������� ��� � ���� ������, �� ��� ������
//  ������� ������ ������ � ���� � ������������� ������.
//////////////////////////////////////////////////////////////////////////
/// � �������:
/// ��������� �������� ����� �������, ������� �������� � �����
/// ��� ����������� � ���� ��� �������. 
/// �� ��������� �� �������� ���������, ����� ���� ��������� ��������.
/// ��� ��������� ���������� ������� �������, � ������� ��� ������� ������ 
/// ������ ���� ����������.
/// ���� ��������� ����, ������� ��������� ��� ������� � ������ ������� ���������� 
/// �� ����������.
/// � �������� ����� ���� ��������� ����������������� �����.
/// ��� ��������� ����� ������������ Ctrl+Z<Enter> ��� Winsows 
/// ��� Ctrl+D<enter> ��� linux ��� ������� exit.
/// ���� ����������� �������� ������� - ��� �����, �� ���� ��� ���������� ����������.
/// ���������� ��������� Ctrl+C �� �����������
/// ��� ��������� ���������� ����� ������� ��������� � ���� �����, � ����� � bucket_type.h
/// ��� ������ ������ ������������ �� ����� � � ��������� ���������.
/// ���� ������ ����������� �� SQLite.
/// ��-�� ����, ��� ��� ���������� ����� ��� �������, ������ ��� ������ ����������� 
/// ���� ����� ��� ���������� � ������ ������. ������ ��� ��� � ��� ����� ��������.
//////////////////////////////////////////////////////////////////////////

#include "worker.h"

#include <iostream>
#include <iomanip>
#include <chrono>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <string>
#include <sstream>
#include "thread_safe_map.h"
#include "threadsafe_cache_exception.h"


using namespace std;

//////////////////////////////////////////////////////////////////////////
/// ����� �� ������� ������� ����� ��������
const bool g_need_drop_table = true;

/// �������� ������� � ������������, ����� ������� ������ �� ���� ������������ � ����
const int g_autosave_timeout = 2000;

/// ����� �� �������� ���������� ���� � ���� ������ ��� ������ � ����
const bool g_show_database_and_cache_on_save = true;

/// ���������� ������-�������
const int numWorkers = 30;

//////////////////////////////////////////////////////////////////////////
/// **********************************************************************
/// �������� �������� ��������� � ������������� ������� ��� ���������
/// ������������ � bucket_type.h
/// **********************************************************************
//////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
std::mutex g_lock;
std::mutex cout_lock;

bool g_finish = false; /// ���� ��� ���������� � ���������� ������

////////////////////////////////////////////////////////////////////////////////////
std::mutex                       g_exception_mutex; /// ������� ��� ������� ����������
std::vector<std::exception_ptr>  g_exceptions;      /// ������� ����������
std::condition_variable          g_queuecheck;      /// �������� ���������� ��� ����������� �� �����������
////////////////////////////////////////////////////////////////////////////////////


template<typename Type>
void PrintResultingTable(const Type &m)
{
    for (auto i = m.begin(); i != m.end(); ++i)
    {
        cout << setw(10) << i->first << " " << i->second << "\n";
    }
    cout << "\n";
}

void threadWorkerFunction(int id, threadsafe_cache::threadsafe_lookup_table<string, string> *lookup_table)
{
    worker < std::string, std::string> worker(id, *lookup_table, cout_lock);
    while (!g_finish)
    {
        try
        {
            worker.action();
        }
        catch (const std::exception & e)
        {
            threadsafe_cache::threadsafe_cache_exception e_new( "Thread " + to_string(id) + " error." + e.what() );
            std::lock_guard<std::mutex> lock(g_exception_mutex);
            g_exceptions.push_back(make_exception_ptr(e_new));
            g_queuecheck.notify_one();
        }

    }
    g_queuecheck.notify_one(); // �� ������� ������ ���������� ���������� ����������.
}

void threadTimeoutSaver(threadsafe_cache::threadsafe_lookup_table<string, string> *lookup_table)
{
    while (!g_finish)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(g_autosave_timeout));

        if (g_show_database_and_cache_on_save)
        {
            auto m = lookup_table->get_map();
            std::lock_guard<mutex> lock(cout_lock);
            lookup_table->show_db();
            std::cout << "Cache before save:" << std::endl;
            PrintResultingTable(m);
        }

        try
        {
            lookup_table->save_to_database();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(g_exception_mutex);
            g_exceptions.push_back(std::current_exception());
            g_queuecheck.notify_one();
        }

        if (g_show_database_and_cache_on_save)
        {
            auto m = lookup_table->get_map();
            std::lock_guard<mutex> lock(cout_lock);
            std::cout << "database after save:" << std::endl;
            lookup_table->show_db();
            std::cout << "Cache after save:" << std::endl;
            PrintResultingTable(m);
        }

    }

}

/// �� ����, ��� ������ ������������ ���������� � �������� � �������.
/// ����� ��� ����� ��������� �������.
/// �� ��������, ����� ������ �������� ���������� � ����������
void threadExceptionHandler()
{

    // �� ��� ���, ���� �� ����� ������� ������
    while (!g_finish)
    {
        stringstream ss;

        {
            std::unique_lock<std::mutex> locker(g_exception_mutex);
            while (!g_finish && g_exceptions.empty()) // �� ������ �����������
                g_queuecheck.wait(locker);
            
            

            /// ���� ����� ������������ ����� ������� �� ����������
            //g_finish = true; 

            // ���� ���� ������ � �������, ������������ ��
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


int main()
{


    string input = "";

    cout
        << "Cache for database" << endl
        << "This program generates a lot of debug information during work" << endl
        << "To break processing enter 'exit<enter>' or 'Ctrl+Z<enter>' for Windows or 'Ctrl+D<enter>' for linux" << endl
        << "Ctrl+C is not processing correctly" << endl;
    cout
        << "" << endl
        << "Available commands: " << endl
        << "    show - displays contents of database and cache" << endl
        << "    exit - terminates processing" << endl
        << "    save - save cache contents to database" << endl;

    cout
        << "" << endl
        << "Press enter to start" << endl;

    std::getline(cin, (std::string()));


    database_connector<string, string> database(g_need_drop_table);
    threadsafe_cache::threadsafe_lookup_table<string, string> g_lookup_table(database, cout_lock);


    std::vector<std::thread> threads;
    /// ��������� ���� ��� ��������� ����������
    threads.push_back(std::thread(threadExceptionHandler));

    /// ��������� ������� ����� 
    for (int i = 0; i < numWorkers; ++i)
        threads.push_back(std::thread(threadWorkerFunction, i + 1, &g_lookup_table));

    /// ��������� ���� ��� �������������� ���������� ���� � ���� ������
    threads.push_back(std::thread(threadTimeoutSaver, &g_lookup_table));

    while (true)
    {
        bool ended = !std::getline(cin, input);

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
            auto m = g_lookup_table.get_map();
            std::lock_guard<mutex> lock(cout_lock);
            PrintResultingTable(m);
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
    g_queuecheck.notify_one(); // ���������� ���������� ����������.

    for (auto &t : threads)
        t.join();

    PrintResultingTable(g_lookup_table.get_map());
    cout << "-----------------------" << "\n";
    cout << "database state before las save:" << endl;
    g_lookup_table.show_db();
    cout << "-----------------------" << "\n";
    g_lookup_table.save_to_database();
    cout << "Cache saved." << endl;
    cout << "-----------------------" << "\n";
    cout << "final database state:" << endl;
    g_lookup_table.show_db();
    cout << "-----------------------" << "\n";



    return 0;
}