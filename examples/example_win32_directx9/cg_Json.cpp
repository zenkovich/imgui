#include "cg_Json.h"

#include <cctype>
#include <utility>

namespace cg
{
    // JsonValue impl
    JsonValue::JsonValue()
        : type(Type::Null), bool_value(false), number_value(0.0), string_value(nullptr), array_value(nullptr), object_value(nullptr) {}
    JsonValue::JsonValue(std::nullptr_t)
        : JsonValue() {}
    JsonValue::JsonValue(bool b)
        : type(Type::Bool), bool_value(b), number_value(0.0), string_value(nullptr), array_value(nullptr), object_value(nullptr) {}
    JsonValue::JsonValue(double d)
        : type(Type::Number), bool_value(false), number_value(d), string_value(nullptr), array_value(nullptr), object_value(nullptr) {}
    JsonValue::JsonValue(const std::string& s)
        : type(Type::String), bool_value(false), number_value(0.0), string_value(new std::string(s)), array_value(nullptr), object_value(nullptr) {}
    JsonValue::JsonValue(std::string&& s)
        : type(Type::String), bool_value(false), number_value(0.0), string_value(new std::string(std::move(s))), array_value(nullptr), object_value(nullptr) {}
    JsonValue::JsonValue(const char* s)
        : JsonValue(std::string(s)) {}
    JsonValue::JsonValue(const JsonArray& a)
        : type(Type::Array), bool_value(false), number_value(0.0), string_value(nullptr), array_value(new JsonArray(a)), object_value(nullptr) {}
    JsonValue::JsonValue(JsonArray&& a)
        : type(Type::Array), bool_value(false), number_value(0.0), string_value(nullptr), array_value(new JsonArray(std::move(a))), object_value(nullptr) {}
    JsonValue::JsonValue(const JsonObject& o)
        : type(Type::Object), bool_value(false), number_value(0.0), string_value(nullptr), array_value(nullptr), object_value(new JsonObject(o)) {}
    JsonValue::JsonValue(JsonObject&& o)
        : type(Type::Object), bool_value(false), number_value(0.0), string_value(nullptr), array_value(nullptr), object_value(new JsonObject(std::move(o))) {}
    JsonValue::JsonValue(const JsonValue& other) : type(Type::Null), bool_value(false), number_value(0.0), string_value(nullptr), array_value(nullptr), object_value(nullptr)
    {
        *this = other;
    }
    JsonValue::JsonValue(JsonValue&& other) noexcept : type(Type::Null), bool_value(false), number_value(0.0), string_value(nullptr), array_value(nullptr), object_value(nullptr)
    {
        *this = std::move(other);
    }
    JsonValue& JsonValue::operator=(const JsonValue& other)
    {
        if (this == &other)
            return *this;
        this->~JsonValue();
        type = other.type;
        bool_value = other.bool_value;
        number_value = other.number_value;
        switch (type)
        {
        case Type::Null: break;
        case Type::Bool: break;
        case Type::Number: break;
        case Type::String: string_value = new std::string(*other.string_value); break;
        case Type::Array: array_value = new JsonArray(*other.array_value); break;
        case Type::Object: object_value = new JsonObject(*other.object_value); break;
        }
        return *this;
    }
    JsonValue& JsonValue::operator=(JsonValue&& other) noexcept
    {
        if (this == &other)
            return *this;
        this->~JsonValue();
        type = other.type;
        bool_value = other.bool_value;
        number_value = other.number_value;
        string_value = other.string_value; other.string_value = nullptr;
        array_value = other.array_value; other.array_value = nullptr;
        object_value = other.object_value; other.object_value = nullptr;
        other.type = Type::Null;
        return *this;
    }
    JsonValue::~JsonValue()
    {
        switch (type)
        {
        case Type::String: delete string_value; break;
        case Type::Array: delete array_value; break;
        case Type::Object: delete object_value; break;
        default: break;
        }
        type = Type::Null;
        string_value = nullptr;
        array_value = nullptr;
        object_value = nullptr;
    }
    JsonParser::JsonParser(const std::string& input) : s(input), i(0), err(nullptr) {}

    bool JsonParser::Parse(const std::string& input, JsonValue& out_value, std::string& out_error)
    {
        JsonParser p(input);
        p.err = &out_error;
        p.skip_ws();
        if (!p.parse_value(out_value))
            return false;
        p.skip_ws();
        if (!p.eof())
        {
            out_error = "Trailing characters after JSON value";
            return false;
        }
        return true;
    }

