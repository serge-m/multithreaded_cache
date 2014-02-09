#ifndef database_h__
#define database_h__

#include <map>
#include <sqlite3.h>
#include <iostream>
#include <iomanip>
#include <sstream>

//namespace
//{
    static const char * dbname = "t5";
    static const char * id_field_name = "key";
    static const char * value_field_name = "data";
//}

class database_exception : public std::exception
{
private:
    std::string message_;
public:
    database_exception(char const * message)
        : message_(message)
    {

    }

    char const * what() const throw()
    {
        return message_.c_str();
    }

    virtual ~database_exception() throw()
    {
    }

};

struct query_result
{
    bool is_empty;
    std::string value;
};

static int callback_empty(void *data, int argc, char **argv, char **azColName)
{
    /*for (int i = 0; i<argc; i++)
    {
        std::cout 
            << azColName[i] 
            << (argv[i] ? argv[i] : "NULL")
            << '\n';
    }*/
    
    return 0;
}

static int callback_show_dp(void *data, int argc, char **argv, char **azColName)
{
    //for (int i = 0; i<argc; i++)
    {
        std::cout
            << std::setw(10)
            << (argv[0] ? argv[0] : "NULL")
            << " "
            << (argv[1] ? argv[1] : "NULL")
            << "\n";
    }

    return 0;
}

static int callback_with_results(void *data, int argc, char **argv, char **azColName)
{
    query_result * result = static_cast<query_result*>(data);
    if (result != nullptr)
    {
        if (argc != 1)
        {
            result->is_empty = true;
        }
        else
        {
            result->is_empty = false;
            result->value = std::string(argv[0]);
        }
    }

    return 0;
}


class sqlite_connection
{
    sqlite3 *db;
    bool is_opened_;
    std::string message_;


    sqlite_connection(const sqlite_connection &) = delete;
    sqlite_connection& operator=(const sqlite_connection &) = delete;

public:
    sqlite_connection()
        : db(nullptr)
        , is_opened_(false)
    {
        int rc;
        rc = sqlite3_open("database.db", &db);
        if (rc)
        {
            message_ = std::string("Can't open database: ") + sqlite3_errmsg(db);
        }
        else
        {
            is_opened_ = true;
        }

        std::cout << "SQLite threadsafe = " << sqlite3_threadsafe() << "\n";
    }

    std::string get_message() const
    {
        return message_;
    }

    bool is_opened() const
    {
        return is_opened_;
    }

    ~sqlite_connection()
    {
        sqlite3_close(db);
    }

    void execute(char const * sqlRequest, int(*callback)(void*, int, char**, char**), void const * data )
    {
        char *zErrMsg = 0;

        int rc = sqlite3_exec(db, sqlRequest, callback, (void*)data, &zErrMsg);
        if (rc != SQLITE_OK)
        {
            std::string message = std::string("SQL error: ") + zErrMsg;
            sqlite3_free(zErrMsg);
            throw database_exception(message.c_str());
        }
    }
};

template<typename Key, typename Value>
class database_connector
{
private:
    //std::map<Key, Value> database_;
    sqlite_connection connection_;


public:

    //bool LoadDataByKey(const Key & k, Value & result)
    //{
    //    auto it = database_.find(k);
    //    if (it != database_.end())
    //    {
    //        result = it->second;
    //        return true;
    //    }
    //    else
    //    {
    //        return false;
    //    }
    //} 

    //bool WriteData(const Key & key, const Value & value)
    //{
    //    database_[key] = value;
    //    return true;
    //}

    bool load_data_by_key(const Key & k, Value & result)
    {
        std::stringstream request;
        request
            << "select " << value_field_name << " from "
            << dbname 
            << " where " << id_field_name << " = '" << k << "';";
        query_result qresult;
        connection_.execute(request.str().c_str(), callback_with_results, &qresult);

        if (qresult.is_empty)
            return false;

        result = qresult.value;
        return true;
    }

    void save_data(const Key & key, const Value & value)
    {
        std::stringstream request;
        request 
            << "insert or replace into "
            << dbname
            << " ( " << id_field_name << ", " << value_field_name << ") " 
            << "values('" << key << "', '" << value << "');";
        connection_.execute(request.str().c_str(), callback_empty, "Writing data");
    }

    std::map<Key, Value>  get_map()
    {
        return database_;
    }

    void show_db()
    {
        std::stringstream request;
        request
            << "select " << id_field_name << ", " << value_field_name << " from "
            << dbname
            << " order by " << id_field_name << ";";
        connection_.execute(request.str().c_str(), callback_show_dp, nullptr);
    }


    database_connector(const database_connector &) = delete;
    database_connector &operator=(const database_connector &) = delete;

    database_connector( bool need_drop_table )
        : connection_()
        
    {
        stringstream ss;
        
        if (!connection_.is_opened())
        {
            throw database_exception(connection_.get_message().c_str());
        }
        
        std::cout << "Database opened" << "\n";

        
        if (need_drop_table)
        {
            std::cout << "Deleting table..." << "\n";

            std::string sqlDrop(std::string("drop table ") + dbname + ";");
            connection_.execute(sqlDrop.c_str(), callback_empty, "Creating table");
            
            std::cout << "Table is deleted" << "\n";

        }
        
        std::cout << "Creating table..." << "\n";

        stringstream ss_sqlCreate;
        ss_sqlCreate
            << "create table if not exists "
            << dbname
            << " ( " << id_field_name << " varchar, " << value_field_name << " text, primary key(" << id_field_name << "));"
            ;
        connection_.execute(ss_sqlCreate.str().c_str(), callback_empty, "Creating table");
        
        std::cout << "Table is created (or exists)" << "\n";


    }



    ~database_connector()
    {
        std::cout << "Database is closed" << "\n";
    }
};
#endif // database_h__