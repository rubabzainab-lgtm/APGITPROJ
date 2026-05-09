#pragma once
#include "Field.h"
#include "../catalog/Schema.h"
#include <cstdio>

class Row {
public:
    Row() : fields_(nullptr), count_(0) {}

    explicit Row(int count) : count_(count) {
        fields_ = new Field*[count];
        for (int i = 0; i < count; ++i) fields_[i] = nullptr;
    }

    ~Row() {
        for (int i = 0; i < count_; ++i) delete fields_[i];
        delete[] fields_;
    }

    void setField(int i, Field* f) {
        if (i < 0 || i >= count_) return;
        delete fields_[i];
        fields_[i] = f;
    }

    Field* getField(int i) const { return (i >= 0 && i < count_) ? fields_[i] : nullptr; }
    int    count()         const { return count_; }

    // Serialize into buf according to schema's column definitions.
    // buf must be at least schema.recordSize() bytes.
    void serialize(const TableSchema& schema, char* buf) const {
        int offset = 0;
        for (int i = 0; i < schema.colCount; ++i) {
            const ColumnDef& col = schema.columns[i];
            Field* f = (i < count_) ? fields_[i] : nullptr;

            if (col.type == FieldType::INT) {
                if (f) f->serialize(buf + offset);
                else   for (int j = 0; j < 4; ++j) buf[offset + j] = 0;
            } else if (col.type == FieldType::FLOAT) {
                if (f) f->serialize(buf + offset);
                else   for (int j = 0; j < 8; ++j) buf[offset + j] = 0;
            } else {
                // STRING: pad/truncate to col.serSize
                const char* str = f ? static_cast<StringField*>(f)->value() : "";
                int len = 0;
                while (str[len] && len < col.serSize - 1) ++len;
                for (int j = 0; j < col.serSize; ++j)
                    buf[offset + j] = (j < len) ? str[j] : '\0';
            }
            offset += col.serSize;
        }
    }

    // Deserialize from buf. Caller owns the returned Row*.
    static Row* deserialize(const TableSchema& schema, const char* buf) {
        Row* row = new Row(schema.colCount);
        int offset = 0;
        for (int i = 0; i < schema.colCount; ++i) {
            const ColumnDef& col = schema.columns[i];
            Field* f = nullptr;
            if (col.type == FieldType::INT) {
                f = IntField::deserialize(buf + offset);
            } else if (col.type == FieldType::FLOAT) {
                f = FloatField::deserialize(buf + offset);
            } else {
                int n = (col.serSize < 255) ? col.serSize : 255;
                char tmp[256];
                for (int j = 0; j < n; ++j) tmp[j] = buf[offset + j];
                tmp[n] = '\0';
                f = new StringField(tmp);
            }
            row->setField(i, f);
            offset += col.serSize;
        }
        return row;
    }

    void print(const TableSchema& schema) const {
        for (int i = 0; i < count_; ++i) {
            if (i > 0) printf(" | ");
            char buf[256];
            if (fields_[i]) fields_[i]->toString(buf, 256);
            else             snprintf(buf, 256, "NULL");
            printf("%-15s", buf);
        }
        printf("\n");
    }

private:
    Field** fields_;
    int     count_;
};
