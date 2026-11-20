#pragma once

#include <string>
#include <string_view>
#include <vector>

struct JsonValue
{
    enum class Type
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> items;
    std::vector<std::string> keys;
    std::vector<JsonValue> values;

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }
    bool isBoolean() const { return type == Type::Boolean; }

    const JsonValue* find(std::string_view key) const;
};

bool parseJson(const std::string& text, JsonValue& result, std::string& error);
