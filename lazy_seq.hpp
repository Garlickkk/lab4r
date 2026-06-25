#ifndef LAZY_SEQ_HPP
#define LAZY_SEQ_HPP

#include <functional>
#include <algorithm>
#include "array_sequence.hpp"
#include "exceptions.hpp"

template <class T>
class LazySequence; // для генератора

template <class T>
class Optional {
private:
    T val; // значение
    bool has_value; // Флаг наличия

public:
    Optional() : val(), has_value(false) {} // Конструктор по умолчанию: значения нет
    explicit Optional(T v) : val(v), has_value(true) {}// Конструктор со значением (запрещено неявное приведение)

    bool HasValue() const { return has_value; }

    T Value() const {
        if (!has_value) {
            throw EmptyContainerException("Optional пуст");
        }
        return val;
    }
};

template <class T>
struct LazyModification {
    bool isInsert; // true = вставка, false = удаление
    int sourceIndex; // индекс в ориг п-ти
    T value;

    LazyModification() : isInsert(true), sourceIndex(0), value() {}

    static LazyModification<T> Insert(int idx, T v) { // статик для того, чтобы вызвать функцию без создания объекта каждый раз
        LazyModification<T> m;
        m.isInsert = true;
        m.sourceIndex = idx;
        m.value = v;
        return m;
    }

    static LazyModification<T> Remove(int idx) {
        LazyModification<T> m;
        m.isInsert = false;
        m.sourceIndex = idx;
        m.value = T();
        return m;
    }
};

template <class T>
class GeneratorWindow : public Sequence<T> { // кольцевой буфер
private:
    DynamicArray<T> buf;// массив в куче, где лежат элементы
    int capacity; // Максимальная вместимость
    int size; // Эл в буфере
    int head;  // Индекс старого элемента
    int total; // кол-во сгенер эл

public:
    explicit GeneratorWindow(int cap)
    : buf(std::max(1, cap)),
      capacity(std::max(1, cap)),
      size(0), head(0), total(0) {}

    void Push(T v) {
        if (size < capacity) {
            int insertIndex = head + size;
            if (insertIndex >= capacity) {
                insertIndex = insertIndex - capacity;
            }
            buf.Set(insertIndex, v);
            size++;
        } else {
            buf.Set(head, v); // Затираем самый старый
            head++;
            if (head >= capacity) {
                head = 0;     // Закольцовываем, если дошли до конца
            }
        }
        total++;
    }

    int GetLength() const override {
        return total;
    }

    T Get(int index) const override {
        int oldest = total - size;
        if (index < oldest) {
            throw IndexOutOfRangeException("Элемент " + std::to_string(index) +
                " уже вытеснен из окна генератора");
        }
        if (index >= total) {
            throw IndexOutOfRangeException(index, total);
        }
        int localIndex = head + (index - oldest);
        if (localIndex >= capacity) {
            localIndex = localIndex - capacity;
        }
        return buf.Get(localIndex);
    }

    T GetFirst() const override {
        if (total == 0) throw EmptyContainerException("Окно генератора пусто");
        return Get(total - size);
    }

    T GetLast() const override {
        if (total == 0) throw EmptyContainerException("Окно генератора пусто");
        return Get(total - 1);
    }

    Sequence<T>* GetSubsequence(int, int) const override { throw InvalidArgumentException("Запрещено"); }
    Sequence<T>* Append(T) override { throw InvalidArgumentException("Запрещено"); }
    Sequence<T>* Prepend(T) override { throw InvalidArgumentException("Запрещено"); }
    Sequence<T>* InsertAt(T, int) override { throw InvalidArgumentException("Запрещено"); }
    Sequence<T>* Concat(Sequence<T>*) override { throw InvalidArgumentException("Запрещено"); }
    Sequence<T>* Empty() const override { throw InvalidArgumentException("Запрещено"); }
};

template <class T>
class Generator {
public:
    explicit Generator(LazySequence<T>* owner) // базовый конструктор для готовых списков
        : owner(owner), sourcePos(0), currentMod(0), window(nullptr), hasRule(false) {}

    Generator(LazySequence<T>* owner, std::function<T(Sequence<T>*)> r, int windowCap = 1) // вычисление беск
        : owner(owner), sourcePos(0), currentMod(0), rule(r), hasRule(true) {
        window = new GeneratorWindow<T>(windowCap);
    }

    Generator(const Generator<T>& other) // конструктор копирования (insertAt+remove)
        : owner(other.owner), sourcePos(other.sourcePos), mods(other.mods),
          currentMod(other.currentMod), rule(other.rule), hasRule(other.hasRule) {
        if (other.window) {
            window = new GeneratorWindow<T>(other.window->GetLength());
        } else {
            window = nullptr;
        }
    }

