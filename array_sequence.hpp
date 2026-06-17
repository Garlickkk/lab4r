#ifndef ARRAY_SEQUENCE_HPP
#define ARRAY_SEQUENCE_HPP

#include "sequence.hpp"
#include "dynamic_array.hpp"


template<class T>
class ArraySequence : public Sequence<T> {
protected:
    DynamicArray<T> *items; // хранилище с запасом: ёмкость массива >= count
    int count; // фактическое число элементов

    virtual ArraySequence<T> *Instance() = 0; // mutable возвращает this, immutable - копию

    void EnsureCapacity(int needed) {
        if (needed <= items->GetSize()) return;
        int newCapacity = items->GetSize() == 0 ? 1 : items->GetSize() * 2;
        if (newCapacity < needed) newCapacity = needed;
        items->Resize(newCapacity);
    }

    void AppendInternal(T item) {
        EnsureCapacity(count + 1);
        items->Set(count, item);
        count++;
    }

    void PrependInternal(T item) {
        InsertAtInternal(item, 0);
    }

    void InsertAtInternal(T item, int index) {
        if (index < 0 || index > count) {
            throw IndexOutOfRangeException(index, count);
        }
        EnsureCapacity(count + 1);
        for (int i = count; i > index; i--) {
            items->Set(i, items->Get(i - 1));
        }
        items->Set(index, item);
        count++;
    }

public:
    ArraySequence() : items(new DynamicArray<T>(0)), count(0) {
    }

    ArraySequence(T *data, int n) : items(new DynamicArray<T>(data, n)), count(n) {
    }

    ArraySequence(const DynamicArray<T> &array) : items(new DynamicArray<T>(array)), count(array.GetSize()) {
    }

    ArraySequence(const ArraySequence<T> &other) : items(new DynamicArray<T>(*other.items)), count(other.count) {
    }

    ~ArraySequence() override { delete items; }

    T GetFirst() const override {
        if (count == 0)
            throw IndexOutOfRangeException("Последовательность пуста");
        return items->Get(0);
    }

    T GetLast() const override {
        if (count == 0)
            throw IndexOutOfRangeException("Последовательность пуста");
        return items->Get(count - 1);
    }

    T Get(int index) const override {
        if (index < 0 || index >= count) {
            throw IndexOutOfRangeException(index, count);
        }
        return items->Get(index);
    }

    int GetLength() const override {
        return count;
    }

    Sequence<T> *GetSubsequence(int startIndex, int endIndex) const override;

    Sequence<T> *Append(T item) override {
        ArraySequence<T> *inst = Instance();
        inst->AppendInternal(item);
        return inst;
    }

    Sequence<T> *Prepend(T item) override {
        ArraySequence<T> *inst = Instance();
        inst->PrependInternal(item);
        return inst;
    }

    Sequence<T> *InsertAt(T item, int index) override {
        ArraySequence<T> *inst = Instance();
        inst->InsertAtInternal(item, index);
        return inst;
    }

    Sequence<T> *Concat(Sequence<T> *other) override;

    Sequence<T> *Empty() const override;
};

template<class T>
class MutableArraySequence;

template<class T>
Sequence<T> *ArraySequence<T>::Empty() const {
    return new MutableArraySequence<T>();
}

template<class T>
Sequence<T> *ArraySequence<T>::GetSubsequence(int startIndex, int endIndex) const {
    if (startIndex < 0 || startIndex >= count)
        throw IndexOutOfRangeException(startIndex, count);
    if (endIndex < 0 || endIndex >= count)
        throw IndexOutOfRangeException(endIndex, count);
    if (endIndex < startIndex)
        throw IndexOutOfRangeException("startIndex > endIndex");

    int n = endIndex - startIndex + 1;
    T *buf = new T[n];
    for (int i = 0; i < n; i++) {
        buf[i] = items->Get(startIndex + i);
    }

    auto *result = new MutableArraySequence<T>(buf, n);
    delete[] buf;
    return result;
}

template<class T>
Sequence<T> *ArraySequence<T>::Concat(Sequence<T> *other) {
    ArraySequence<T> *inst = Instance();
    for (int i = 0; i < other->GetLength(); i++) {
        inst->AppendInternal(other->Get(i));
    }
    return inst;
}

//Mutable
template<class T>
class MutableArraySequence : public ArraySequence<T> {
protected:
    ArraySequence<T> *Instance() override {
        return this;
    }

public:
    using ArraySequence<T>::ArraySequence;
    MutableArraySequence(const ArraySequence<T> &other) : ArraySequence<T>(other) {
    }
};

//Immutable
template<class T>
class ImmutableArraySequence : public ArraySequence<T> {
protected:
    ArraySequence<T> *Instance() override {
        return new MutableArraySequence<T>(static_cast<const ArraySequence<T> &>(*this));
    }

public:
    using ArraySequence<T>::ArraySequence;

    ImmutableArraySequence(const ArraySequence<T> &other) : ArraySequence<T>(other) {
    }
};

#endif // ARRAY_SEQUENCE_HPP
