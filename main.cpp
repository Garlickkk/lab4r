#include <iostream>
#include <string>
#include <sstream>
#include <limits>
#include "array_sequence.hpp"
#include "lazy_seq.hpp"
#include "streams.hpp"
#include "stream_coder.hpp"

void runAllTests();

void clearInputBuffer() { // ф-ции для очистки буфера
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // уд всех сим в строке
}

int readInt(const std::string& prompt) { // безопасное чтение целых
    int value;
    std::cout << prompt;
    while (!(std::cin >> value)) {
        clearInputBuffer();
        std::cout << "Ошибка! Введите целое число: ";
    }
    clearInputBuffer();
    return value;
}

std::string readString(const std::string& prompt) { // безопасное чтение строк
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

static Sequence<char>* StringToSequence(const std::string& s) {
    auto seq = new MutableArraySequence<char>();
    for (char c : s) seq->Append(c); // копируем каждый символ
    return seq;
}

static std::string PrintableSequence(const Sequence<char>* seq, int maxShow = 80) {
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
    int codecKind = 1; // 1 = Caesar, 2 = XOR
    int caesarShift = 3;
    std::string xorKey = "K";
    size_t bufferSize = 16;
};

static Codec<char>* MakeCodec(const CoderConfig& cfg) {
    switch (cfg.codecKind) {
        case 1: return new CaesarCodec(cfg.caesarShift);
        case 2: return new XorCodec(cfg.xorKey.c_str(), static_cast<int>(cfg.xorKey.size()));
        default: throw InvalidArgumentException("Неизвестный кодек");
    }
}

static std::string CodecName(const CoderConfig& cfg) {
    switch (cfg.codecKind) {
        case 1: return "Caesar(shift=" + std::to_string(cfg.caesarShift) + ")";
        case 2: return "XOR(key=\"" + cfg.xorKey + "\")";
        default: return "?";
    }
}

static void Configure(CoderConfig& cfg) {
    std::cout << "\nНастройка кодека\n";
    cfg.codecKind = readInt("1. Caesar  2. XOR\nВыбор: ");

    if (cfg.codecKind == 1) {
        cfg.caesarShift = readInt("Сдвиг Caesar: ");
    } else if (cfg.codecKind == 2) {
        cfg.xorKey = readString("Ключ XOR (строка): ");
        if (cfg.xorKey.empty()) cfg.xorKey = "K";
    }

    int b = readInt("Размер буфера (>0): ");
    if (b > 0) cfg.bufferSize = static_cast<size_t>(b);

    std::cout << "Сохранено: " << CodecName(cfg) << ", буфер " << cfg.bufferSize << "\n";
}

static void RunOnText(const CoderConfig& cfg, bool encode) {
    std::string line = readString("\nВведите текст (одной строкой): ");
    Sequence<char>* input = StringToSequence(line);
    Sequence<char>* output = new MutableArraySequence<char>();

    SequenceReadStream<char> rs(input); // Поток чтения
    SequenceWriteStream<char> ws(output); // Поток записи
    Codec<char>* codec = MakeCodec(cfg);
    StreamCoder<char> coder(codec, cfg.bufferSize);

    try {
        rs.Open(); ws.Open();
        encode ? coder.Encode(&rs, &ws) : coder.Decode(&rs, &ws);
        rs.Close(); ws.Close();

        std::cout << "Исходный: " << PrintableSequence(input) << "\n";
        std::cout << "Результат: " << PrintableSequence(output) << "\n";
        std::cout << "Обработано: " << coder.GetBytesIn() << " -> " << coder.GetBytesOut() << " символов, буфер " << cfg.bufferSize << "\n";
    } catch (const std::exception& e) {
        std::cout << "Ошибка: " << e.what() << "\n";
    }

    delete codec; delete input; delete output;
}

static void RunOnFile(const CoderConfig& cfg, bool encode) {
    std::string inPath = readString("\nПуть к входному файлу: ");
    std::string outPath = readString("Путь к выходному файлу: ");

    try {
        FileReadStream rs(inPath);
        FileWriteStream ws(outPath);
        Codec<char>* codec = MakeCodec(cfg);
        StreamCoder<char> coder(codec, cfg.bufferSize);

        rs.Open(); ws.Open();
        encode ? coder.Encode(&rs, &ws) : coder.Decode(&rs, &ws);
        rs.Close(); ws.Close();

        std::cout << "Готово. " << coder.GetBytesIn() << " байт прочитано, "
             << coder.GetBytesOut() << " байт записано. Буфер " << cfg.bufferSize << ".\n";
        delete codec;
    } catch (const std::exception& e) {
        std::cout << "Ошибка: " << e.what() << "\n";
    }
}

struct FibonacciRule {
    long operator()(Sequence<long>* prev) const {
        int n = prev->GetLength();
        return prev->Get(n - 1) + prev->Get(n - 2);
    }
};

static void DemoLazySequence() {
    std::cout << "\n--- Демонстрация LazySequence (Фибоначчи) ---\n";

    FibonacciRule rule;
    long init[] = {1, 1};
    MutableArraySequence<long> initial(init, 2);
    LazySequence<long> fib(rule, &initial, -1);

    int countGen = readInt("Сколько элементов Фибоначчи вывести через генератор? ");
    if (countGen < 0) countGen = 0;

    Generator<long>* gen = fib.CreateGenerator();
    std::cout << "Через генератор (" << countGen << " элементов): ";
    for (int i = 0; i < countGen; i++) {
        if (gen->HasNext()) std::cout << gen->GetNext() << " ";
    }
    std::cout << "\n(В памяти закэшировано: " << fib.GetMaterializedCount() << " элементов)\n\n";
    delete gen;

    int countStream = readInt("Сколько элементов вывести через LazyReadStream? ");
    if (countStream < 0) countStream = 0;

    LazyReadStream<long> ls(&fib);
    ls.Open();
    std::cout << "Через LazyReadStream (" << countStream << " элементов): ";
    for (int i = 0; i < countStream && !ls.IsEndOfStream(); i++) {
        std::cout << ls.Read() << " ";
    }
    std::cout << "\n";
    ls.Close();
}

struct AlphabetRule {
    char operator()(Sequence<char>* prev) const {
        char last = prev->Get(prev->GetLength() - 1);
        return (last == 'z') ? 'a' : static_cast<char>(last + 1);
    }
};

static void DemoLazyEncoding(const CoderConfig& cfg) {
    std::cout << "\n--- Кодирование префикса ленивого потока ---\n";

    AlphabetRule rule;
    char init[] = {'a'};
    MutableArraySequence<char> initial(init, 1);
    LazySequence<char> stream(rule, &initial, -1);

    int inputLen = readInt("Сколько символов бесконечного потока закодировать? ");
    size_t prefixLen = (inputLen > 0) ? static_cast<size_t>(inputLen) : 0;

    LazyReadStream<char> rs(&stream);
    auto outSeq = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws(outSeq);

    Codec<char>* codec = MakeCodec(cfg);
    StreamCoder<char> coder(codec, cfg.bufferSize);

    rs.Open(); ws.Open();
    coder.EncodeN(&rs, &ws, prefixLen);
    rs.Close(); ws.Close();

    std::cout << "Кодек: " << CodecName(cfg) << ", буфер " << cfg.bufferSize << ", взято " << prefixLen << " символов\n";
    std::cout << "Исходный префикс: ";
    for (size_t i = 0; i < prefixLen; i++) std::cout << stream.Get(i);
    std::cout << "\nПосле кодирования: " << PrintableSequence(outSeq) << "\n";

    delete codec; delete outSeq;
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
        std::cout << "6. Демонстрация LazySequence (Фибоначчи)\n";
        std::cout << "7. Кодирование префикса бесконечного потока\n";
        std::cout << "8. Запустить все тесты\n";
        std::cout << "0. Выход\n";

        int choice = readInt("Выбор: ");

        switch (choice) {
            case 1: Configure(cfg); break;
            case 2: RunOnText(cfg, true); break;
            case 3: RunOnText(cfg, false); break;
            case 4: RunOnFile(cfg, true); break;
            case 5: RunOnFile(cfg, false); break;
            case 6: DemoLazySequence(); break;
            case 7: DemoLazyEncoding(cfg); break;
            case 8: runAllTests(); break;
            case 0: running = false; break;
            default: std::cout << "Неверный выбор. Напишите ещё раз\n";
        }
    }
    return 0;
}