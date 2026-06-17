#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

class IndexOutOfRangeException : public std::out_of_range {
public:
    explicit IndexOutOfRangeException(const std::string& msg)
        : std::out_of_range(msg) {}

    explicit IndexOutOfRangeException(int index, int size)
        : std::out_of_range("Индекс " + std::to_string(index) +
                           " вышел за пределы [0, " + std::to_string(size) + ")") {}
};

class EmptyContainerException : public IndexOutOfRangeException {
public:
    explicit EmptyContainerException(const std::string& msg)
        : IndexOutOfRangeException(msg) {}

    EmptyContainerException() : IndexOutOfRangeException("Контейнер пуст") {}
};

class InvalidArgumentException : public std::invalid_argument {
public:
    explicit InvalidArgumentException(const std::string& msg)
        : std::invalid_argument(msg) {}
};

class EndOfStreamException : public std::runtime_error {
public:
    EndOfStreamException() : std::runtime_error("Достигнут конец потока") {}
    explicit EndOfStreamException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class StreamNotOpenException : public std::runtime_error {
public:
    StreamNotOpenException() : std::runtime_error("Поток не открыт") {}
    explicit StreamNotOpenException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class InfiniteSequenceException : public std::runtime_error {
public:
    InfiniteSequenceException()
        : std::runtime_error("Операция невозможна для бесконечной последовательности") {}
    explicit InfiniteSequenceException(const std::string& msg)
        : std::runtime_error(msg) {}
};

#endif // EXCEPTIONS_HPP
