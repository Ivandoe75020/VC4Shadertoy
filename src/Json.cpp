#include "pch.h"
#include "Json.h"

#include <charconv>
#include <cstdint>
#include <system_error>
#include <utility>

namespace
{
    constexpr int MaxDepth = 200;

    class Parser
    {
    public:
        explicit Parser(const std::string& text)
            : text_(text)
        {
        }

        bool parseDocument(JsonValue& result, std::string& error)
        {
            if (text_.size() >= 3 && static_cast<unsigned char>(text_[0]) == 0xEF &&
                static_cast<unsigned char>(text_[1]) == 0xBB && static_cast<unsigned char>(text_[2]) == 0xBF)
            {
                position_ = 3;
            }

            skipWhitespace();
            if (position_ >= text_.size())
            {
                error = describe("the file is empty");
                return false;
            }

            if (!parseValue(result, 0))
            {
                error = error_;
                return false;
            }

            skipWhitespace();
            if (position_ < text_.size())
            {
                error = describe("unexpected content after the end of the JSON data");
                return false;
            }
            return true;
        }

    private:
        const std::string& text_;
        std::size_t position_ = 0;
        std::string error_;

        std::string describe(const std::string& message) const
        {
            std::size_t line = 1;
            std::size_t column = 1;
            const std::size_t limit = position_ < text_.size() ? position_ : text_.size();
            for (std::size_t i = 0; i < limit; ++i)
            {
                if (text_[i] == '\n')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }
            return message + " (line " + std::to_string(line) + ", column " + std::to_string(column) + ")";
        }

        bool fail(const std::string& message)
        {
            error_ = describe(message);
            return false;
        }

        void skipWhitespace()
        {
            while (position_ < text_.size())
            {
                const char c = text_[position_];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                {
                    ++position_;
                }
                else
                {
                    break;
                }
            }
        }

        bool consumeLiteral(const char* literal)
        {
            std::size_t i = 0;
            while (literal[i] != '\0')
            {
                if (position_ + i >= text_.size() || text_[position_ + i] != literal[i])
                {
                    return false;
                }
                ++i;
            }
            position_ += i;
            return true;
        }

        bool parseValue(JsonValue& value, int depth)
        {
            if (depth > MaxDepth)
            {
                return fail("the data is nested too deeply");
            }
            if (position_ >= text_.size())
            {
                return fail("unexpected end of the data");
            }

            const char c = text_[position_];
            if (c == '{')
            {
                return parseObject(value, depth);
            }
            if (c == '[')
            {
                return parseArray(value, depth);
            }
            if (c == '"')
            {
                value.type = JsonValue::Type::String;
                return parseString(value.text);
            }
            if (c == 't')
            {
                if (!consumeLiteral("true"))
                {
                    return fail("invalid value");
                }
                value.type = JsonValue::Type::Boolean;
                value.boolean = true;
                return true;
            }
            if (c == 'f')
            {
                if (!consumeLiteral("false"))
                {
                    return fail("invalid value");
                }
                value.type = JsonValue::Type::Boolean;
                value.boolean = false;
                return true;
            }
            if (c == 'n')
            {
                if (!consumeLiteral("null"))
                {
                    return fail("invalid value");
                }
                value.type = JsonValue::Type::Null;
                return true;
            }
            if (c == '-' || (c >= '0' && c <= '9'))
            {
                return parseNumber(value);
            }
            return fail("unexpected character");
        }

        bool parseObject(JsonValue& value, int depth)
        {
            value.type = JsonValue::Type::Object;
            ++position_;
            skipWhitespace();
            if (position_ < text_.size() && text_[position_] == '}')
            {
                ++position_;
                return true;
            }

            for (;;)
            {
                skipWhitespace();
                if (position_ >= text_.size() || text_[position_] != '"')
                {
                    return fail("a property name was expected");
                }

                std::string key;
                if (!parseString(key))
                {
                    return false;
                }

                skipWhitespace();
                if (position_ >= text_.size() || text_[position_] != ':')
                {
                    return fail("a colon was expected");
                }
                ++position_;
                skipWhitespace();

                JsonValue member;
                if (!parseValue(member, depth + 1))
                {
                    return false;
                }
                value.keys.push_back(std::move(key));
                value.values.push_back(std::move(member));

                skipWhitespace();
                if (position_ >= text_.size())
                {
                    return fail("unexpected end of the data");
                }
                if (text_[position_] == ',')
                {
                    ++position_;
                    continue;
                }
                if (text_[position_] == '}')
                {
                    ++position_;
                    return true;
                }
                return fail("a comma or a closing brace was expected");
            }
        }

