// Minimal JSON parser for configuration (no external deps)
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace cg
{
    struct JsonValue;

    using JsonObject = std::unordered_map<std::string, JsonValue>;
    using JsonArray = std::vector<JsonValue>;

    struct JsonValue
    {
        enum class Type
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object
        };

        Type type;
        bool bool_value;
        double number_value;
        std::string* string_value;
        JsonArray* array_value;
        JsonObject* object_value;

        JsonValue();
        JsonValue(std::nullptr_t);
        JsonValue(bool b);
        JsonValue(double d);
        JsonValue(const std::string& s);
        JsonValue(std::string&& s);
        JsonValue(const char* s);
        JsonValue(const JsonArray& a);
        JsonValue(JsonArray&& a);
        JsonValue(const JsonObject& o);
        JsonValue(JsonObject&& o);
        JsonValue(const JsonValue& other);
        JsonValue(JsonValue&& other) noexcept;
        JsonValue& operator=(const JsonValue& other);
        JsonValue& operator=(JsonValue&& other) noexcept;
        ~JsonValue();

        bool is_null() const { return type == Type::Null; }
        bool is_bool() const { return type == Type::Bool; }
        bool is_number() const { return type == Type::Number; }
        bool is_string() const { return type == Type::String; }
        bool is_array() const { return type == Type::Array; }
        bool is_object() const { return type == Type::Object; }

        const JsonArray& as_array() const { return *array_value; }
        const JsonObject& as_object() const { return *object_value; }
        const std::string& as_string() const { return *string_value; }
        double as_number() const { return number_value; }
        bool as_bool() const { return bool_value; }

        JsonArray& get_array() { return *array_value; }
        JsonObject& get_object() { return *object_value; }
        std::string& get_string() { return *string_value; }
        double& get_number() { return number_value; }
        bool& get_bool() { return bool_value; }
    };

    class JsonParser
    {
    public:
        static bool Parse(const std::string& input, JsonValue& out_value, std::string& out_error);

    private:
        JsonParser(const std::string& input);
        bool parse_value(JsonValue& out);
        bool parse_object(JsonValue& out);
        bool parse_array(JsonValue& out);
        bool parse_string(std::string& out);
        bool parse_number(double& out);
        bool parse_true();
        bool parse_false();
        bool parse_null();
        void skip_ws();
        bool consume(char c);
        bool peek(char c) const;
        bool eof() const;

        const std::string& s;
        size_t i;
        std::string* err;
    };
}


