#ifndef STREAM_CODER_HPP
#define STREAM_CODER_HPP

#include "streams.hpp"
#include "dynamic_array.hpp"
#include "exceptions.hpp"

template <class T>
class Codec {
public:
    virtual T Encode(T item) const = 0;
    virtual T Decode(T item) const = 0;
    virtual void Reset() const {} // сброс внутреннего состояния перед новым проходом;
    virtual ~Codec() = default;
};

// Шифр Цезаря
class CaesarCodec : public Codec<char> {
private:
    int shift; // ключ(сколько сдвиг)
    char ShiftLatin(char c, int s) const {
        if (c >= 'a' && c <= 'z') {
            return static_cast<char>('a' + ((c - 'a' + s) % 26 + 26) % 26); // преобр из int к chat
        }
        if (c >= 'A' && c <= 'Z') {
            return static_cast<char>('A' + ((c - 'A' + s) % 26 + 26) % 26);
        }
        return c;
    }
public:
    explicit CaesarCodec(int shift) : shift(shift) {}
    char Encode(char c) const override { return ShiftLatin(c, shift); } // шифровка (вперед)
    char Decode(char c) const override { return ShiftLatin(c, -shift); } // дешифровка (назад)
};

// XOR кодек
class XorCodec : public Codec<char> {
private:
    DynamicArray<char> key; // ключ
    mutable size_t keyPos; // текущая буква пароля, которую можно менять
public:
    explicit XorCodec(const char* keyData, int keyLen)
        : key(const_cast<char*>(keyData), keyLen), keyPos(0) {
        if (keyLen <= 0) throw InvalidArgumentException("Ключ XOR не может быть пустым");
    }
    ~XorCodec() override = default;

    char Encode(char c) const override {
        char k = key.Get(keyPos);
        keyPos = (keyPos + 1) % key.GetSize(); // сдвиг, если дошли до конца то сначала
        return static_cast<char>(c ^ k);
    }
    char Decode(char c) const override { return Encode(c); }
    void Reset() const override { keyPos = 0; } // сброс позиции ключа перед новым проходом
};


template <class T>
class StreamCoder { // Кодировщик с буфером
private:
    Codec<T>* codec; // алгоритм шифрования
    size_t bufferSize; // размер буфера
    DynamicArray<T> buffer; // временное хранилище
    size_t inBytes; // прочитано
    size_t outBytes; // записано

    void EnsureValid() {
        if (codec == nullptr) throw InvalidArgumentException("Кодек не задан");
        if (bufferSize == 0) throw InvalidArgumentException("Размер буфера должен быть > 0");
    }

    size_t FillBuffer(ReadOnlyStream<T>* in, size_t limit) { // заполнение буфера, но не более limit элементов
        size_t cnt = 0; // эл положили
        while (cnt < limit && !in->IsEndOfStream()) { // пока не забили буфер
            buffer.Set(cnt, in->Read()); // читаем по эл
            cnt++;
        }
        return cnt; // сколько эл прочитано
    }

    void FlushEncoded(WriteOnlyStream<T>* out, size_t cnt) { // буфер в поток (зашифрованный)
        for (size_t i = 0; i < cnt; i++) {
            out->Write(codec->Encode(buffer.Get(i))); // зашифр символ в поток
        }
        outBytes += cnt;
    }

    void FlushDecoded(WriteOnlyStream<T>* out, size_t cnt) { // зашифровать буфер
        for (size_t i = 0; i < cnt; i++) {
            out->Write(codec->Decode(buffer.Get(i))); // то же но узже расшифровка
        }
        outBytes += cnt;
    }

public:
    StreamCoder(Codec<T>* codec, size_t bufferSize = 64) // буф по ум - 64
        : codec(codec), bufferSize(bufferSize),
          buffer(static_cast<int>(bufferSize)),
          inBytes(0), outBytes(0) { // кол-во проч и зап эл
        EnsureValid(); // проверька на правильность
    }



    void Encode(ReadOnlyStream<T>* in, WriteOnlyStream<T>* out) {
        inBytes = outBytes = 0;
        codec->Reset(); // кодек со внутренним состоянием начинает проход с чистого листа
        if (!in->IsOpen()) in->Open();
        if (!out->IsOpen()) out->Open();
        while (!in->IsEndOfStream()) {
            size_t cnt = FillBuffer(in, bufferSize);
            inBytes += cnt;
            FlushEncoded(out, cnt);
        }
    }

    void Decode(ReadOnlyStream<T>* in, WriteOnlyStream<T>* out) {
        inBytes = outBytes = 0;
        codec->Reset();
        if (!in->IsOpen()) in->Open();
        if (!out->IsOpen()) out->Open();
        while (!in->IsEndOfStream()) {
            size_t cnt = FillBuffer(in, bufferSize);
            inBytes += cnt;
            FlushDecoded(out, cnt);
        }
    }

    void EncodeN(ReadOnlyStream<T>* in, WriteOnlyStream<T>* out, size_t n) { // кодирование первых n эл
        inBytes = outBytes = 0;
        codec->Reset();
        if (!in->IsOpen()) in->Open();
        if (!out->IsOpen()) out->Open();
        size_t remain = n;
        while (remain > 0 && !in->IsEndOfStream()) {
            size_t want = remain < bufferSize ? remain : bufferSize;
            size_t cnt = FillBuffer(in, want);
            inBytes += cnt;
            FlushEncoded(out, cnt);
            remain -= cnt;
        }
    }

    size_t GetBufferSize() const { return bufferSize; }
    size_t GetBytesIn() const { return inBytes; }
    size_t GetBytesOut() const { return outBytes; }
};

#endif // STREAM_CODER_HPP