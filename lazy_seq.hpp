#ifndef LAZY_SEQ_HPP
#define LAZY_SEQ_HPP

#include <functional>
#include "array_sequence.hpp"
#include "exceptions.hpp"

template <class T>
class LazySequence;

template <class T>
class Optional { // для trygetnext
private:
    T* val; //nullptr = > контейнер пуст

public:
    Optional() : val(nullptr) {} // констр по ум

    explicit Optional(T v) : val(new T(v)) {}

    Optional(const Optional<T>& other) { // конструктор копироваения
        if (other.val != nullptr) {
            val = new T(*other.val);
        } else {
            val = nullptr;
        }
    }

    Optional<T>& operator=(const Optional<T>& other) { // оператор присваивания
        if (this != &other) {
            delete val;
            val = other.val ? new T(*other.val) : nullptr; // новое -копируем, иначе зануляем
        }
        return *this;
    }

    ~Optional() { delete val; }

    bool HasValue() const { return val != nullptr; }

    T Value() const {
        if (!val) {
            throw EmptyContainerException("Optional пуст");
        }
        return *val;
    }
};

template <class T>
struct LazyModification {
    int kind;        // 0 = insert, 1 = remove
    int sourceIndex; // индекс в ориг п-ти
    T value; // значение для вставки

    LazyModification() : kind(0), sourceIndex(0), value() {}

    static LazyModification<T> Insert(int idx, T v) {
        LazyModification<T> m;
        m.kind = 0;
        m.sourceIndex = idx;
        m.value = v;
        return m;
    }

    static LazyModification<T> Remove(int idx) {
        LazyModification<T> m;
        m.kind = 1;
        m.sourceIndex = idx;
        m.value = T();
        return m;
    }
};

template <class T>
class Generator {
private:
    LazySequence<T>* owner; // ук на ориг ленивую п-ть
    int position; // сколько элементов отдали наружу
    int sourcePos; // позиция чтения внутри owner
    int currentMod; // какую правку ждем сейчас
    MutableArraySequence<LazyModification<T>> mods; // массив всех правок

    void InsertSorted(LazyModification<T> m) {
        int i = 0;
        int n = mods.GetLength();
        while (i < n) {
            const LazyModification<T>& c = mods.Get(i);
            if (c.sourceIndex < m.sourceIndex) { i++; continue; }
            if (c.sourceIndex == m.sourceIndex && c.kind <= m.kind) { i++; continue; }
            break;
        }
        mods.InsertAt(m, i);
    }

    void Skip() {
        while (currentMod < mods.GetLength()) {
            const LazyModification<T>& m = mods.Get(currentMod);
            if (m.kind == 1 && m.sourceIndex == sourcePos) {
                sourcePos++; // перешагиваем эл в оригинале
                currentMod++;// сдвигаем закладку
            } else {
                break;
            }
        }
    }

public:
    Generator(LazySequence<T>* owner) // Базовый конструктор генератора без модификаций
        : owner(owner), position(0), sourcePos(0), currentMod(0), mods() {}

    Generator(LazySequence<T>* owner, int index, T itemToInsert) // Конструктор генератора с первичной одиночной вставкой
        : owner(owner), position(0), sourcePos(0), currentMod(0), mods() {
        if (index < 0) throw IndexOutOfRangeException(index, 0);
        InsertSorted(LazyModification<T>::Insert(index, itemToInsert));// Создаем правку вставки и добавляем ее
    }

    Generator(LazySequence<T>* owner, int index, Sequence<T>* itemsToInsert)
        : owner(owner), position(0), sourcePos(0), currentMod(0), mods() {
        if (index < 0) throw IndexOutOfRangeException(index, 0);
        for (int i = 0; i < itemsToInsert->GetLength(); i++) {
            InsertSorted(LazyModification<T>::Insert(index, itemsToInsert->Get(i))); // Вставляем каждый элемент
        }
    }

    Generator(const Generator<T>& other) // Конструктор копирования генератора
        : owner(other.owner), position(other.position), sourcePos(other.sourcePos),
          currentMod(other.currentMod), mods(other.mods) {}

    bool HasNext() const {
        if (owner->IsInfinite()) return true;

        int remaining = owner->GetLength() - sourcePos;
        if (remaining < 0) remaining = 0;

        for (int i = currentMod; i < mods.GetLength(); i++) {
            const LazyModification<T>& m = mods.Get(i);
            if (m.kind == 0) {
                remaining++;
            } else if (m.sourceIndex >= sourcePos && m.sourceIndex < owner->GetLength()) {
                remaining--; // удаляем вычтенный элемент
            }
        }
        return remaining > 0;
    }

