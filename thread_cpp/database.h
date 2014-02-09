#include <map>
#include <sqlite3.h>
#include <iostream>
#include <iomanip>

namespace
{
    const char * dbname = "t5";
}

class DatabaseException : public std::exception
{
private:
    std::string message_;
public:
    DatabaseException(char const * message)
        : message_(message)
    {

    }

};

struct query_result
{
    bool is_empty;
    std::string value;
};

static int callback(void *data, int argc, char **argv, char **azColName){
    int i;
    if (data != nullptr)
    {
        fprintf(stderr, "%s: ", (const char*)data);
    }
    for (i = 0; i<argc; i++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }

    printf("\n");
    return 0;
}

static int callback_show_dp(void *data, int argc, char **argv, char **azColName){
    int i;
    
    //for (i = 0; i<argc; i++)
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

static int my_callback(void *data, int argc, char **argv, char **azColName)
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

        std::cout << sqlite3_threadsafe() << "!!!!!!!!!!!!!!!!!!!\n";
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
            throw DatabaseException(message.c_str());
        }
    }
};

template<typename Key, typename Value>
class Database
{
private:
    std::map<Key, Value> database_;
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

    bool LoadDataByKey(const Key & k, Value & result)
    {
        std::stringstream request;
        request
            << "select value from "
            << dbname 
            << " where id = '"
            << k
            << "';";
        query_result qresult;
        connection_.execute(request.str().c_str(), my_callback, &qresult);

        if (qresult.is_empty)
            return false;

        result = qresult.value;
        return true;
    }

    void WriteData(const Key & key, const Value & value)
    {
        std::stringstream request;
        request 
            << "insert or replace into "
            << dbname
            << " (id, value) values('"
            << key 
            << "', '" 
            << value 
            << "');";
        connection_.execute(request.str().c_str(), callback, "Writing data" );
    }

    std::map<Key, Value>  get_map()
    {
        return database_;
    }

    void show_db()
    {
        std::stringstream request;
        request
            << "select id, value from "
            << dbname
            << " order by id;";
        connection_.execute(request.str().c_str(), callback_show_dp, nullptr);
    }



    Database()
        : connection_()
    {
        stringstream ss;
        
        if (!connection_.is_opened())
        {
            throw DatabaseException(connection_.get_message().c_str());
        }
        
        std::cout << "Database opened" << "\n";

        std::cout << "Creating table..." << "\n";
        /// check if table exists
        /*char * sql =
            "IF EXISTS(SELECT 1"
            "FROM INFORMATION_SCHEMA.TABLES"
            "WHERE TABLE_TYPE = 'BASE TABLE'"
            "AND TABLE_NAME = 'mytablename')"
            "SELECT 1 AS res ELSE SELECT 0 AS res;";*/
        /*std::string sqlDrop(std::string("drop table ") + dbname + ";");
        connection_.execute( sqlDrop.c_str(), callback, "Creating table");*/
        
        std::string sqlCreate(std::string("create table if not exists ") + dbname + " ( id varchar, value text, primary key(id));");
        connection_.execute(sqlCreate.c_str(), callback, "Creating table");
        
        std::cout << "Table is created (or exists)" << "\n";


    }



    ~Database()
    {
        
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!closed!!!!!!!!!!!!" << "\n";
    }
};