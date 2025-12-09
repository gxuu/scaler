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

    std::pair<K, bool> getKey(const V& value)
    {
        auto k = _valueToKey.find(value);
        if (k == _valueToKey.end()) {
            return {{}, false};
        }
        return {k->second, true};
    }

    const std::set<V>* getValues(const K& key)
    {
        auto v = _keyToValues.find(key);
        if (v == _keyToValues.end()) {
            return nullptr;
        }
        return &v->second;
    }

    std::pair<std::set<V>, bool> removeKey(const K& key)
    {
        auto v = _keyToValues.find(key);
        if (v == _keyToValues.end()) {
            return {{}, false};
        }

        for (const auto& value: v->second) {
            _valueToKey.erase(value);
        }

        auto res = v->second;
        _keyToValues.erase(v);
        return {res, true};
    }

    std::pair<K, bool> removeValue(const V& value)
    {
        auto k = _valueToKey.find(value);
        if (k == _valueToKey.end()) {
            return {{}, false};
        }

        auto res = k->second;
        _valueToKey.erase(k);

        auto vit = _keyToValues.find(res);
        if (vit->second.size() == 0) {
            _keyToValues.erase(vit);
        }

        return {res, true};
    }
};