    T GetNext() {
        if (!HasNext()) {
            throw IndexOutOfRangeException("Достигнут конец последовательности");
        }
        Skip();

        if (currentMod < mods.GetLength()) {
            const LazyModification<T>& m = mods.Get(currentMod);
            if (m.kind == 0 && m.sourceIndex == sourcePos) {
                T v = m.value;
                currentMod++;
                position++;
                return v;
            }
        }

        T v = owner->Get(sourcePos);
        sourcePos++;
        position++;
        return v;
    }

    Optional<T> TryGetNext() {
        if (!HasNext()) return Optional<T>();
        return Optional<T>(GetNext());
    }

    void Reset() {
        position = 0;
        sourcePos = 0;
        currentMod = 0;
    }

    int GetPosition() const { return position; }

    Generator<T>* InsertAt(T item, int index) const {
        if (index < 0) throw IndexOutOfRangeException(index, 0);
        Generator<T>* g = new Generator<T>(*this);
        g->InsertSorted(LazyModification<T>::Insert(index, item));
        return g;
    }

    Generator<T>* InsertAt(Sequence<T>* items, int index) const {
        if (index < 0) throw IndexOutOfRangeException(index, 0);
        Generator<T>* g = new Generator<T>(*this);
        for (int i = 0; i < items->GetLength(); i++) {
            g->InsertSorted(LazyModification<T>::Insert(index, items->Get(i)));
        }
        return g;
    }

    Generator<T>* RemoveAt(int index) const {
        if (index < 0) throw IndexOutOfRangeException(index, 0);
        Generator<T>* g = new Generator<T>(*this);
        g->InsertSorted(LazyModification<T>::Remove(index));
        return g;
    }

    Generator<T>* Append(T item) const {
        if (owner->IsInfinite()) return new Generator<T>(*this);
        Generator<T>* g = new Generator<T>(*this);
        g->InsertSorted(LazyModification<T>::Insert(owner->GetLength(), item));
        return g;
    }

    Generator<T>* Append(Sequence<T>* items) const {
        if (owner->IsInfinite()) return new Generator<T>(*this);
        Generator<T>* g = new Generator<T>(*this);
        for (int i = 0; i < items->GetLength(); i++) {
            g->InsertSorted(LazyModification<T>::Insert(owner->GetLength(), items->Get(i)));
        }
        return g;
    }
};

template <class T>
struct ShiftRule {
    LazySequence<T>* self; // Указатель на оригинал
    explicit ShiftRule(LazySequence<T>* s) : self(s) {}
    T operator()(Sequence<T>* curr) const {
        int n = curr->GetLength();
        return self->Get(n - 1);
    }
};

template <class T>
struct ConcatRule {
    LazySequence<T>* lazyOther;
    int firstLen;
    ConcatRule(LazySequence<T>* other, int len) : lazyOther(other), firstLen(len) {}
    T operator()(Sequence<T>* curr) const {
        int n = curr->GetLength();
        return lazyOther->Get(n - firstLen);
    }
};

template <class T>
struct MapRule {
    LazySequence<T>* self;
    std::function<T(T)> f;
    MapRule(LazySequence<T>* s, std::function<T(T)> func) : self(s), f(func) {}
    T operator()(Sequence<T>* curr) const {
        int n = curr->GetLength();
        return f(self->Get(n));
    }
};

template <class T>
struct ZipRule {
    LazySequence<T>* selfPtr;
    LazySequence<T>* otherPtr;
    std::function<T(T, T)> f;
    ZipRule(LazySequence<T>* s, LazySequence<T>* o, std::function<T(T, T)> func)
        : selfPtr(s), otherPtr(o), f(func) {}
    T operator()(Sequence<T>* curr) const {
        int n = curr->GetLength();
        return f(selfPtr->Get(n), otherPtr->Get(n));
    }
};

template <class T>
class LazySequence : public Sequence<T> {
private:
    mutable ArraySequence<T>* storage;
    std::function<T(Sequence<T>*)> rule;
    bool hasRule;
    int totalLength;

    void GrowOne(T value) const {
        Sequence<T>* next = storage->Append(value);
        if (next != storage) {
            delete storage;
            storage = static_cast<ArraySequence<T>*>(next);
        }
    }

