#include <map>

template<typename Key, typename Value>
class Database
{
private:
    std::map<Key, Value> database_;
public:
    bool LoadDataByKey(const Key & k, Value & result)
    {
        auto it = database_.find(k);
        if (it != database_.end())
        {
            result = it->second;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool WriteData(const Key & key, const Value & value)
    {
        database_[key] = value;
        return true;
    }
};