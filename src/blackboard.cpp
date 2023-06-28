#include "behaviortree_cpp_v3/blackboard.h"

namespace BT
{
void Blackboard::enableAutoRemapping(bool remapping)
{
  autoremapping_ = remapping;
}

const PortInfo* Blackboard::portInfo(const std::string& key)
{
  std::unique_lock<std::mutex> lock(mutex_);

  auto it = storage_.find(key);
  return (it == storage_.end()) ? nullptr : &(it->second->port_info);
}

void Blackboard::addSubtreeRemapping(StringView internal, StringView external)
{
  internal_to_external_.insert(
      {static_cast<std::string>(internal), static_cast<std::string>(external)});
}

void Blackboard::debugMessage() const
{
  for (const auto& it: storage_)
  {
    const auto& key = it.first;
    const auto& entry = it.second;

    auto port_type = entry->port_info.type();
    if (!port_type)
    {
      port_type = &(entry->value.type());
    }

    std::cout << key << " (" << demangle(port_type) << ") -> ";

    if (auto parent = parent_bb_.lock())
    {
      auto remapping_it = internal_to_external_.find(key);
      if (remapping_it != internal_to_external_.end())
      {
        std::cout << "remapped to parent [" << remapping_it->second << "]" << std::endl;
        continue;
      }
    }
    std::cout << ((entry->value.empty()) ? "empty" : "full") << std::endl;
  }
}

std::vector<StringView> Blackboard::getKeys() const
{
  if (storage_.empty())
  {
    return {};
  }
  std::vector<StringView> out;
  out.reserve(storage_.size());
  for (const auto& entry_it : storage_)
  {
    out.push_back(entry_it.first);
  }
  return out;
}

std::shared_ptr<Blackboard::Entry>
Blackboard::createEntryImpl(const std::string &key, const PortInfo& info)
{
  std::unique_lock<std::mutex> lock(mutex_);
  // This function might be called recursively, when we do remapping, because we move
  // to the top scope to find already existing  entries

  // search if exists already
  auto storage_it = storage_.find(key);
  if(storage_it != storage_.end())
  {
    const auto old_type = storage_it->second->port_info.type();
    if (old_type && info.type() && old_type != info.type())
    {
      throw LogicError("Blackboard: once declared, the type of a port "
                       "shall not change. Previously declared type [",
                       BT::demangle(old_type), "] != new type [",
                       BT::demangle(info.type()), "]");
    }
    return storage_it->second;
  }

  std::shared_ptr<Entry> entry;

  // manual remapping first
  auto remapping_it = internal_to_external_.find(key);
  if (remapping_it != internal_to_external_.end())
  {
    const auto& remapped_key = remapping_it->second;
    if (auto parent = parent_bb_.lock())
    {
      entry = parent->createEntryImpl(remapped_key, info);
    }
  }
  else if(autoremapping_)
  {
    if (auto parent = parent_bb_.lock())
    {
      entry = parent->createEntryImpl(key, info);
    }
  }
  else // not remapped, nor found. Create locally.
  {
    entry = std::make_shared<Entry>(info);
  }
  storage_.insert( {key, entry} );
  return entry;
}

}   // namespace BT
