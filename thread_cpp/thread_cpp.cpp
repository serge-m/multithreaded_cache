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

void threadWorkerFunction()
{
    Worker worker( g_lookup_table, cout_lock );
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


struct Data
{
    bool isChanged;
    bool isLocked;

};

int main()
{
    srand((unsigned int)time(0));
    std::thread t1(threadWorkerFunction);
    std::thread t2(threadWorkerFunction);
    std::thread t3(threadWorkerFunction);

    string input = "";
    cout << "Please enter 1 char: ";
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
            auto m = g_lookup_table.get_map();
            
            for (auto i = m.begin(); i != m.end(); ++i)
            {
                cout << i->first << " " << i->second << "\n";
            }
            cout << "\n";
        }
        /*if (input.length() == 1)
        {
        myChar = input[0];
        break;
        }*/

        //cout << "Invalid character, please try again" << endl;
    }

    g_finish = true;

    t1.join();
    t2.join();
    t3.join();

    return 0;
}