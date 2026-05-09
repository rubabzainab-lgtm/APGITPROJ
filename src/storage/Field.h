#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>

enum class FieldType { INT, FLOAT, STRING };

// Abstract base class for all field types
class Field {
public:
    virtual ~Field() {}
    virtual FieldType   type()                     const = 0;
    virtual int         compare(const Field* o)    const = 0;
    virtual void        serialize(char* buf)       const = 0;
    virtual int         serializedSize()           const = 0;
    virtual void        toString(char* out, int maxLen) const = 0;
    virtual Field*      clone()                    const = 0;

    bool operator==(const Field& o) const { return compare(&o) == 0; }
    bool operator!=(const Field& o) const { return compare(&o) != 0; }
    bool operator< (const Field& o) const { return compare(&o) <  0; }
    bool operator<=(const Field& o) const { return compare(&o) <= 0; }
    bool operator> (const Field& o) const { return compare(&o) >  0; }
    bool operator>=(const Field& o) const { return compare(&o) >= 0; }
};

// ── IntField ──────────────────────────────────────────────────────────────────
class IntField : public Field {
public:
    explicit IntField(int v = 0) : val_(v) {}
    int value() const { return val_; }

    FieldType type() const override { return FieldType::INT; }

    int compare(const Field* o) const override {
        const IntField* other = static_cast<const IntField*>(o);
        if (val_ < other->val_) return -1;
        if (val_ > other->val_) return  1;
        return 0;
    }

    // serialized as 4 bytes little-endian
    void serialize(char* buf) const override {
        buf[0] = (char)(val_ & 0xFF);
        buf[1] = (char)((val_ >> 8)  & 0xFF);
        buf[2] = (char)((val_ >> 16) & 0xFF);
        buf[3] = (char)((val_ >> 24) & 0xFF);
    }

    int serializedSize() const override { return 4; }

    void toString(char* out, int maxLen) const override {
        snprintf(out, maxLen, "%d", val_);
    }

    Field* clone() const override { return new IntField(val_); }

    static IntField* deserialize(const char* buf) {
        int v = ((unsigned char)buf[0])
              | ((unsigned char)buf[1] << 8)
              | ((unsigned char)buf[2] << 16)
              | ((unsigned char)buf[3] << 24);
        return new IntField(v);
    }

private:
    int val_;
};

// ── FloatField ────────────────────────────────────────────────────────────────
class FloatField : public Field {
public:
    explicit FloatField(double v = 0.0) : val_(v) {}
    double value() const { return val_; }

    FieldType type() const override { return FieldType::FLOAT; }

    int compare(const Field* o) const override {
        const FloatField* other = static_cast<const FloatField*>(o);
        if (val_ < other->val_ - 1e-9) return -1;
        if (val_ > other->val_ + 1e-9) return  1;
        return 0;
    }

    // serialized as 8 bytes (IEEE 754 double)
    void serialize(char* buf) const override {
        const char* src = reinterpret_cast<const char*>(&val_);
        for (int i = 0; i < 8; ++i) buf[i] = src[i];
    }

    int serializedSize() const override { return 8; }

    void toString(char* out, int maxLen) const override {
        snprintf(out, maxLen, "%.2f", val_);
    }

    Field* clone() const override { return new FloatField(val_); }

    static FloatField* deserialize(const char* buf) {
        double v;
        char* dst = reinterpret_cast<char*>(&v);
        for (int i = 0; i < 8; ++i) dst[i] = buf[i];
        return new FloatField(v);
    }

private:
    double val_;
};

// ── StringField ───────────────────────────────────────────────────────────────
class StringField : public Field {
public:
    static const int MAX_LEN = 128;

    explicit StringField(const char* v = "") {
        snprintf(buf_, sizeof(buf_), "%s", v);
    }

    const char* value() const { return buf_; }

    FieldType type() const override { return FieldType::STRING; }

    int compare(const Field* o) const override {
        const StringField* other = static_cast<const StringField*>(o);
        return strcmp(buf_, other->buf_);
    }

    // serialized as MAX_LEN bytes (null-padded)
    void serialize(char* buf) const override {
        int len = 0;
        while (buf_[len] && len < MAX_LEN - 1) ++len;
        for (int i = 0; i < MAX_LEN; ++i)
            buf[i] = (i <= len) ? buf_[i] : '\0';
    }

    int serializedSize() const override { return MAX_LEN; }

    void toString(char* out, int maxLen) const override {
        snprintf(out, maxLen, "%s", buf_);
    }

    Field* clone() const override { return new StringField(buf_); }

    static StringField* deserialize(const char* buf) {
        char tmp[MAX_LEN + 1];
        for (int i = 0; i < MAX_LEN; ++i) tmp[i] = buf[i];
        tmp[MAX_LEN] = '\0';
        return new StringField(tmp);
    }

private:
    char buf_[MAX_LEN + 1];
};
