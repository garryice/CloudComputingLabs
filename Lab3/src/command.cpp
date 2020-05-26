#include "command.hpp"

namespace simple_kv_store {

/*
GET command
*/

get_command::get_command()
    : command(command_type::GET) {}

get_command::get_command(std::string &&key)
    : command(command_type::GET), key_(std::move(key)) {}

get_command::get_command(const get_command &cmd)
    : command(cmd.type), key_(cmd.key_) {}

get_command::get_command(get_command &&cmd)
    : command(cmd.type), key_(std::move(cmd.key_)) { cmd.type = command_type::UNKNOWN; }

get_command &get_command::operator=(const get_command &cmd)
{
    if (&cmd != this)
    {
        get_command tmp(cmd);
        std::swap(*this, tmp);
    }

    return *this;
}

get_command &get_command::operator=(get_command &&cmd)
{
    if (&cmd != this)
    {
        std::swap(*this, cmd);
    }

    return *this;
}

std::vector<std::string> get_command::args()
{
    return std::vector<std::string>{key_};
}

/*
SET command.
*/

set_command::set_command()
    : command(command_type::SET) {}

set_command::set_command(std::string &&key, std::string &&value)
    : command(command_type::SET), key_(std::move(key)), value_(std::move(value)) {}

set_command::set_command(const set_command &cmd)
    : command(cmd.type), key_(cmd.key_), value_(cmd.value_) {}

set_command::set_command(set_command &&cmd)
    : command(cmd.type), key_(std::move(cmd.key_)), value_(std::move(cmd.value_))
{
    cmd.type = command_type::UNKNOWN;
}

set_command &set_command::operator=(const set_command &cmd)
{
    if (this != &cmd)
    {
        set_command tmp(cmd);
        std::swap(*this, tmp);
    }

    return *this;
}

set_command &set_command::operator=(set_command &&cmd)
{
    if (this != &cmd)
    {
        std::swap(*this, cmd);
    }

    return *this;
}

std::vector<std::string> set_command::args()
{
    return std::vector<std::string>{key_, value_};
}

/*
DEL command.
*/

del_command::del_command()
    : command(command_type::DEL) {}

del_command::del_command(std::vector<std::string> &&keys)
    : command(command_type::DEL), keys_(std::move(keys)) {}

del_command::del_command(const del_command &cmd)
    : command(cmd.type), keys_(cmd.keys_) {}

del_command::del_command(del_command &&cmd)
    : command(cmd.type), keys_(std::move(cmd.keys_)) { cmd.type = command_type::UNKNOWN; }

del_command &del_command::operator=(const del_command &cmd)
{
    if (this != &cmd)
    {
        del_command tmp(cmd);
        std::swap(*this, tmp);
    }
    return *this;
}

del_command &del_command::operator=(del_command &&cmd)
{
    if (this != &cmd)
    {
        std::swap(*this, cmd);
    }
    return *this;
}

std::vector<std::string> del_command::args()
{
    return keys_;
}

}