    Generator(LazySequence<T>* owner, int index, T item) // вставка 1 эл
        : owner(owner), sourcePos(0), currentMod(0), window(nullptr), hasRule(false) {
        InsertSorted(LazyModification<T>::Insert(index, item));
    }

    Generator(LazySequence<T>* owner, int index, Sequence<T>* items) // вставка массива
        : owner(owner), sourcePos(0), currentMod(0), window(nullptr), hasRule(false) {
        for (int i = 0; i < items->GetLength(); i++) {
            InsertSorted(LazyModification<T>::Insert(index + i, items->Get(i)));
        }
    }

    ~Generator() {
        if (window) delete window;
    }

    bool HasNext() const {
        if (owner->IsInfinite()) return true;

        int remaining = owner->GetLength() - sourcePos;
        if (remaining < 0) remaining = 0;

        for (int i = currentMod; i < mods.GetLength(); i++) {
            const LazyModification<T>& m = mods.Get(i);
            if (m.isInsert) {
                remaining++;
            } else if (m.sourceIndex >= sourcePos && m.sourceIndex < owner->GetLength()) {
                remaining--;
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
            if (m.isInsert && m.sourceIndex == sourcePos) {
                T v = m.value;
                currentMod++;
                return v;
            }
        }

        T v = owner->Get(sourcePos);
        sourcePos++;
        return v;
    }

    Optional<T> TryGetNext() {
        if (HasNext()) {
            return Optional<T>(GetNext());
        }
        return Optional<T>();
    }

    Generator<T>* Append(T item) const { // добавление 1 эл в конец списка
        Generator<T>* g = new Generator<T>(*this);
        g->InsertSorted(LazyModification<T>::Insert(owner->GetLength(), item)); // отлож операция
        return g;
    }

    Generator<T>* Append(Sequence<T>* items) const { // добавление коллекции в конец спсика
        Generator<T>* g = new Generator<T>(*this);
        int endPos = owner->GetLength();
        for (int i = 0; i < items->GetLength(); i++) {
            g->InsertSorted(LazyModification<T>::Insert(endPos + i, items->Get(i)));
        }
        return g;
    }

    Generator<T>* Insert(T item) const { // вставка 1 эл в текущую позицию генератора
        Generator<T>* g = new Generator<T>(*this);
        g->InsertSorted(LazyModification<T>::Insert(sourcePos, item));
        return g;
    }

    Generator<T>* Insert(Sequence<T>* items) const { // вставка коллекции в текущую позицию генератора
        Generator<T>* g = new Generator<T>(*this);
        for (int i = 0; i < items->GetLength(); i++) {
            g->InsertSorted(LazyModification<T>::Insert(sourcePos + i, items->Get(i)));
        }
        return g;
    }

    Generator<T>* Remove(T item) const { // уд первого вхождения по эл
        Generator<T>* g = new Generator<T>(*this);
        int idx = g->FindRemovalIndex(item);
        if (idx != -1) {
            g->InsertSorted(LazyModification<T>::Remove(idx));
        }
        return g;
    }
    Generator<T>* Remove(Sequence<T>* items) const { // удаление всех вхождений коллекции по зачениям
        Generator<T>* g = new Generator<T>(*this);
        for (int i = 0; i < items->GetLength(); i++) {
            int idx = g->FindRemovalIndex(items->Get(i));
            if (idx != -1) {
                g->InsertSorted(LazyModification<T>::Remove(idx)); // Записываем в блокнот
            }
        }
        return g;
    }

private:
    LazySequence<T>* owner;
    int sourcePos;
    MutableArraySequence<LazyModification<T>> mods; // отложенные правки
    int currentMod; // следующая правка в отложенных правках
    GeneratorWindow<T>* window; // кольцевой буфер
    std::function<T(Sequence<T>*)> rule;
    bool hasRule; // флаг работант ли по правилу или просто

    void Skip() {
        while (currentMod < mods.GetLength()) {
            const LazyModification<T>& m = mods.Get(currentMod);
            if (!m.isInsert && m.sourceIndex == sourcePos) {
                sourcePos++;
                currentMod++;
            } else {
                break;
            }
        }
    }

    void InsertSorted(LazyModification<T> m) { // метод для добавления правок с сохранением п-ти
        int i = 0; // нач списка правок
        int n = mods.GetLength(); // кл-во сущ правок
        while (i < n) { // ищем поз до новой вствки
            const LazyModification<T>& c = mods.Get(i); // тек правка
            if (c.sourceIndex < m.sourceIndex) { i++; continue; } // инд правки меньше чем у новой
            if (c.sourceIndex == m.sourceIndex && c.isInsert >= m.isInsert) { i++; continue; } // индексы равны
            break; // нашли место где новая правка стояла раньше
        }
        mods.InsertAt(m, i); // вставляем новыую правку на i
        if (i < currentMod) currentMod++; // перемещение курсора чтобы не выполять старую правку дважды
    }

   bool HasRemoveModAt(int idx) const { // есть ли в списке mods удаление эл с этим ind
        for (int i = 0; i < mods.GetLength(); i++) {
            const LazyModification<T>& m = mods.Get(i);
            if (!m.isInsert && m.sourceIndex == idx) return true;
        }
        return false;
    }

    void CheckRemoveIndex(int index) const { // можно ли уд индекс?
        if (index < 0 || (!owner->IsInfinite() && index >= owner->GetLength())) {
            throw IndexOutOfRangeException("Некорректный индекс для удаления");
        }
    }

    int FindRemovalIndex(T item) const { // поиск индекса первого эл с этим значением
        int limit = owner->IsInfinite() ? 1000 : owner->GetLength(); // если список беск проверяем только 1000
        for (int i = 0; i < limit; i++) {
            if (owner->Get(i) == item && !HasRemoveModAt(i)) return i; // значение совпало и не будет уд
        }
        return -1;
    }
};

template <class T>
struct MapRule {
    LazySequence<T>* self; // ориг список
    std::function<T(T)> f; // ф-ция преобразования
    MapRule(LazySequence<T>* s, std::function<T(T)> func) : self(s), f(func) {} // конструткор
    T operator()(Sequence<T>* curr) const { // перезагрузка оператора круглые скобки
        int n = curr->GetLength(); // текущий шаг
        return f(self->Get(n)); // перед старый эл ф фун преоб
    }
};

template <class T>
struct ZipRule {
    LazySequence<T>* selfPtr; // 1-й список
    LazySequence<T>* otherPtr; // 2-й список
    std::function<T(T, T)> f;
    ZipRule(LazySequence<T>* s, LazySequence<T>* o, std::function<T(T, T)> func)
        : selfPtr(s), otherPtr(o), f(func) {}
    T operator()(Sequence<T>* curr) const { // для сохранения указателей на 2 п-ти
        int n = curr->GetLength();
        return f(selfPtr->Get(n), otherPtr->Get(n));
    }
};

template <class T>
struct SubseqRule {
    Sequence<T>* self; // исх список
    int start; // инд начала вырезания
    SubseqRule(Sequence<T>* s, int st) : self(s), start(st) {}
    T operator()(Sequence<T>* curr) const {
        return self->Get(start + curr->GetLength());
    }
};

template <class T>
struct InsertOneRule { // вставка эл в середиину
    Sequence<T>* self;
    int index; // инд куда вставляем
    T item; // что вставляем
    InsertOneRule(Sequence<T>* s, int i, T v) : self(s), index(i), item(v) {}
    T operator()(Sequence<T>* curr) const {
        int n = curr->GetLength();
        if (n < index) return self->Get(n); // до места вставки
        if (n == index) return item; // на месте вставки
        return self->Get(n - 1); // после места ставки
    }
};

template <class T>
struct ConcatTwoRule {
    Sequence<T>* first; // 1-я часть скл списка
    Sequence<T>* second; // 2-я часть скл списка
    int len; // длина 1-го списка
    ConcatTwoRule(Sequence<T>* a, Sequence<T>* b, int firstLen)
        : first(a), second(b), len(firstLen) {}
    T operator()(Sequence<T>* curr) const {
        int n = curr->GetLength(); // Какой по счету элемент запрашивают
        if (n < len) { // если в 1-й часте
            return first->Get(n);
        } else {
            return second->Get(n - len);
        }
    }
};

template <class T>
class LazySequence : public Sequence<T> {
private:
    mutable ArraySequence<T>* storage; // вычисленные элементы
    std::function<T(Sequence<T>*)> rule; // правило создания
    bool hasRule;
    int totalLength; // длина (-1 - бесконечна)