    void Materialize(int upTo) const {
        if (upTo < 0) {
            throw IndexOutOfRangeException(upTo, 0);
        }
        if (totalLength >= 0 && upTo >= totalLength) {
            throw IndexOutOfRangeException(upTo, totalLength);
        }
        if (!hasRule) {
            if (upTo >= storage->GetLength()) {
                throw IndexOutOfRangeException(upTo, storage->GetLength());
            }
            return;
        }
        while (storage->GetLength() <= upTo) {
            T v = rule(storage);
            GrowOne(v);
        }
    }

public:
    LazySequence()
        : storage(new ImmutableArraySequence<T>()),
          hasRule(false), totalLength(0) {}

    LazySequence(T* items, int count)
        : storage(new ImmutableArraySequence<T>(items, count)),
          hasRule(false), totalLength(count) {}

    LazySequence(Sequence<T>* seq)
        : storage(new ImmutableArraySequence<T>()),
          hasRule(false), totalLength(0) {
        for (int i = 0; i < seq->GetLength(); i++) {
            GrowOne(seq->Get(i));
            totalLength++;
        }
    }

    LazySequence(std::function<T(Sequence<T>*)> r, Sequence<T>* initial, int total = -1)
        : storage(new ImmutableArraySequence<T>()),
          rule(r), hasRule(true), totalLength(total) {
        for (int i = 0; i < initial->GetLength(); i++) {
            GrowOne(initial->Get(i));
        }
    }

    LazySequence(const LazySequence<T>& other) {
        storage = new ImmutableArraySequence<T>();
        LazySequence<T>* parent = const_cast<LazySequence<T>*>(&other);
        rule = [parent](Sequence<T>* curr) { return parent->Get(curr->GetLength()); };
        hasRule = true;
        totalLength = other.totalLength;
    }

    LazySequence<T>& operator=(const LazySequence<T>& other) {
        if (this != &other) {
            delete storage;
            storage = new ImmutableArraySequence<T>();
            LazySequence<T>* parent = const_cast<LazySequence<T>*>(&other);
            rule = [parent](Sequence<T>* curr) { return parent->Get(curr->GetLength()); };
            hasRule = true;
            totalLength = other.totalLength;
        }
        return *this;
    }

    ~LazySequence() override { delete storage; }

    T GetFirst() const override {
        if (totalLength == 0) {
            throw EmptyContainerException("Ленивая последовательность пуста");
        }
        Materialize(0);
        return storage->Get(0);
    }

    T GetLast() const override {
        if (totalLength < 0) {
            throw InfiniteSequenceException("Нельзя получить последний элемент бесконечной последовательности");
        }
        if (totalLength == 0) {
            throw EmptyContainerException("Ленивая последовательность пуста");
        }
        Materialize(totalLength - 1);
        return storage->Get(totalLength - 1);
    }

    T Get(int index) const override {
        if (index < 0) {
            throw IndexOutOfRangeException(index, totalLength < 0 ? 0 : totalLength);
        }
        if (totalLength >= 0 && index >= totalLength) {
            throw IndexOutOfRangeException(index, totalLength);
        }
        Materialize(index);
        return storage->Get(index);
    }

    int GetLength() const override { return totalLength; }

    bool IsInfinite() const { return totalLength < 0; }

    int GetMaterializedCount() const { return storage->GetLength(); }

    LazySequence<T>* GetSubsequence(int startIndex, int endIndex) const override {
        if (startIndex < 0) {
            throw IndexOutOfRangeException(startIndex, totalLength);
        }
        if (totalLength >= 0 && endIndex >= totalLength) {
            throw IndexOutOfRangeException(endIndex, totalLength);
        }
        if (endIndex < startIndex) {
            throw InvalidArgumentException("startIndex > endIndex");
        }
        Materialize(endIndex);
        LazySequence<T>* result = new LazySequence<T>();
        for (int i = startIndex; i <= endIndex; i++) {
            result->GrowOne(storage->Get(i));
            result->totalLength++;
        }
        return result;
    }

    LazySequence<T>* Append(T item) override {
        if (IsInfinite()) {
            return new LazySequence<T>(*this);
        }
        LazySequence<T>* result = new LazySequence<T>(*this);
        result->GrowOne(item);
        result->totalLength++;
        return result;
    }

    LazySequence<T>* Prepend(T item) override {
        if (!IsInfinite()) {
            LazySequence<T>* result = new LazySequence<T>();
            result->GrowOne(item);
            result->totalLength = 1;
            for (int i = 0; i < totalLength; i++) {
                result->GrowOne(Get(i));
                result->totalLength++;
            }
            return result;
        }
        LazySequence<T>* self = const_cast<LazySequence<T>*>(this);
        MutableArraySequence<T> initial;
        initial.Append(item);

        ShiftRule<T> newRule(self);
        return new LazySequence<T>(newRule, &initial, -1);
    }

