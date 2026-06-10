#ifndef ARRAY_SEQUENCE_HPP
#define ARRAY_SEQUENCE_HPP

#include "sequence.hpp"
#include "dynamic_array.hpp"


template<class T>
class ArraySequence : public Sequence<T> {
protected:
    DynamicArray<T> *items;


    virtual ArraySequence<T> *Instance() = 0; // mutable возвращает this, immutable - копию


    void AppendInternal(T item) {
        items->Resize(items->GetSize() + 1);
        items->Set(items->GetSize() - 1, item);
    }

    void PrependInternal(T item) {
        int oldSize = items->GetSize();
        items->Resize(oldSize + 1);
        for (int i = oldSize; i > 0; i--) {
            items->Set(i, items->Get(i - 1));
        }
        items->Set(0, item);
    }

    void InsertAtInternal(T item, int index) {
        if (index < 0 || index > items->GetSize()) {
            throw IndexOutOfRangeException(index, items->GetSize());
        }
        int oldSize = items->GetSize();
        items->Resize(oldSize + 1);
        for (int i = oldSize; i > index; i--) {
            items->Set(i, items->Get(i - 1));
        }
        items->Set(index, item);
    }

public:
    ArraySequence() : items(new DynamicArray<T>(0)) {
    }

    ArraySequence(T *data, int count) : items(new DynamicArray<T>(data, count)) {
    }

    ArraySequence(const DynamicArray<T> &array) : items(new DynamicArray<T>(array)) {
    }

    ArraySequence(const ArraySequence<T> &other) : items(new DynamicArray<T>(*other.items)) {
    }

    ~ArraySequence() override { delete items; }

    T GetFirst() const override {
        if (items->GetSize() == 0)
            throw IndexOutOfRangeException("Последовательность пуста");
        return items->Get(0);
    }

    T GetLast() const override {
        if (items->GetSize() == 0)
            throw IndexOutOfRangeException("Последовательность пуста");
        return items->Get(items->GetSize() - 1);
    }

    T Get(int index) const override {
        return items->Get(index);
    }

    int GetLength() const override {
        return items->GetSize();
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
    if (startIndex < 0 || startIndex >= items->GetSize())
        throw IndexOutOfRangeException(startIndex, items->GetSize());
    if (endIndex < 0 || endIndex >= items->GetSize())
        throw IndexOutOfRangeException(endIndex, items->GetSize());
    if (endIndex < startIndex)
        throw IndexOutOfRangeException("startIndex > endIndex");

    int count = endIndex - startIndex + 1;
    T *buf = new T[count];
    for (int i = 0; i < count; i++) {
        buf[i] = items->Get(startIndex + i);
    }

    auto *result = new MutableArraySequence<T>(buf, count);
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
    void RemoveFirst() {
        if (this->items->GetSize() == 0) {
            throw EmptyContainerException("Последовательность пуста");
        }
        int oldSize = this->items->GetSize();
        for (int i = 0; i < oldSize - 1; i++) {
            this->items->Set(i, this->items->Get(i + 1));
        }
        this->items->Resize(oldSize - 1);
    }

    void RemoveLast() {
        if (this->items->GetSize() == 0) {
            throw EmptyContainerException("Последовательность пуста");
        }
        this->items->Resize(this->items->GetSize() - 1);
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