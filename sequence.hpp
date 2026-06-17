#ifndef SEQUENCE_HPP
#define SEQUENCE_HPP

#include <functional>
#include <algorithm>

template <class T>
class Sequence {
public:
    virtual T GetFirst() const = 0;
    virtual T GetLast() const = 0;
    virtual T Get(int index) const = 0;
    virtual Sequence<T>* GetSubsequence(int startIndex, int endIndex) const = 0;
    virtual int GetLength() const = 0;


    virtual Sequence<T>* Append(T item) = 0;
    virtual Sequence<T>* Prepend(T item) = 0;
    virtual Sequence<T>* InsertAt(T item, int index) = 0;
    virtual Sequence<T>* Concat(Sequence<T>* other) = 0;

    virtual Sequence<T>* Empty() const = 0; // пустая последовтальнолсть

    Sequence<T>* Map(std::function<T(T)> f) const {
        Sequence<T>* result = Empty();
        for (int i = 0; i < GetLength(); i++) {
            Sequence<T>* next = result->Append(f(Get(i)));
            if (next != result) {
                delete result;
                result = next;
            }
        }
        return result;
    }

    Sequence<T>* Map(std::function<T(T, int)> f) const {
        Sequence<T>* result = Empty();
        for (int i = 0; i < GetLength(); i++) {
            Sequence<T>* next = result->Append(f(Get(i), i));
            if (next != result) {
                delete result;
                result = next;
            }
        }
        return result;
    }

    Sequence<T>* Where(std::function<bool(T)> f) const {
        Sequence<T>* result = Empty();
        for (int i = 0; i < GetLength(); i++) {
            if (f(Get(i))) {
                Sequence<T>* next = result->Append(Get(i));
                if (next != result) {
                    delete result;
                    result = next;
                }
            }
        }
        return result;
    }

    T Reduce(std::function<T(T, T)> f, T init) const {
        T acc = init;
        for (int i = 0; i < GetLength(); i++) {
            acc = f(acc, Get(i));
        }
        return acc;
    }

    Sequence<T>* Zip(Sequence<T>* other, std::function<T(T, T)> f) const {
        int len = std::min(GetLength(), other->GetLength());
        Sequence<T>* result = Empty();
        for (int i = 0; i < len; i++) {
            Sequence<T>* next = result->Append(f(Get(i), other->Get(i)));
            if (next != result) {
                delete result;
                result = next;
            }
        }
        return result;
    }

    Sequence<T>* FlatMap(std::function<Sequence<T>*(T)> f) const {
        Sequence<T>* result = Empty();
        for (int i = 0; i < GetLength(); i++) {
            Sequence<T>* sub = f(Get(i));
            for (int j = 0; j < sub->GetLength(); j++) {
                Sequence<T>* next = result->Append(sub->Get(j));
                if (next != result) {
                    delete result;
                    result = next;
                }
            }
            delete sub;
        }
        return result;
    }

    Sequence<T>* Skip(int n) const {
        Sequence<T>* result = Empty();
        for (int i = n; i < GetLength(); i++) {
            Sequence<T>* next = result->Append(Get(i));
            if (next != result) {
                delete result;
                result = next;
            }
        }
        return result;
    }

    Sequence<T>* Take(int n) const {
        Sequence<T>* result = Empty();
        int limit = std::min(n, GetLength());
        for (int i = 0; i < limit; i++) {
            Sequence<T>* next = result->Append(Get(i));
            if (next != result) {
                delete result;
                result = next;
            }
        }
        return result;
    }


    T operator[](int index) const {
        return Get(index);
    }


    virtual ~Sequence() = default;
};

#endif // SEQUENCE_HPP