        bool parseArray(JsonValue& value, int depth)
        {
            value.type = JsonValue::Type::Array;
            ++position_;
            skipWhitespace();
            if (position_ < text_.size() && text_[position_] == ']')
            {
                ++position_;
                return true;
            }

            for (;;)
            {
                skipWhitespace();
                JsonValue item;
                if (!parseValue(item, depth + 1))
                {
                    return false;
                }
                value.items.push_back(std::move(item));

                skipWhitespace();
                if (position_ >= text_.size())
                {
                    return fail("unexpected end of the data");
                }
                if (text_[position_] == ',')
                {
                    ++position_;
                    continue;
                }
                if (text_[position_] == ']')
                {
                    ++position_;
                    return true;
                }
                return fail("a comma or a closing bracket was expected");
            }
        }

        bool parseNumber(JsonValue& value)
        {
            const std::size_t start = position_;
            if (text_[position_] == '-')
            {
                ++position_;
            }
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9')
            {
                return fail("invalid number");
            }
            if (text_[position_] == '0')
            {
                ++position_;
            }
            else
            {
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                {
                    ++position_;
                }
            }
            if (position_ < text_.size() && text_[position_] == '.')
            {
                ++position_;
                const std::size_t digitsStart = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                {
                    ++position_;
                }
                if (position_ == digitsStart)
                {
                    return fail("invalid number");
                }
            }
            if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E'))
            {
                ++position_;
                if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-'))
                {
                    ++position_;
                }
                const std::size_t digitsStart = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                {
                    ++position_;
                }
                if (position_ == digitsStart)
                {
                    return fail("invalid number");
                }
            }

            double parsed = 0.0;
            const auto result = std::from_chars(text_.data() + start, text_.data() + position_, parsed);
            if (result.ec != std::errc{} && result.ec != std::errc::result_out_of_range)
            {
                return fail("invalid number");
            }
            value.type = JsonValue::Type::Number;
            value.number = parsed;
            return true;
        }

        static void appendUtf8(std::string& out, std::uint32_t codePoint)
        {
            if (codePoint < 0x80)
            {
                out.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else if (codePoint < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
                out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
        }

        bool parseHex4(std::uint32_t& result)
        {
            if (position_ + 4 > text_.size())
            {
                return fail("invalid unicode escape");
            }
            result = 0;
            for (int i = 0; i < 4; ++i)
            {
                const char c = text_[position_ + i];
                result <<= 4;
                if (c >= '0' && c <= '9')
                {
                    result |= static_cast<std::uint32_t>(c - '0');
                }
                else if (c >= 'a' && c <= 'f')
                {
                    result |= static_cast<std::uint32_t>(c - 'a' + 10);
                }
                else if (c >= 'A' && c <= 'F')
                {
                    result |= static_cast<std::uint32_t>(c - 'A' + 10);
                }
                else
                {
                    return fail("invalid unicode escape");
                }
            }
            position_ += 4;
            return true;
        }

        bool parseString(std::string& out)
        {
            ++position_;
            out.clear();
            for (;;)
            {
                if (position_ >= text_.size())
                {
                    return fail("a string is not terminated");
                }

                const char c = text_[position_++];
                if (c == '"')
                {
                    return true;
                }
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    --position_;
                    return fail("a string contains a control character");
                }
                if (c != '\\')
                {
                    out.push_back(c);
                    continue;
                }

                if (position_ >= text_.size())
                {
                    return fail("a string is not terminated");
                }
                const char escape = text_[position_++];
                switch (escape)
                {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u':
                {
                    std::uint32_t codePoint = 0;
                    if (!parseHex4(codePoint))
                    {
                        return false;
                    }
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
                    {
                        if (position_ + 1 < text_.size() && text_[position_] == '\\' && text_[position_ + 1] == 'u')
                        {
                            position_ += 2;
                            std::uint32_t low = 0;
                            if (!parseHex4(low))
                            {
                                return false;
                            }
                            if (low >= 0xDC00 && low <= 0xDFFF)
                            {
                                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                            }
                            else
                            {
                                codePoint = 0xFFFD;
                            }
                        }
                        else
                        {
                            codePoint = 0xFFFD;
                        }
                    }
                    else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
                    {
                        codePoint = 0xFFFD;
                    }
                    appendUtf8(out, codePoint);
                    break;
                }
                default:
                    --position_;
                    return fail("invalid escape sequence in a string");
                }
            }
        }
    };
}

const JsonValue* JsonValue::find(std::string_view key) const
{
    if (type != Type::Object)
    {
        return nullptr;
    }
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        if (keys[i] == key)
        {
            return &values[i];
        }
    }
    return nullptr;
}

bool parseJson(const std::string& text, JsonValue& result, std::string& error)
{
    result = JsonValue{};
    error.clear();
    Parser parser(text);
    return parser.parseDocument(result, error);
}
