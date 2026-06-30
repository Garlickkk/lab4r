#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include "array_sequence.hpp"
#include "lazy_seq.hpp"
#include "streams.hpp"
#include "file_streams.hpp"
#include "stream_coder.hpp"

void runAllTests();

void clearInputBuffer() {
    std::cin.clear();
    std::cin.ignore(10000, '\n');
}

int readInt(const std::string& prompt) { // без чтенение целого
    int value;
    std::cout << prompt;
    while (!(std::cin >> value)) {
        clearInputBuffer();
        std::cout << "Ошибка! Введите целое число: ";
    }
    clearInputBuffer();
    return value;
}

std::string readString(const std::string& prompt) { // без чтение строчки
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

Sequence<char>* StringToSequence(const std::string& s) { // код/дек введенный текст
    auto seq = new MutableArraySequence<char>();
    for (char c : s) seq->Append(c);
    return seq;
}

std::string PrintableSequence(const Sequence<char>* seq, int maxShow = 80) { // для Xor защита от служебных кодов
    std::ostringstream os;
    int n = seq->GetLength();
    int show = std::min(n, maxShow);
    for (int i = 0; i < show; i++) {
        char c = seq->Get(i);
        if (c >= 32 && c < 127) os << c;
        else {
            os << "\\x";
            const char* hex = "0123456789ABCDEF";
            os << hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
            os << hex[static_cast<unsigned char>(c) & 0xF];
        }
    }
    if (n > show) os << " (всего " << n << " символов)";
    return os.str();
}

struct CoderConfig {
    int codecKind = 1;
    int caesarShift = 3;
    std::string xorKey = "K";
    size_t bufferSize = 16;
};

Codec<char>* MakeCodec(const CoderConfig& cfg) {
    switch (cfg.codecKind) {
        case 1: return new CaesarCodec(cfg.caesarShift);
        case 2: return new XorCodec(cfg.xorKey.c_str(), static_cast<int>(cfg.xorKey.size()));
        default: throw InvalidArgumentException("Неизвестный кодек");
    }
}

std::string CodecName(const CoderConfig& cfg) { // какой сейчас кодек
    switch (cfg.codecKind) {
        case 1: return "Caesar(shift=" + std::to_string(cfg.caesarShift) + ")";
        case 2: return "XOR(key=\"" + cfg.xorKey + "\")";
        default: return "?";
    }
}

void Configure(CoderConfig& cfg) { // изменение кодека
    std::cout << "\nНастройка кодека\n";
    cfg.codecKind = readInt("1. Caesar  2. XOR\nВыбор: ");
    if (cfg.codecKind == 1) cfg.caesarShift = readInt("Сдвиг Caesar: ");
    else if (cfg.codecKind == 2) { cfg.xorKey = readString("Ключ XOR: "); if (cfg.xorKey.empty()) cfg.xorKey = "K"; }
    int b = readInt("Размер буфера (>0): ");
    if (b > 0) cfg.bufferSize = static_cast<size_t>(b);
    std::cout << "Сохранено: " << CodecName(cfg) << ", буфер " << cfg.bufferSize << "\n";
}

void RunOnText(const CoderConfig& cfg, bool encode) { // шифр / дешифр текста с клавиаутры
    std::string line = readString("\nВведите текст: ");
    Sequence<char>* input = StringToSequence(line);
    MutableArraySequence<char>* output = new MutableArraySequence<char>();
    SequenceReadStream<char> rs(input);
    SequenceWriteStream<char> ws(output);
    Codec<char>* codec = MakeCodec(cfg);
    StreamCoder<char> coder(codec, cfg.bufferSize);
    try {
        rs.Open(); ws.Open();
        encode ? coder.Encode(&rs, &ws) : coder.Decode(&rs, &ws);
        rs.Close(); ws.Close();
        std::cout << "Результат: " << PrintableSequence(output) << "\n";
    } catch (const std::exception& e) { std::cout << "Ошибка: " << e.what() << "\n"; }
    delete codec; delete input; delete output;
}

void RunOnFile(const CoderConfig& cfg, bool encode) { // шифр/дешифр из файла
    std::string inPath = readString("\nПуть к файлу: ");
    std::string outPath = readString("Путь к выходному: ");
    try {
        FileReadStream rs(inPath);
        FileWriteStream ws(outPath);
        Codec<char>* codec = MakeCodec(cfg);
        StreamCoder<char> coder(codec, cfg.bufferSize);
        rs.Open(); ws.Open();
        encode ? coder.Encode(&rs, &ws) : coder.Decode(&rs, &ws);
        rs.Close(); ws.Close();
        std::cout << "Готово.\n";
        delete codec;
    } catch (const std::exception& e) { std::cout << "Ошибка: " << e.what() << "\n"; }
}

void InteractiveLazyMenu() {
    LazySequence<int>* activeSeq = new LazySequence<int>();
    Generator<int>* activeGen = nullptr;
    bool running = true;

    while (running) {
        std::cout << "\nРучное тестирование" << std::endl;
        std::cout << "Seq: " << (activeSeq->IsInfinite() ? "Бесконечная" : "Длина " + std::to_string(activeSeq->GetLength()))
                  << " | Кэш: " << activeSeq->GetMaterializedCount() << " | Gen: " << (activeGen ? "Активен" : "Нет") << std::endl;

        std::cout << "--- LazySequence ---\n1. Создать (пустую)\n2. Append(val)\n3. Prepend(val)\n4. InsertAt(val, idx)\n5. Concat\n"
                  << "6. GetFirst()\n7. GetLast()\n8. Get(idx)\n9. Map(x*10)\n10. Reduce(sum)\n"
                  << "--- Generator ---\n11. Создать Gen из Seq\n12. GetNext()\n13. Append(val)\n14. Append(items)\n"
                  << "15. Insert(val)\n16. Insert(items)\n17. Remove(val)\n18. Remove(items)\n0. Выход" << std::endl;

        int choice = readInt("Выбор: ");
        try {
            switch (choice) {
                case 1: delete activeSeq; activeSeq = new LazySequence<int>(); break;
                case 2: { int v = readInt("Val: "); LazySequence<int>* n = activeSeq->Append(v); delete activeSeq; activeSeq = n; break; }
                case 3: { int v = readInt("Val: "); LazySequence<int>* n = activeSeq->Prepend(v); delete activeSeq; activeSeq = n; break; }
                case 4: { int v = readInt("Val: "), i = readInt("Idx: "); LazySequence<int>* n = activeSeq->InsertAt(v, i); delete activeSeq; activeSeq = n; break; }
                case 5: { int arr[] = {99, 100}; MutableArraySequence<int> o(arr, 2); LazySequence<int>* n = activeSeq->Concat(&o); delete activeSeq; activeSeq = n; break; }
                case 6: std::cout << "First: " << activeSeq->GetFirst() << std::endl; break;
                case 7: std::cout << "Last: " << activeSeq->GetLast() << std::endl; break;
                case 8: { int i = readInt("Idx: "); std::cout << "Result: " << activeSeq->Get(i) << std::endl; break; }
                case 9: { LazySequence<int>* n = activeSeq->Map([](int x){ return x*10; }); delete activeSeq; activeSeq = n; break; }
                case 10: { std::cout << "Sum: " << activeSeq->Reduce([](int a, int b){ return a+b; }, 0) << std::endl; break; }
                case 11: if (activeGen) delete activeGen; activeGen = activeSeq->CreateGenerator(); break;
                case 12: if (activeGen) std::cout << "Next: " << activeGen->GetNext() << std::endl; break;
                case 13: { int v = readInt("Val: "); Generator<int>* n = activeGen->Append(v); delete activeGen; activeGen = n; break; }
                case 14: { int arr[] = {7, 8}; MutableArraySequence<int> s(arr, 2); Generator<int>* n = activeGen->Append(&s); delete activeGen; activeGen = n; break; }
                case 15: { int v = readInt("Val: "); Generator<int>* n = activeGen->Insert(v); delete activeGen; activeGen = n; break; }
                case 16: { int arr[] = {7, 8}; MutableArraySequence<int> s(arr, 2); Generator<int>* n = activeGen->Insert(&s); delete activeGen; activeGen = n; break; }
                case 17: { int v = readInt("Val: "); Generator<int>* n = activeGen->Remove(v); delete activeGen; activeGen = n; break; }
                case 18: { int arr[] = {7, 8}; MutableArraySequence<int> s(arr, 2); Generator<int>* n = activeGen->Remove(&s); delete activeGen; activeGen = n; break; }
                case 0: running = false; break;
            }
        } catch (const std::exception& e) { std::cout << "Ошибка: " << e.what() << std::endl; }
    }
    delete activeSeq; if (activeGen) delete activeGen;
}

int main() {
    CoderConfig cfg;
    bool running = true;

    while (running) {
        std::cout << "\n=======================================\n";
        std::cout << "  ЛР №4 \n";
        std::cout << "  Кодек: " << CodecName(cfg) << " | Буфер: " << cfg.bufferSize << "\n";
        std::cout << "=========================================\n";
        std::cout << "1. Настроить кодек и буфер\n";
        std::cout << "2. Закодировать введённый текст\n";
        std::cout << "3. Декодировать введённый текст\n";
        std::cout << "4. Закодировать файл\n";
        std::cout << "5. Декодировать файл\n";
        std::cout << "8. Запустить все тесты\n";
        std::cout << "9. Интерактивное тестирование (Lazy/Gen)\n";
        std::cout << "0. Выход\n";

        int choice = readInt("Выбор: ");

        switch (choice) {
            case 1: Configure(cfg); break;
            case 2: RunOnText(cfg, true); break;
            case 3: RunOnText(cfg, false); break;
            case 4: RunOnFile(cfg, true); break;
            case 5: RunOnFile(cfg, false); break;
            case 8: runAllTests(); break;
            case 9: InteractiveLazyMenu(); break;
            case 0: running = false; break;
            default: std::cout << "Неверный выбор.\n";
        }
    }
    return 0;
}