    void GrowOne(T value) const { // добавление эл в выч
        Sequence<T>* next = storage->Append(value);
        if (next != storage) {
            delete storage;
            storage = static_cast<ArraySequence<T>*>(next);
        }
    }

    void Materialize(int upTo) const {  // доступ к эл через мемоизацию. Если эл уже вычислен (исп в get..)
        if (upTo < 0) {
            throw IndexOutOfRangeException(upTo, 0);
        }
        if (totalLength >= 0 && upTo >= totalLength) {
            throw IndexOutOfRangeException(upTo, totalLength);
        }
        if (!hasRule) {
            if (upTo >= storage->GetLength()) { // storage - эл, которые уже вычислены
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
    LazySequence() // конструктор по ум пуст п-ть
        : storage(new ImmutableArraySequence<T>()),
          hasRule(false), totalLength(0) {}

    LazySequence(T* items, int count) // п-ть из массива (конечна)
        : storage(new ImmutableArraySequence<T>(items, count)),
          hasRule(false), totalLength(count) {}

    LazySequence(Sequence<T>* seq) // копирование из другой
        : storage(new ImmutableArraySequence<T>()),
          hasRule(false), totalLength(0) {
        if (seq->GetLength() < 0) {
            delete storage;
            throw InfiniteSequenceException("Нельзя скопировать бесконечную последовательность поэлементно");
        }
        for (int i = 0; i < seq->GetLength(); i++) {
            GrowOne(seq->Get(i));
            totalLength++;
        }
    }

    LazySequence(std::function<T(Sequence<T>*)> r, Sequence<T>* initial, int total = -1) // с правилом
        : storage(new ImmutableArraySequence<T>()),
          rule(r), hasRule(true), totalLength(total) {
        for (int i = 0; i < initial->GetLength(); i++) {
            GrowOne(initial->Get(i));
        }
    }

    LazySequence(const LazySequence<T>& other) // копирующий конструктор
        : storage(new MutableArraySequence<T>(*other.storage)),
          rule(other.rule), hasRule(other.hasRule), totalLength(other.totalLength) {}

    LazySequence<T>& operator=(const LazySequence<T>& other) { // оператор присваивания
        if (this != &other) {
            ArraySequence<T>* newStorage = new MutableArraySequence<T>(*other.storage);
            delete storage;
            storage = newStorage;
            rule = other.rule;
            hasRule = other.hasRule;
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
        LazySequence<T>* self = const_cast<LazySequence<T>*>(this);
        MutableArraySequence<T> initial;
        SubseqRule<T> r(self, startIndex);
        return new LazySequence<T>(r, &initial, endIndex - startIndex + 1);
    }

    LazySequence<T>* Append(T item) override {
        if (IsInfinite()) {
            return new LazySequence<T>(*this);
        }
        return InsertAt(item, totalLength);
    }

    LazySequence<T>* Prepend(T item) override {
        return InsertAt(item, 0);
    }

    LazySequence<T>* InsertAt(T item, int index) override {
        if (index < 0 || (!IsInfinite() && index > totalLength)) {
            throw IndexOutOfRangeException(index, IsInfinite() ? 0 : totalLength + 1);
        }
        MutableArraySequence<T> initial;
        InsertOneRule<T> r(this, index, item);
        return new LazySequence<T>(r, &initial, IsInfinite() ? -1 : totalLength + 1);
    }

    LazySequence<T>* Concat(Sequence<T>* other) override {
        if (IsInfinite()) {
            return new LazySequence<T>(*this);
        }
        int otherLen = other->GetLength();
        MutableArraySequence<T> initial;
        ConcatTwoRule<T> r(this, other, totalLength);
        return new LazySequence<T>(r, &initial, otherLen < 0 ? -1 : totalLength + otherLen);
    }

    Sequence<T>* Empty() const override {
        return new LazySequence<T>();
    }

    LazySequence<T>* Map(std::function<T(T)> f) const {
        LazySequence<T>* self = const_cast<LazySequence<T>*>(this);
        MutableArraySequence<T> initial;
        MapRule<T> r(self, f);
        return new LazySequence<T>(r, &initial, totalLength);
    }

    LazySequence<T>* Zip(LazySequence<T>* other, std::function<T(T, T)> f) const {
        LazySequence<T>* selfPtr = const_cast<LazySequence<T>*>(this);
        int total;
        if (IsInfinite() && other->IsInfinite()) total = -1;
        else if (IsInfinite()) total = other->totalLength;
        else if (other->IsInfinite()) total = totalLength;
        else total = totalLength < other->totalLength ? totalLength : other->totalLength;

        MutableArraySequence<T> initial;
        ZipRule<T> r(selfPtr, other, f);
        return new LazySequence<T>(r, &initial, total);
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
        if (hasRule) {
            return new Generator<T>(this, rule);
        }
        return new Generator<T>(this);
    }
};

#endif // LAZY_SEQ_HPP