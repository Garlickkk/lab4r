#ifndef STREAMS_HPP
#define STREAMS_HPP

#include "sequence.hpp"
#include "array_sequence.hpp"
#include "lazy_seq.hpp"
#include "exceptions.hpp"
#include <fstream>
#include <string>

template <class T>
class ReadOnlyStream {
protected:
    bool isOpen; // открыт или закры поток

public:
    ReadOnlyStream() : isOpen(false) {}
    virtual ~ReadOnlyStream() = default;

    virtual bool IsEndOfStream() const = 0;
    virtual T Read() = 0;
    virtual size_t GetPosition() const = 0;
    virtual bool IsCanSeek() const = 0; // можно ли обратиться к предыдущим элементам
    virtual size_t Seek(size_t index) = 0; // перем на заданный обр индекс
    virtual bool IsCanGoBack() const = 0; // может ли перем на пред эл

    virtual void Open() { isOpen = true; }
    virtual void Close() { isOpen = false; }

    bool IsOpen() const { return isOpen; }
};

template <class T>
class WriteOnlyStream {
protected:
    bool isOpen;

public:
    WriteOnlyStream() : isOpen(false) {}
    virtual ~WriteOnlyStream() = default;

    virtual size_t Write(T item) = 0;
    virtual size_t GetPosition() const = 0;

    virtual void Open() { isOpen = true; }
    virtual void Close() { isOpen = false; }

    bool IsOpen() const { return isOpen; }
};

template <class T>
class SequenceReadStream : public ReadOnlyStream<T> {
private:
    Sequence<T>* source; // откуда читаем
    size_t position; // индекс след эл для чтения

public:
    SequenceReadStream(Sequence<T>* source) : source(source), position(0) {} // принятие структуры и став на нач эл

    bool IsEndOfStream() const override {
        return static_cast<int>(position) >= source->GetLength(); // поз = дл -> true - закончилась
    }

    T Read() override {
        if (!this->isOpen) throw StreamNotOpenException();
        if (IsEndOfStream()) throw EndOfStreamException();
        T v = source->Get(position);
        position++;
        return v;
    }

    size_t GetPosition() const override { return position; }

    bool IsCanSeek() const override { return true; }

    size_t Seek(size_t index) override {
        if (!this->isOpen) throw StreamNotOpenException();
        if (static_cast<int>(index) > source->GetLength()) {
            position = source->GetLength();
        } else {
            position = index;
        }
        return position;
    }

    bool IsCanGoBack() const override { return true; }
};

template <class T>
class LazyReadStream : public ReadOnlyStream<T> {
private:
    LazySequence<T>* source;
    size_t position; // текущая позиция

public:
    LazyReadStream(LazySequence<T>* source) : source(source), position(0) {} // уст на нулевой элемент

    bool IsEndOfStream() const override {
        if (source->IsInfinite()) return false;
        return static_cast<int>(position) >= source->GetLength();
    }

    T Read() override {
        if (!this->isOpen) throw StreamNotOpenException();
        if (IsEndOfStream()) throw EndOfStreamException();
        T v = source->Get(position);
        position++;
        return v;
    }

    size_t GetPosition() const override { return position; }

    bool IsCanSeek() const override { return true; }

    size_t Seek(size_t index) override {
        if (!this->isOpen) throw StreamNotOpenException();
        if (!source->IsInfinite() && static_cast<int>(index) > source->GetLength()) { // если надо пер на больше чем длина ставим на посл эл
            position = source->GetLength();
        } else {
            position = index;
        }
        return position;
    }

    bool IsCanGoBack() const override { return true; }
};

template <class T>
class SequenceWriteStream : public WriteOnlyStream<T> {
private:
    Sequence<T>* sink; // ук на приемника
    size_t position; // ск эл записали

public:
    SequenceWriteStream(Sequence<T>* sink) : sink(sink), position(sink->GetLength()) {} // конструктор, sink->GetLength() дописываем к имеющимся данным

    size_t Write(T item) override {
        if (!this->isOpen) throw StreamNotOpenException();
        sink->Append(item); // доб в результат
        position++;
        return position;
    }

    size_t GetPosition() const override { return position; }

    Sequence<T>* GetSink() const { return sink; }
};

class FileReadStream : public ReadOnlyStream<char> {
private:
    std::string path; // путь к файлу пользователя
    std::ifstream* fin; // ук на поток чтения
    bool eof; // флаг конца файла
    size_t position; // инд прочитанного байта

    void RefreshEof() { eof = (fin == nullptr) || (fin->peek() == EOF); }

public:
    FileReadStream(const std::string& p) : path(p), fin(nullptr), eof(true), position(0) {} // флаг конца
    ~FileReadStream() override { Close(); }

    void Open() override {
        if (fin != nullptr) Close();
        fin = new std::ifstream(path, std::ios::binary); // открытие файла в бинарном режиме
        if (!fin->is_open()) { delete fin; fin = nullptr; throw StreamNotOpenException("Не удалось открыть файл: " + path); }
        this->isOpen = true;
        position = 0;
        RefreshEof(); // пр-ка не пустой ли файл
    }

    void Close() override {
        if (fin != nullptr) { fin->close(); delete fin; fin = nullptr; }
        this->isOpen = false;
    }

    bool IsEndOfStream() const override { return eof; }

    char Read() override {
        if (!this->isOpen) throw StreamNotOpenException();
        if (eof) throw EndOfStreamException();
        char c;// буфер для символа
        if (!fin->get(c)) { eof = true; throw EndOfStreamException(); }
        position++;
        eof = fin->peek() == EOF;
        return c;
    }

    size_t GetPosition() const override { return position; }
    bool IsCanSeek() const override { return true; }
    size_t Seek(size_t index) override {
        if (!this->isOpen) throw StreamNotOpenException();
        fin->clear();
        fin->seekg(static_cast<std::streamoff>(index), std::ios::beg);
        position = index;
        RefreshEof();
        return position;
    }
    bool IsCanGoBack() const override { return true; }
};

class FileWriteStream : public WriteOnlyStream<char> {
private:
    std::string path; // путь
    std::ofstream* fout; // ук на поток записи
    size_t position; // ск уже записали

public:
    FileWriteStream(const std::string& p) : path(p), fout(nullptr), position(0) {} // конструк сохраняющий пусть
    ~FileWriteStream() override { Close(); }

    void Open() override {
        if (fout != nullptr) Close();
        fout = new std::ofstream(path, std::ios::binary);
        if (!fout->is_open()) { delete fout; fout = nullptr; throw StreamNotOpenException("Не удалось открыть файл для записи: " + path); }
        this->isOpen = true;
        position = 0;
    }

    void Close() override {
        if (fout != nullptr) { fout->close(); delete fout; fout = nullptr; }
        this->isOpen = false;
    }

    size_t Write(char c) override {
        if (!this->isOpen) throw StreamNotOpenException();
        fout->put(c);
        position++;
        return position;
    }

    size_t GetPosition() const override { return position; }
};

#endif // STREAMS_HPP