    LazySequence<T>* InsertAt(T item, int index) override {
        if (!IsInfinite()) {
            if (index < 0 || index > totalLength) {
                throw IndexOutOfRangeException(index, totalLength);
            }
            LazySequence<T>* result = new LazySequence<T>();
            for (int i = 0; i < index; i++) {
                result->GrowOne(Get(i));
                result->totalLength++;
            }
            result->GrowOne(item);
            result->totalLength++;
            for (int i = index; i < totalLength; i++) {
                result->GrowOne(Get(i));
                result->totalLength++;
            }
            return result;
        }
        if (index < 0) {
            throw IndexOutOfRangeException(index, -1);
        }
        LazySequence<T>* self = const_cast<LazySequence<T>*>(this);
        MutableArraySequence<T> initial;
        for (int i = 0; i < index; i++) initial.Append(Get(i));
        initial.Append(item);

        ShiftRule<T> newRule(self);
        return new LazySequence<T>(newRule, &initial, -1);
    }

    LazySequence<T>* Concat(Sequence<T>* other) override {
        LazySequence<T>* lazyOther = static_cast<LazySequence<T>*>(other);
        if (IsInfinite()) {
            return new LazySequence<T>(*this);
        }
        if (!lazyOther->IsInfinite()) {
            LazySequence<T>* result = new LazySequence<T>(*this);
            for (int i = 0; i < lazyOther->totalLength; i++) {
                result->GrowOne(lazyOther->Get(i));
                result->totalLength++;
            }
            return result;
        }
        MutableArraySequence<T> initial;
        for (int i = 0; i < totalLength; i++) initial.Append(Get(i));
        initial.Append(lazyOther->Get(0));

        ConcatRule<T> newRule(lazyOther, totalLength);
        return new LazySequence<T>(newRule, &initial, -1);
    }

    Sequence<T>* Empty() const override {
        return new LazySequence<T>();
    }

    LazySequence<T>* Map(std::function<T(T)> f) const {
        if (!IsInfinite()) {
            LazySequence<T>* result = new LazySequence<T>();
            for (int i = 0; i < totalLength; i++) {
                result->GrowOne(f(Get(i)));
                result->totalLength++;
            }
            return result;
        }
        LazySequence<T>* self = const_cast<LazySequence<T>*>(this);
        MutableArraySequence<T> initial;
        initial.Append(f(self->Get(0)));

        MapRule<T> newRule(self, f);
        return new LazySequence<T>(newRule, &initial, -1);
    }

    LazySequence<T>* Zip(LazySequence<T>* other, std::function<T(T, T)> f) const {
        bool infA = IsInfinite();
        bool infB = other->IsInfinite();
        if (infA && infB) {
            LazySequence<T>* selfPtr = const_cast<LazySequence<T>*>(this);
            LazySequence<T>* otherPtr = other;
            MutableArraySequence<T> initial;
            initial.Append(f(selfPtr->Get(0), otherPtr->Get(0)));

            ZipRule<T> newRule(selfPtr, otherPtr, f);
            return new LazySequence<T>(newRule, &initial, -1);
        }
        int n;
        if (!infA && !infB) {
            n = totalLength < other->totalLength ? totalLength : other->totalLength;
        } else if (infA) {
            n = other->totalLength;
        } else {
            n = totalLength;
        }
        LazySequence<T>* result = new LazySequence<T>();
        for (int i = 0; i < n; i++) {
            result->GrowOne(f(Get(i), other->Get(i)));
            result->totalLength++;
        }
        return result;
    }

    LazySequence<T>* Where(std::function<bool(T)> f) const {
        if (IsInfinite()) {
            throw InfiniteSequenceException("Where бесконечной последовательности");
        }
        LazySequence<T>* result = new LazySequence<T>();
        for (int i = 0; i < totalLength; i++) {
            T v = Get(i);
            if (f(v)) {
                result->GrowOne(v);
                result->totalLength++;
            }
        }
        return result;
    }

    T Reduce(std::function<T(T, T)> f, T init) const {
        if (IsInfinite()) {
            throw InfiniteSequenceException("Reduce бесконечной последовательности");
        }
        T acc = init;
        for (int i = 0; i < totalLength; i++) {
            acc = f(acc, Get(i));
        }
        return acc;
    }

    Generator<T>* CreateGenerator() {
        return new Generator<T>(this);
    }
};

#endif // LAZY_SEQ_HPP