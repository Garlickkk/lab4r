#ifndef FILE_STREAMS_HPP
#define FILE_STREAMS_HPP

#include "streams.hpp"
#include "exceptions.hpp"
#include <fstream>
#include <string>

class FileReadStream : public ReadOnlyStream<char> {// чтение из файла
private:
    std::string path; // путь к файлу пользователя
    std::ifstream* fin; // ук на поток чтения
    bool eof; // флаг конца файла
    size_t position; // инд прочитанного байта

    void RefreshEof() { eof = (fin == nullptr) || (fin->peek() == EOF); } // не закончился ли файл?

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

class FileWriteStream : public WriteOnlyStream<char> { // запись в файл
private:
    std::string path; // путь
    std::ofstream* fout; // ук на поток записи
    size_t position; // ск уже записали

public:
    FileWriteStream(const std::string& p) : path(p), fout(nullptr), position(0) {} // конструк сохраняющий пусть
    ~FileWriteStream() override { Close(); }

    void Open() override {
        if (fout != nullptr) Close();
        fout = new std::ofstream(path, std::ios::binary); // создает новый объект в куче и открывает
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

#endif // FILE_STREAMS_HPP
