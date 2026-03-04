#pragma once

#include <stdint.h>
#include <stddef.h>

// ===[ JsonValue Types ]===

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonValueType;

typedef struct JsonValue {
    JsonValueType type;
    union {
        bool boolValue;
        double numberValue;
        char* stringValue;
        struct { struct JsonValue* items; int count; int capacity; } array;
        struct { char** keys; struct JsonValue* values; int count; int capacity; } object;
    };
} JsonValue;

// ===[ Lifecycle ]===

JsonValue* JsonReader_parse(const char* json);
void JsonReader_free(JsonValue* value);

// ===[ Type Checks ]===

bool JsonReader_isNull(const JsonValue* value);
bool JsonReader_isBool(const JsonValue* value);
bool JsonReader_isNumber(const JsonValue* value);
bool JsonReader_isString(const JsonValue* value);
bool JsonReader_isArray(const JsonValue* value);
bool JsonReader_isObject(const JsonValue* value);

// ===[ Value Getters ]===

bool JsonReader_getBool(const JsonValue* value);
double JsonReader_getDouble(const JsonValue* value);
int64_t JsonReader_getInt(const JsonValue* value);
const char* JsonReader_getString(const JsonValue* value);

// ===[ Array Access ]===

int JsonReader_arrayLength(const JsonValue* value);
JsonValue* JsonReader_arrayGet(const JsonValue* value, int index);

// ===[ Object Access ]===

int JsonReader_objectLength(const JsonValue* value);
JsonValue* JsonReader_objectGet(const JsonValue* value, const char* key);
const char* JsonReader_objectGetKey(const JsonValue* value, int index);
JsonValue* JsonReader_objectGetValue(const JsonValue* value, int index);
