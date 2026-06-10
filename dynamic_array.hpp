#ifndef DYNAMIC_ARRAY_HPP
#define DYNAMIC_ARRAY_HPP
#include <stdexcept>
#include "exceptions.hpp"

template <class T>
class DynamicArray {
private:
    T* data;
    int size;

public:
    DynamicArray(T* items, int count);// Копировать элементы из переданного массива
    DynamicArray(int size); // Создать массив заданной длины
    DynamicArray(const DynamicArray<T>& dynamicArray); // Копирующий конструктор
    DynamicArray<T>& operator=(const DynamicArray<T>& other); // Оператор присваивания
    ~DynamicArray(); // освобождение памяти

    T Get(int index) const;
    int GetSize() const;
    void Set(int index, T value);
    void Resize(int newSize);
};

template <class T>
DynamicArray<T>::DynamicArray(T* items, int count) {
    if (count < 0) {
        throw InvalidArgumentException("размер не может быть отрицательным");
    }
    if (items == nullptr) {
        throw InvalidArgumentException("Указатель не должен 0");
    }
    this->size = count;
    this->data = new T[count]; // выделение памяти
    for (int i = 0; i < count; i++) {
        this->data[i] = items[i];
    }
}

template <class T>
DynamicArray<T>::DynamicArray(int size) { // пустой массив этого размера
    if (size < 0) {
        throw InvalidArgumentException("размер не может быть отрицательным");
    }
    this->size = size;
    this->data = new T[size];
}

template <class T>
DynamicArray<T>::DynamicArray(const DynamicArray<T>& dynamicArray) {
    this->size = dynamicArray.size;
    this->data = new T[this->size];
    for (int i = 0; i < this->size; i++) {
        this->data[i] = dynamicArray.data[i];
    }
}

template <class T>
DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray<T>& other) {
    if (this != &other) {
        delete[] this->data;
        this->size = other.size;
        this->data = new T[this->size];
        for (int i = 0; i < this->size; i++) {
            this->data[i] = other.data[i];
        }
    }
    return *this;
}

template <class T>
DynamicArray<T>::~DynamicArray() {
    delete[] this->data;
}

template<class T>
T DynamicArray<T>::Get(int index) const {
    if (index < 0 || index >= this->size) {
        throw IndexOutOfRangeException(index, this->size);
    }
    return this->data[index];
}

template<class T>
int DynamicArray<T>::GetSize() const {
    return this->size;
}

template<class T>
void DynamicArray<T>::Set(int index, T value) {
    if (index < 0 || index >= this->size) {
        throw IndexOutOfRangeException(index, this->size);
    }
    this->data[index] = value;
}

template<class T>
void DynamicArray<T>::Resize(int newSize) {
    if (newSize < 0) {
        throw InvalidArgumentException("Размер должен быть больше или равен 0");
    }
    T* newData = new T[newSize];
    int copyCount = std::min(this->size, newSize); // кол-во эл которые копируются
    for (int i = 0; i < copyCount; i++) {
        newData[i] = this->data[i];
    }
    delete[] this->data;
    this->data = newData;
    this->size = newSize;
}

#endif