#pragma once

#include <map>
#include <set>

template <typename K, typename V>
struct OneToManyDict {
    std::map<K, std::set<V>> _keyToValues;
    std::map<V, K> _valueToKey;

    bool contains(const K& key) { return _keyToValues.contains(key); }

    const std::map<K, std::set<V>>& keys() { return _keyToValues; }
    const std::map<V, K>& values() { return _valueToKey; }

    bool add(const K& key, const V& value)
    {
        if (_valueToKey.contains(value) && _valueToKey[value] != key) {
            return false;
        }

        _valueToKey[value] = key;
        _keyToValues[key].insert(value);
        return true;
    }

    bool hasKey(const K& key) { return _keyToValues.contains(key); }
    bool hasValue(const V& value) { return _valueToKey.contains(value); }

    const K* getKey(const V& value)
    {
        auto it = _valueToKey.find(value);
        return it == _valueToKey.end() ? nullptr : &it->second;
    }

    const std::set<V>* getValues(const K& key)
    {
        auto it = _keyToValues.find(key);
        return it == _keyToValues.end() ? nullptr : &it->second;
    }

    std::pair<std::set<V>, bool> removeKey(const K& key)
    {
        auto it = _keyToValues.find(key);
        if (it == _keyToValues.end()) {
            return {{}, false};
        }

        for (const auto& value: it->second) {
            _valueToKey.erase(value);
        }

        auto res = std::move(it->second);
        _keyToValues.erase(it);
        return {res, true};
    }

    std::pair<K, bool> removeValue(const V& value)
    {
        auto it = _valueToKey.find(value);
        if (it == _valueToKey.end()) {
            return {{}, false};
        }

        auto key = std::move(it->second);
        _valueToKey.erase(it);

        auto val = _keyToValues.find(key);
        if (val->second.size() == 0) {
            _keyToValues.erase(val);
        }

        return {key, true};
    }
};