    bool JsonParser::parse_value(JsonValue& out)
    {
        skip_ws();
        if (eof())
            return false;
        char c = s[i];
        if (c == '"')
        {
            std::string str;
            if (!parse_string(str)) return false;
            out = JsonValue(std::move(str));
            return true;
        }
        if (c == '{')
            return parse_object(out);
        if (c == '[')
            return parse_array(out);
        if (c == 't') { if (!parse_true()) return false; out = JsonValue(true); return true; }
        if (c == 'f') { if (!parse_false()) return false; out = JsonValue(false); return true; }
        if (c == 'n') { if (!parse_null()) return false; out = JsonValue(nullptr); return true; }
        double num;
        if (parse_number(num)) { out = JsonValue(num); return true; }
        if (err)
            *err = "Invalid JSON value";
        return false;
    }

    bool JsonParser::parse_object(JsonValue& out)
    {
        if (!consume('{'))
            return false;
        skip_ws();
        JsonObject obj;
        if (peek('}')) { consume('}'); out = JsonValue(std::move(obj)); return true; }
        while (true)
        {
            skip_ws();
            std::string key;
            if (!parse_string(key))
                return false;
            skip_ws();
            if (!consume(':')) { if (err) *err = "Expected ':' in object"; return false; }
            skip_ws();
            JsonValue val;
            if (!parse_value(val))
                return false;
            obj.emplace(std::move(key), std::move(val));
            skip_ws();
            if (consume('}')) break;
            if (!consume(',')) { if (err) *err = "Expected ',' in object"; return false; }
        }
        out = JsonValue(std::move(obj));
        return true;
    }

    bool JsonParser::parse_array(JsonValue& out)
    {
        if (!consume('['))
            return false;
        skip_ws();
        JsonArray arr;
        if (peek(']')) { consume(']'); out = JsonValue(std::move(arr)); return true; }
        while (true)
        {
            JsonValue val;
            if (!parse_value(val))
                return false;
            arr.emplace_back(std::move(val));
            skip_ws();
            if (consume(']')) break;
            if (!consume(',')) { if (err) *err = "Expected ',' in array"; return false; }
        }
        out = JsonValue(std::move(arr));
        return true;
    }

    bool JsonParser::parse_string(std::string& out)
    {
        if (!consume('"'))
            return false;
        std::string res;
        while (!eof())
        {
            char c = s[i++];
            if (c == '"') { out = std::move(res); return true; }
            if (c == '\\')
            {
                if (eof())
                    return false;
                char e = s[i++];
                switch (e)
                {
                case '"': res.push_back('"'); break;
                case '\\': res.push_back('\\'); break;
                case '/': res.push_back('/'); break;
                case 'b': res.push_back('\b'); break;
                case 'f': res.push_back('\f'); break;
                case 'n': res.push_back('\n'); break;
                case 'r': res.push_back('\r'); break;
                case 't': res.push_back('\t'); break;
                default:
                    return false; // no \u support
                }
            }
            else
            {
                res.push_back(c);
            }
        }
        return false;
    }

    bool JsonParser::parse_number(double& out)
    {
        size_t start = i;
        if (peek('-'))
            i++;
        if (eof())
            return false;
        if (s[i] == '0')
        {
            i++;
        }
        else if (std::isdigit(static_cast<unsigned char>(s[i])))
        {
            while (!eof() && std::isdigit(static_cast<unsigned char>(s[i])))
                i++;
        }
        else
            return false;

        if (!eof() && s[i] == '.')
        {
            i++;
            if (eof() || !std::isdigit(static_cast<unsigned char>(s[i])))
                return false;
            while (!eof() && std::isdigit(static_cast<unsigned char>(s[i])))
                i++;
        }
        if (!eof() && (s[i] == 'e' || s[i] == 'E'))
        {
            i++;
            if (!eof() && (s[i] == '+' || s[i] == '-'))
                i++;
            if (eof() || !std::isdigit(static_cast<unsigned char>(s[i])))
                return false;
            while (!eof() && std::isdigit(static_cast<unsigned char>(s[i])))
                i++;
        }
        try
        {
            out = std::stod(s.substr(start, i - start));
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    bool JsonParser::parse_true()  { const char* t = "true";  for (int k=0;k<4;k++) if (eof()||s[i++]!=t[k]) return false; return true; }
    bool JsonParser::parse_false() { const char* f = "false"; for (int k=0;k<5;k++) if (eof()||s[i++]!=f[k]) return false; return true; }
    bool JsonParser::parse_null()  { const char* n = "null";  for (int k=0;k<4;k++) if (eof()||s[i++]!=n[k]) return false; return true; }

    void JsonParser::skip_ws()
    {
        while (!eof())
        {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                i++;
                continue;
            }
            break;
        }
    }

    bool JsonParser::consume(char c)
    {
        if (!eof() && s[i] == c)
        {
            i++;
            return true;
        }
        return false;
    }

    bool JsonParser::peek(char c) const
    {
        return !eof() && s[i] == c;
    }

    bool JsonParser::eof() const
    {
        return i >= s.size();
    }
}


