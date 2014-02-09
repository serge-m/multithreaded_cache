//////////////////////////////////////////////////////////////////////////
/// Условие задачи
//////////////////////////////////////////////////////////////////////////
// Имеем базу данных с единственной таблицей :
// records
// {
//     key : varchar
//     data : text
// }
// Имеем многопоточное приложение, которое работает с записями в этой таблице.
// Необходимо реализовать класс - прослойку(кэш) между приложением и базой
// данных со следующими свойствами :
// -Одновременно с одной записью в кэше должен работать только один поток
//  в приложении.
// -Несколько потоков должны иметь возможность работать с несколькими
//  разными записями параллельно.
// -Прослойка должна кэшировать записи и сохранять изменения в базу
//  данных с заданной периодичностью.
// -Если поток запросил запись, которая занята другим потоком, то он
//  должен ждать записи некоторое время, по истечению которого выкинуть исключение.
// -Если поток запросил запись, которой нет в кэше, то кэш должен
//  подгрузить её из базы.
// -Если поток запросил запись, которой нет в базе данных, то кэш должен
//  создать пустую запись в базе с запрашиваемым ключом.
//////////////////////////////////////////////////////////////////////////
/// О решении:
/// создается заданное сичло потоков, которые общаются с кэшем
/// кэш реализуется в виде хеш таблицы. 
/// По умолчанию он довольно маленький, чтобы чаще случались коллизии.
/// Для обработки исключений создана очередь, в которую все рабочие потоки 
/// кидают свои исключения.
/// Есть отдельный тред, который мониторит эту очередь и просто выводит информацию 
/// об исключении.
/// В основном треде идет обработка пользовательского ввода.
/// Для остановки нужно использовать Ctrl+Z<Enter> для Winsows 
/// или Ctrl+D<enter> для linux или команду exit.
/// ввод прерывается дебажным выводом - это криво, но зато без замоорочек реализации.
/// Нормальная обработка Ctrl+C не реализована
/// Для настройки параметров нужно крутить константы в этом файле, а также в bucket_type.h
/// При выходу работа прекращается не сразу а с небольшой задержкой.
/// База данных реализована на SQLite.
/// Из-за того, что кэш реализован через хеш таблицу, бывает что потоки блокируются 
/// даже когда они образаются к разным ключам. Потому что хеш у них может совпасть.
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
/// Нужно ли удалять таблицу перед запуском
const bool g_need_drop_table = true;

/// Интервал времени в милисекундах, через который данные из кэша сбрасываются в базу
const int g_autosave_timeout = 2000;

/// нужно ли печатать содержимое кэша и базы данных при записи в базу
const bool g_show_database_and_cache_on_save = true;

/// Количество тредов-рабочих
const int numWorkers = 30;

//////////////////////////////////////////////////////////////////////////
/// **********************************************************************
/// Таймауты ожидания мьютексов и искусственные задежки при обработке
/// регулируются в bucket_type.h
/// **********************************************************************
//////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
std::mutex g_lock;
std::mutex cout_lock;

bool g_finish = false; /// флаг для оповещения о завершении работы

////////////////////////////////////////////////////////////////////////////////////
std::mutex                       g_exception_mutex; /// мьютекс для очереди исключений
std::vector<std::exception_ptr>  g_exceptions;      /// очередь исключений
std::condition_variable          g_queuecheck;      /// условная переменная для оповещиения об исключениях
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
    g_queuecheck.notify_one(); // на всяякий случай уведомляем обработчик исключений.
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

/// Не знаю, кто должен обрабатывать исключения о таймауте в потоках.
/// Пусть это будет отдельный процесс.
/// Он например, будет просто печатать информацию о исключении
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
    /// Запускаем тред для обработки исключений
    threads.push_back(std::thread(threadExceptionHandler));

    /// Запускаем рабочие треды 
    for (int i = 0; i < numWorkers; ++i)
        threads.push_back(std::thread(threadWorkerFunction, i + 1, &g_lookup_table));

    /// Запускаем тред для периодического сохранения кэша в базу данных
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
    g_queuecheck.notify_one(); // уведомляем обработчик исключений.

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