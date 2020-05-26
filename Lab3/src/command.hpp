/// File: command.hpp
/// =================
/// Copyright 2020 Cloud-fantasy team
/// Contains command objects supported by the KV store.
#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <vector>

namespace simple_kv_store {

/// Tags.
enum class command_type : uint8_t {
    GET,
    SET,
    DEL,
    UNKNOWN
};

/// Base command class.
class command {
public:
    /// Ctor.
    command(command_type type = command_type::UNKNOWN) : type(type) {}

    /// Tag
    command_type type = command_type::UNKNOWN;

    /// Returns the args of this command. 
    /// NOTE: all args are serialized as std::string.
    virtual std::vector<std::string> args() = 0;
};

/// GET key
class get_command : public command {
public:
    /// Ctors.
    get_command();
    get_command(std::string const &key);
    get_command(const get_command &);
    get_command(get_command &&);
    ~get_command() = default;

    /// Assignment.
    get_command& operator=(const get_command&);
    get_command& operator=(get_command&&);

    /// GETTER.
    std::string &key() { return key_; }
    const std::string &key() const { return key_; }

    virtual std::vector<std::string> args() override;

private:
    /// Key into the store.
    std::string key_;
};

/// SET key value
class set_command : public command {
public:
    /// Ctors.
    set_command();
    set_command(std::string const &key, std::string const &value);
    set_command(const set_command&);
    set_command(set_command&&);
    ~set_command() = default;

    /// Assignment.
    set_command& operator=(const set_command&);
    set_command& operator=(set_command&&);

    /// GETTER.
    std::string &key() { return key_; }
    const std::string &key() const { return key_; }
    std::string &value() { return value_; }
    const std::string &value() const { return value_; }

    virtual std::vector<std::string> args() override;

private:
    std::string key_;
    std::string value_;
};

/// DEL key1 key2 ...
class del_command : public command {
public:
    /// Ctors.
    del_command();
    del_command(std::vector<std::string> const&args);
    del_command(const del_command&);
    del_command(del_command&&);
    ~del_command() = default;

    /// Assignment.
    del_command& operator=(const del_command&);
    del_command& operator=(del_command&&);

    void set_keys(std::vector<std::string> const &keys) { keys_ = keys; }

    virtual std::vector<std::string> args() override;

private:
    std::vector<std::string> keys_;
};

} // namespace simple_kv_store


#endif