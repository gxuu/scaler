#pragma once
#include <map>
#include <set>

template <typename KeyT, typename ValT>
class one_to_many_dict {
  std::map<KeyT, std::set<ValT>> _key_to_value_set;
  std::map<ValT, KeyT>           _value_to_key;

public:
  void add(KeyT key, ValT val) {
    _value_to_key[val] = key;
    _key_to_value_set[key].insert(val);
  }

  KeyT remove_value(ValT val) {
    auto key = _value_to_key[val];
    _value_to_key.erase(val);
    _key_to_value_set[key].erase(val);
    if(_key_to_value_set[key].empty()) {
      _key_to_value_set.erase(key);
    }
    return key;
  }
};
