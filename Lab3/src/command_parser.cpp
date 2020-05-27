#include <sstream>
#include "errors.hpp"
#include "common.hpp"
#include "command_parser.hpp"

namespace simple_kv_store {

std::string command_parser::separator = "\r\n";

command_parser::command_parser(std::vector<char> &&data)
    : data_(std::move(data)), idx_(0) {}

command_parser::command_parser(command_parser &&cmd_parser)
    : data_(std::move(cmd_parser.data_)), idx_(cmd_parser.idx_)
{
    data_.reserve(PARSER_BUFF_SIZE); 
}

command_parser& command_parser::operator=(command_parser &&cmd_parser)
{
    if (this != &cmd_parser)
    {
        std::swap(*this, cmd_parser);
    }
    return *this;
}

std::unique_ptr<command> command_parser::read_command()
{
    // Must be a RESP array.
    expect_char('*');

    /// Number of elems composed of this command.
    std::size_t num_elem = read_num_elem();
    expect_separator();

    // Type of the command.
    command_type type = read_command_type();
    expect_separator();

    switch (type)
    {
        case CMD_GET: return read_get_command(num_elem);
        case CMD_SET: return read_set_command(num_elem);
        case CMD_DEL: return read_del_command(num_elem);
        default:
            __PARSER_THROW("unregconized command");
    }

    return nullptr;
}

std::size_t command_parser::read_num_elem()
{
    std::stringstream num_stream;
    char ch = peek_char();

    /// Consume all numeric char
    while (ch >= '0' && ch <= '9')
    {
        read_char();
        num_stream << ch;
        ch = peek_char();
    }

    try
    {
        std::string num_str = num_stream.str();
        std::size_t num = std::stol(num_str);

        return num;
    } catch (std::exception &e) { __PARSER_THROW("error read_num_elem"); }
}

command_type command_parser::read_command_type()
{
    std::string type_str = read_bulk_string();
    if (type_str == "GET")
        return CMD_GET;
    if (type_str == "SET")
        return CMD_SET;
    if (type_str == "DEL")
        return CMD_DEL;

    __PARSER_THROW("unknown command type");
}

std::unique_ptr<command>
command_parser::read_get_command(std::size_t num_elem)
{
    if (num_elem == 0)
        __PARSER_THROW("mismatching args");

    /// Read the key.
    std::stringstream key_stream;
    key_stream << read_bulk_string();

    /// NOTE: If there are more than one string, we'll concatenate them as a single key.
    for (std::size_t i = 1; i < num_elem; i++)
        key_stream << " " << read_bulk_string();

    std::string key = key_stream.str();
    std::unique_ptr<command> get_cmd{ new get_command(std::move(key)) };
    return get_cmd;
}

std::unique_ptr<command>
command_parser::read_set_command(std::size_t num_elem)
{
    if (num_elem < 2)
        __PARSER_THROW("missing args");

    /// NOTE: Wasn't mine idea.
    std::string key = read_bulk_string();

    /// Read the value.
    std::stringstream value_stream;
    value_stream << read_bulk_string();

    /// NOTE: if there are more than one string, we'll concatenate them as a single value.
    for (std::size_t i = 2; i < num_elem; i++)
        value_stream << read_bulk_string();

    std::string value = value_stream.str();
    std::unique_ptr<command> set_cmd{ new set_command{std::move(key), std::move(value)} };
    return set_cmd;
}

std::unique_ptr<command>
command_parser::read_del_command(std::size_t num_elem)
{
    if (num_elem == 0)
        __PARSER_THROW("mising args");

    /// Consume as many keys as we can.
    std::vector<std::string> keys;
    keys.push_back(read_bulk_string());

    for (std::size_t i = 1; i < num_elem; i++)
        keys.push_back(read_bulk_string());

    std::unique_ptr<command> del_cmd{ new del_command{ std::move(keys) } };
    return del_cmd;
}

/// TODO: I think it'd make more sense to be able to parse simple strings too.
std::string command_parser::read_bulk_string()
{
    expect_char('$', "expecting '$'");

    /// Get the size of the string.
    std::size_t num_elem = read_num_elem();
    expect_separator();

    /// Read string itself.
    std::stringstream type_stream;
    for (std::size_t i = 0; i < num_elem; i++)
        type_stream << read_char();

    return type_stream.str();
}

void command_parser::expect_char(char ch, const std::string &msg)
{
    char c = read_char();
    if (c != ch)
        __PARSER_THROW(msg);
}

void command_parser::expect_separator()
{
    char c = read_char();
    if (c != '\r')
        __PARSER_THROW("expect separator");

    c = read_char();
    if (c != '\n')
        __PARSER_THROW("expect separator");
}

char command_parser::read_char()
{
    char ch = peek_char();

    idx_++;
    return ch;
}

char command_parser::peek_char()
{
    /// Buffer is empty.
    if (data_.empty())
    {
        __PARSER_THROW("fail reading from client");
    }
    /// Buffer is full. Discard all data consumed.
    else if (idx_ == data_.size())
    {
        __PARSER_THROW("incomplete command");
    }

    return data_[idx_];
}

}    // namespace simple_kv_store