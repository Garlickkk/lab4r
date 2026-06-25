#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <string>
#include <cstring>
#include "array_sequence.hpp"
#include "lazy_seq.hpp"
#include "streams.hpp"
#include "file_streams.hpp"
#include "stream_coder.hpp"

int passed = 0;
int failed = 0;

void runTest(void (*test)(), const std::string& name) {
    try {
        test();
        passed++;
        std::cout << "  [OK] " << name << std::endl;
    } catch (const std::exception& e) {
        failed++;
        std::cout << "  [НЕУДАЧА] " << name << ": " << e.what() << std::endl;
    } catch (...) {
        failed++;
        std::cout << "  [НЕУДАЧА] " << name << ": неизвестная ошибка" << std::endl;
    }
}

struct TestFibRule {
    long operator()(Sequence<long>* prev) const {
        int n = prev->GetLength();
        return prev->Get(n - 1) + prev->Get(n - 2);
    }
};

struct TestIncRule {
    int operator()(Sequence<int>* prev) const {
        return prev->Get(prev->GetLength() - 1) + 1;
    }
};

struct TestPow2Rule {
    int operator()(Sequence<int>* prev) const {
        return prev->Get(prev->GetLength() - 1) * 2;
    }
};

struct TestAddTenRule {
    int operator()(Sequence<int>* prev) const {
        return prev->Get(prev->GetLength() - 1) + 10;
    }
};

struct TestAlphabetRule {
    char operator()(Sequence<char>* prev) const {
        char last = prev->Get(prev->GetLength() - 1);
        if (last == 'z') return 'a';
        return static_cast<char>(last + 1);
    }
};

struct TestMulTen {
    int operator()(int x) const { return x * 10; }
};

struct TestIsEven {
    bool operator()(int x) const { return x % 2 == 0; }
};

struct TestSum {
    int operator()(int a, int b) const { return a + b; }
};

struct TestSquare {
    int operator()(int x) const { return x * x; }
};

//LazySequence
void test_lazy_empty() {
    LazySequence<int> ls;
    assert(ls.GetLength() == 0);
    assert(ls.GetMaterializedCount() == 0);
    assert(!ls.IsInfinite());
    bool thrown = false;
    try { ls.GetFirst(); } catch (const EmptyContainerException&) { thrown = true; }
    assert(thrown);
}

void test_lazy_from_array() {
    int data[] = {10, 20, 30, 40, 50};
    LazySequence<int> ls(data, 5);
    assert(ls.GetLength() == 5);
    assert(ls.GetFirst() == 10);
    assert(ls.GetLast() == 50);
    assert(ls.GetMaterializedCount() == 5);
}

void test_lazy_from_sequence() {
    int data[] = {1, 2, 3};
    MutableArraySequence<int> seq(data, 3);
    LazySequence<int> ls(&seq);
    assert(ls.GetLength() == 3);
    assert(ls.Get(0) == 1);
    assert(ls.Get(2) == 3);
}

void test_lazy_fibonacci() {
    TestFibRule rule;
    long init[] = {1, 1};
    MutableArraySequence<long> initial(init, 2);

    LazySequence<long> fib(rule, &initial, -1);
    assert(fib.IsInfinite());
    assert(fib.Get(0) == 1);
    assert(fib.Get(1) == 1);
    assert(fib.Get(4) == 5);
    assert(fib.Get(9) == 55);
}

void test_lazy_finite_with_rule() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> ls(rule, &initial, 10);
    assert(ls.GetLength() == 10);
    assert(!ls.IsInfinite());
    assert(ls.GetLast() == 9);
    assert(ls.Get(5) == 5);
}

void test_lazy_out_of_range() {
    int data[] = {1, 2, 3};
    LazySequence<int> ls(data, 3);
    bool thrown = false;
    try { ls.Get(5); } catch (const IndexOutOfRangeException&) { thrown = true; }
    assert(thrown);

    thrown = false;
    try { ls.Get(-1); } catch (const IndexOutOfRangeException&) { thrown = true; }
    assert(thrown);
}

void test_lazy_infinite_getlast_throws() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> ls(rule, &initial, -1);
    bool thrown = false;
    try { ls.GetLast(); } catch (const InfiniteSequenceException&) { thrown = true; }
    assert(thrown);
}

void test_lazy_subsequence() {
    int data[] = {1, 2, 3, 4, 5};
    LazySequence<int> ls(data, 5);
    LazySequence<int>* sub = ls.GetSubsequence(1, 3);
    assert(sub->GetLength() == 3);
    assert(sub->Get(0) == 2);
    assert(sub->Get(2) == 4);
    delete sub;
}

void test_lazy_append_prepend_insert() {
    int data[] = {2, 3, 4};
    LazySequence<int> ls(data, 3);

    LazySequence<int>* a = ls.Append(5);
    assert(a->GetLength() == 4);
    assert(a->GetLast() == 5);
    delete a;

    LazySequence<int>* p = ls.Prepend(1);
    assert(p->GetFirst() == 1);
    assert(p->Get(1) == 2);
    delete p;

    LazySequence<int>* i = ls.InsertAt(99, 1);
    assert(i->Get(0) == 2);
    assert(i->Get(1) == 99);
    assert(i->Get(2) == 3);
    delete i;
}

void test_lazy_concat() {
    int a[] = {1, 2}, b[] = {3, 4, 5};
    LazySequence<int> la(a, 2), lb(b, 3);
    LazySequence<int>* c = la.Concat(&lb);
    assert(c->GetLength() == 5);
    assert(c->Get(0) == 1);
    assert(c->Get(4) == 5);
    delete c;
}

void test_lazy_concat_with_array_sequence() {
    int a[] = {1, 2};
    int b[] = {3, 4};
    LazySequence<int> la(a, 2);
    MutableArraySequence<int> plain(b, 2);
    LazySequence<int>* c = la.Concat(&plain);
    assert(c->GetLength() == 4);
    assert(c->Get(0) == 1);
    assert(c->Get(3) == 4);
    delete c;
}

void test_lazy_map_where_reduce() {
    int d[] = {1, 2, 3, 4, 5};
    LazySequence<int> ls(d, 5);

    LazySequence<int>* mapped = ls.Map(TestMulTen());
    assert(mapped->Get(0) == 10);
    assert(mapped->Get(4) == 50);
    delete mapped;

    LazySequence<int>* filt = ls.Where(TestIsEven());
    assert(filt->GetLength() == 2);
    assert(filt->Get(0) == 2);
    delete filt;

    int s = ls.Reduce(TestSum(), 0);
    assert(s == 15);
}

void test_lazy_copy() {
    int d[] = {1, 2, 3};
    LazySequence<int> a(d, 3);
    LazySequence<int> b(a);
    assert(b.GetLength() == 3);
    assert(b.Get(1) == 2);
}

void test_lazy_ops_are_lazy() {
    int d[] = {1, 2, 3, 4, 5};
    LazySequence<int> ls(d, 5);

    LazySequence<int>* m = ls.Map(TestMulTen());
    assert(m->GetMaterializedCount() == 0);
    assert(m->Get(2) == 30);
    assert(m->GetMaterializedCount() == 3);
    delete m;

    LazySequence<int>* c = ls.Concat(&ls);
    assert(c->GetMaterializedCount() == 0);
    assert(c->Get(7) == 3);
    delete c;

    LazySequence<int>* sub = ls.GetSubsequence(1, 3);
    assert(sub->GetMaterializedCount() == 0);
    assert(sub->Get(1) == 3);
    delete sub;
}

//Generator
void test_generator_finite() {
    int d[] = {10, 20, 30};
    LazySequence<int> ls(d, 3);
    Generator<int>* g = ls.CreateGenerator();
    assert(g->HasNext());
    assert(g->GetNext() == 10);
    assert(g->GetNext() == 20);
    assert(g->GetNext() == 30);
    assert(!g->HasNext());
    delete g;
    Generator<int>* g2 = ls.CreateGenerator();
    assert(g2->GetNext() == 10);
    delete g2;
}

void test_generator_infinite() {
    TestPow2Rule rule;
    int init[] = {1};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> pow2(rule, &initial, -1);
    Generator<int>* g = pow2.CreateGenerator();
    assert(g->HasNext());
    assert(g->GetNext() == 1);
    assert(g->GetNext() == 2);
    assert(g->GetNext() == 4);
    assert(g->GetNext() == 8);
    delete g;
}

void test_generator_try_get_next() {
    int d[] = {7, 8};
    LazySequence<int> ls(d, 2);
    Generator<int>* g = ls.CreateGenerator();
    Optional<int> a = g->TryGetNext();
    Optional<int> b = g->TryGetNext();
    Optional<int> c = g->TryGetNext();
    assert(a.HasValue() && a.Value() == 7);
    assert(!c.HasValue());
    delete g;
}

void test_generator_insert() {
    int d[] = {1, 2, 3};
    LazySequence<int> ls(d, 3);
    Generator<int>* g0 = ls.CreateGenerator();

    Generator<int>* g = g0->Insert(99);
    assert(g->GetNext() == 99);
    assert(g->GetNext() == 1);
    assert(g->GetNext() == 2);
    assert(g->GetNext() == 3);
    delete g0; delete g;
}

void test_generator_append() {
    int d[] = {1, 2};
    LazySequence<int> ls(d, 2);
    Generator<int>* g0 = ls.CreateGenerator();
    Generator<int>* g = g0->Append(99);
    assert(g->GetNext() == 1);
    assert(g->GetNext() == 2);
    assert(g->GetNext() == 99);
    delete g0; delete g;
}

void test_generator_bulk_insert() {
    int src[] = {1, 2};
    int ins[] = {7, 8, 9};
    LazySequence<int> ls(src, 2);
    MutableArraySequence<int> insertion(ins, 3);
    Generator<int>* g0 = ls.CreateGenerator();

    Generator<int>* g = g0->Insert(&insertion);
    assert(g->GetNext() == 7);
    assert(g->GetNext() == 8);
    assert(g->GetNext() == 9);
    assert(g->GetNext() == 1);
    assert(g->GetNext() == 2);
    delete g0; delete g;
}

void test_generator_ctor_with_insert() {
    int d[] = {1, 2, 3};
    LazySequence<int> ls(d, 3);
    Generator<int>* g = new Generator<int>(&ls, 1, 99);
    assert(g->GetNext() == 1);
    assert(g->GetNext() == 99);
    assert(g->GetNext() == 2);
    assert(g->GetNext() == 3);
    delete g;
}

void test_generator_ctor_with_bulk_insert() {
    int d[] = {1, 2};
    int ins[] = {7, 8};
    LazySequence<int> ls(d, 2);
    MutableArraySequence<int> insertion(ins, 2);
    Generator<int>* g = new Generator<int>(&ls, 1, &insertion);
    assert(g->GetNext() == 1);
    assert(g->GetNext() == 7);
    assert(g->GetNext() == 8);
    assert(g->GetNext() == 2);
    delete g;
}

void test_generator_bounded_memory() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> nats(rule, &initial, -1);

    Generator<int>* g = nats.CreateGenerator();
    for (int i = 0; i < 1000; i++) {
        assert(g->GetNext() == i);
    }
    assert(nats.GetMaterializedCount() == 1);
    delete g;
}

void test_generator_fibonacci_window() {
    TestFibRule rule;
    long init[] = {1, 1};
    MutableArraySequence<long> initial(init, 2);
    LazySequence<long> fib(rule, &initial, -1);

    Generator<long>* g = fib.CreateGenerator();
    long expected[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55};
    for (int i = 0; i < 10; i++) {
        assert(g->GetNext() == expected[i]);
    }
    assert(fib.GetMaterializedCount() == 2);
    delete g;
}

void test_generator_remove_by_value() {
    int d[] = {1, 2, 3, 2};
    LazySequence<int> ls(d, 4);
    Generator<int>* g0 = ls.CreateGenerator();

    Generator<int>* g1 = g0->Remove(2);
    assert(g1->GetNext() == 1);
    assert(g1->GetNext() == 3);
    assert(g1->GetNext() == 2);
    assert(!g1->HasNext());

    int twice[] = {2, 2};
    MutableArraySequence<int> values(twice, 2);
    Generator<int>* g2 = g0->Remove(&values); // оба вхождения
    assert(g2->GetNext() == 1);
    assert(g2->GetNext() == 3);
    assert(!g2->HasNext());

    Generator<int>* g3 = g0->Remove(99);
    assert(g3->GetNext() == 1);
    assert(g3->GetNext() == 2);
    assert(g3->GetNext() == 3);
    assert(g3->GetNext() == 2);
    assert(!g3->HasNext());

    delete g0; delete g1; delete g2; delete g3;
}

void test_generator_remove_by_value_infinite_throws() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> nats(rule, &initial, -1);
    Generator<int>* g = nats.CreateGenerator();
    bool thrown = false;
    try {
        Generator<int>* g2 = g->Remove(5);
        delete g2;
    } catch (const InfiniteSequenceException&) { thrown = true; }
    assert(thrown);
    delete g;
}

void test_empty_getfirst_is_index_out_of_range() {
    LazySequence<int> ls;
    bool thrown = false;
    try { ls.GetFirst(); } catch (const IndexOutOfRangeException&) { thrown = true; }
    assert(thrown);
    thrown = false;
    try { ls.GetLast(); } catch (const IndexOutOfRangeException&) { thrown = true; }
    assert(thrown);
}

//Streams
void test_sequence_read_stream() {
    int d[] = {1, 2, 3, 4};
    MutableArraySequence<int> seq(d, 4);
    SequenceReadStream<int> rs(&seq);
    rs.Open();
    assert(rs.Read() == 1);
    assert(rs.Read() == 2);
    rs.Seek(0);
    assert(rs.Read() == 1);
    rs.Close();
}

void test_sequence_read_stream_not_open() {
    int d[] = {1};
    MutableArraySequence<int> seq(d, 1);
    SequenceReadStream<int> rs(&seq);
    bool thrown = false;
    try { rs.Read(); } catch (const StreamNotOpenException&) { thrown = true; }
    assert(thrown);
}

void test_sequence_write_stream() {
    MutableArraySequence<int>* sink = new MutableArraySequence<int>();
    SequenceWriteStream<int> ws(sink);
    ws.Open();
    ws.Write(10);
    ws.Write(20);
    ws.Write(30);
    assert(sink->GetLength() == 3);
    assert(sink->Get(0) == 10);
    assert(sink->Get(2) == 30);
    ws.Close();
    delete sink;
}

void test_lazy_read_stream_infinite() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> ls(rule, &initial, -1);
    LazyReadStream<int> rs(&ls);
    rs.Open();
    for (int i = 0; i < 100; i++) {
        assert(rs.Read() == i);
    }
    rs.Close();
}

//Codec
void test_caesar_codec() {
    CaesarCodec c(3);
    assert(c.Encode('a') == 'd');
    assert(c.Encode('A') == 'D');
    assert(c.Decode('d') == 'a');
}

void test_xor_codec() {
    const char* key = "K";
    XorCodec c(key, 1);
    char e = c.Encode('A');
    c.Reset();
    assert(c.Decode(e) == 'A');
}

//StreamCoder
Sequence<char>* RunCoder(const char* in, int len, Codec<char>* codec, size_t bufSize, bool encode) {
    char* mutData = new char[len];
    std::memcpy(mutData, in, len);
    MutableArraySequence<char>* src = new MutableArraySequence<char>(mutData, len);
    delete[] mutData;

    SequenceReadStream<char> rs(src);
    MutableArraySequence<char>* sink = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws(sink);
    StreamCoder<char> coder(codec, bufSize);
    rs.Open(); ws.Open();
    if (encode) coder.Encode(&rs, &ws);
    else coder.Decode(&rs, &ws);
    rs.Close(); ws.Close();
    delete src;
    return sink;
}

void test_coder_caesar_encode() {
    CaesarCodec codec(3);
    Sequence<char>* out = RunCoder("hello", 5, &codec, 2, true);
    assert(out->GetLength() == 5);
    assert(out->Get(0) == 'k');
    assert(out->Get(4) == 'r');
    delete out;
}

void test_coder_roundtrip_caesar() {
    CaesarCodec codec(7);
    const char* in = "Hello, World!";
    int len = static_cast<int>(std::strlen(in));
    Sequence<char>* enc = RunCoder(in, len, &codec, 4, true);
    char* tmp = new char[enc->GetLength()];
    for (int i = 0; i < enc->GetLength(); i++) tmp[i] = enc->Get(i);
    Sequence<char>* dec = RunCoder(tmp, enc->GetLength(), &codec, 4, false);
    assert(dec->GetLength() == len);
    for (int i = 0; i < len; i++) assert(dec->Get(i) == in[i]);
    delete[] tmp;
    delete enc;
    delete dec;
}

void test_coder_roundtrip_xor() {
    XorCodec codec("SecretKey", 9);
    const char* in = "Test message";
    int len = static_cast<int>(std::strlen(in));
    Sequence<char>* enc = RunCoder(in, len, &codec, 8, true);
    char* tmp = new char[enc->GetLength()];
    for (int i = 0; i < enc->GetLength(); i++) tmp[i] = enc->Get(i);
    Sequence<char>* dec = RunCoder(tmp, enc->GetLength(), &codec, 8, false);
    assert(dec->GetLength() == len);
    delete[] tmp;
    delete enc;
    delete dec;
}

void test_coder_empty() {
    CaesarCodec codec(1);
    MutableArraySequence<char>* src = new MutableArraySequence<char>();
    SequenceReadStream<char> rs(src);
    MutableArraySequence<char>* sink = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws(sink);
    StreamCoder<char> coder(&codec, 4);
    rs.Open(); ws.Open();
    coder.Encode(&rs, &ws);
    rs.Close(); ws.Close();
    assert(sink->GetLength() == 0);
    delete src;
    delete sink;
}

void test_coder_various_buffer_sizes() {
    CaesarCodec codec(5);
    const char* in = "Test-string";
    int len = static_cast<int>(std::strlen(in));
    size_t sizes[] = {1, 8, 100};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        Sequence<char>* enc = RunCoder(in, len, &codec, sizes[i], true);
        char* tmp = new char[enc->GetLength()];
        for (int k = 0; k < enc->GetLength(); k++) tmp[k] = enc->Get(k);
        Sequence<char>* dec = RunCoder(tmp, enc->GetLength(), &codec, sizes[i], false);
        for (int k = 0; k < len; k++) assert(dec->Get(k) == in[k]);
        delete[] tmp;
        delete enc;
        delete dec;
    }
}

void test_coder_bad_buffer() {
    CaesarCodec codec(1);
    bool thrown = false;
    try {
        StreamCoder<char> coder(&codec, 0);
    } catch (const InvalidArgumentException&) { thrown = true; }
    assert(thrown);
}

void test_lazy_prepend_infinite() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> nats(rule, &initial, -1);
    LazySequence<int>* shifted = nats.Prepend(-1);
    assert(shifted->Get(0) == -1);
    assert(shifted->Get(1) == 0);
    assert(shifted->Get(5) == 4);
    delete shifted;
}

void test_lazy_insert_infinite() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> nats(rule, &initial, -1);
    LazySequence<int>* withInserted = nats.InsertAt(99, 3);
    assert(withInserted->Get(0) == 0);
    assert(withInserted->Get(3) == 99);
    assert(withInserted->Get(7) == 6);
    delete withInserted;
}

void test_lazy_concat_finite_infinite() {
    int f[] = {10, 20, 30};
    LazySequence<int> finite(f, 3);
    TestIncRule rule;
    int init[] = {100};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> infinite(rule, &initial, -1);

    LazySequence<int>* c = finite.Concat(&infinite);
    assert(c->Get(0) == 10);
    assert(c->Get(2) == 30);
    assert(c->Get(3) == 100);
    assert(c->Get(10) == 107);
    delete c;
}

void test_lazy_concat_infinite_left() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> infinite(rule, &initial, -1);
    int f[] = {99};
    LazySequence<int> finite(f, 1);

    LazySequence<int>* c = infinite.Concat(&finite);
    assert(c->Get(0) == 0);
    assert(c->Get(5) == 5);
    delete c;
}

void test_lazy_map_infinite() {
    TestIncRule rule;
    int init[] = {0};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> nats(rule, &initial, -1);
    LazySequence<int>* squared = nats.Map(TestSquare());
    assert(squared->Get(0) == 0);
    assert(squared->Get(3) == 9);
    assert(squared->Get(10) == 100);
    delete squared;
}

void test_lazy_zip_finite() {
    int a[] = {1, 2, 3, 4};
    int b[] = {10, 20, 30};
    LazySequence<int> la(a, 4);
    LazySequence<int> lb(b, 3);
    LazySequence<int>* z = la.Zip(&lb, TestSum());
    assert(z->GetLength() == 3);
    assert(z->Get(0) == 11);
    assert(z->Get(2) == 33);
    delete z;
}

void test_lazy_zip_finite_infinite() {
    int a[] = {1, 2, 3};
    LazySequence<int> la(a, 3);
    TestAddTenRule rule;
    int init[] = {10};
    MutableArraySequence<int> initial(init, 1);
    LazySequence<int> nats(rule, &initial, -1);

    LazySequence<int>* z = la.Zip(&nats, TestSum());
    assert(z->Get(0) == 11);
    assert(z->Get(2) == 33);
    delete z;
}

void test_lazy_zip_infinite() {
    TestIncRule ruleA;
    TestPow2Rule ruleB;
    int initA[] = {1};
    int initB[] = {1};
    MutableArraySequence<int> ia(initA, 1);
    MutableArraySequence<int> ib(initB, 1);
    LazySequence<int> a(ruleA, &ia, -1);
    LazySequence<int> b(ruleB, &ib, -1);

    LazySequence<int>* z = a.Zip(&b, TestSum());
    assert(z->Get(0) == 2);
    assert(z->Get(3) == 12);
    delete z;
}

// Файловые потоки
std::string MakeTempFilePath(const std::string& tag) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "/tmp/lab4_test_%s_%d.dat",
                  tag.c_str(), static_cast<int>(std::rand()));
    return std::string(buf);
}

void test_file_streams_roundtrip() {
    std::string inPath = MakeTempFilePath("in");
    std::string encPath = MakeTempFilePath("enc");
    std::string outPath = MakeTempFilePath("out");

    const char* content = "Hello!";
    int contentLen = static_cast<int>(std::strlen(content));
    {
        std::ofstream fout(inPath.c_str(), std::ios::binary);
        fout.write(content, contentLen);
    }

    {
        FileReadStream rs(inPath);
        FileWriteStream ws(encPath);
        CaesarCodec codec(5);
        StreamCoder<char> coder(&codec, 8);
        coder.Encode(&rs, &ws);
    }

    {
        FileReadStream rs(encPath);
        FileWriteStream ws(outPath);
        CaesarCodec codec(5);
        StreamCoder<char> coder(&codec, 8);
        coder.Decode(&rs, &ws);
    }

    {
        std::ifstream fin(outPath.c_str(), std::ios::binary);
        char buf[256] = {0};
        fin.read(buf, contentLen);
        for (int i = 0; i < contentLen; i++) {
            assert(buf[i] == content[i]);
        }
    }

    std::remove(inPath.c_str());
    std::remove(encPath.c_str());
    std::remove(outPath.c_str());
}

void test_file_stream_seek() {
    std::string path = MakeTempFilePath("seek");
    {
        std::ofstream fout(path.c_str(), std::ios::binary);
        fout << "ABCDEFGH";
    }
    FileReadStream rs(path);
    rs.Open();
    assert(rs.Read() == 'A');
    rs.Seek(4);
    assert(rs.Read() == 'E');
    rs.Close();
    std::remove(path.c_str());
}

void test_stress_one_million() {
    TestAlphabetRule rule;
    char init[] = {'a'};
    MutableArraySequence<char> initial(init, 1);
    LazySequence<char> stream(rule, &initial, -1);

    LazyReadStream<char> rs(&stream);
    MutableArraySequence<char>* sink = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws(sink);

    CaesarCodec codec(13);
    StreamCoder<char> coder(&codec, 4096);
    rs.Open();
    ws.Open();
    coder.EncodeN(&rs, &ws, 1000000);
    rs.Close();
    ws.Close();

    assert(sink->GetLength() == 1000000);
    assert(sink->Get(0) == 'n');
    assert(sink->Get(25) == 'm');
    delete sink;
}

void test_stress_roundtrip_500k() {
    TestAlphabetRule rule;
    char init[] = {'a'};
    MutableArraySequence<char> initial(init, 1);
    LazySequence<char> stream(rule, &initial, -1);

    LazyReadStream<char> rs(&stream);
    MutableArraySequence<char>* encoded = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws(encoded);
    CaesarCodec codec(7);
    StreamCoder<char> coder(&codec, 1024);
    rs.Open(); ws.Open();
    coder.EncodeN(&rs, &ws, 500000);
    rs.Close(); ws.Close();

    SequenceReadStream<char> rs2(encoded);
    MutableArraySequence<char>* decoded = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws2(decoded);
    StreamCoder<char> coder2(&codec, 1024);
    rs2.Open(); ws2.Open();
    coder2.Decode(&rs2, &ws2);
    rs2.Close(); ws2.Close();

    assert(decoded->GetLength() == 500000);
    assert(decoded->Get(0) == 'a');
    assert(decoded->Get(499999) == stream.Get(499999));

    delete encoded;
    delete decoded;
}

void test_coder_encode_n_lazy() {
    TestAlphabetRule rule;
    char init[] = {'a'};
    MutableArraySequence<char> initial(init, 1);
    LazySequence<char> stream(rule, &initial, -1);

    LazyReadStream<char> rs(&stream);
    MutableArraySequence<char>* sink = new MutableArraySequence<char>();
    SequenceWriteStream<char> ws(sink);

    CaesarCodec codec(1);
    StreamCoder<char> coder(&codec, 4);
    rs.Open(); ws.Open();
    coder.EncodeN(&rs, &ws, 10);
    rs.Close(); ws.Close();

    assert(sink->GetLength() == 10);
    assert(sink->Get(0) == 'b');
    assert(sink->Get(1) == 'c');
    delete sink;
}

void runAllTests() {
    passed = failed = 0;
    std::cout << "\nТесты\n";
    runTest(test_lazy_empty, "пустая");
    runTest(test_lazy_from_array, "из массива");
    runTest(test_lazy_from_sequence, "из Sequence");
    runTest(test_lazy_fibonacci, "Фибоначчи (бесконечная)");
    runTest(test_lazy_finite_with_rule, "конечная с правилом");
    runTest(test_lazy_out_of_range, "выход за границы");
    runTest(test_lazy_infinite_getlast_throws, "GetLast у бесконечной");
    runTest(test_lazy_subsequence, "GetSubsequence");
    runTest(test_lazy_append_prepend_insert, "Append/Prepend/InsertAt");
    runTest(test_lazy_concat, "Concat");
    runTest(test_lazy_concat_with_array_sequence, "Concat с обычной ArraySequence");
    runTest(test_lazy_map_where_reduce, "Map/Where/Reduce");
    runTest(test_lazy_copy, "копирование");
    runTest(test_lazy_ops_are_lazy, "ленивость Map/Concat/GetSubsequence");
    runTest(test_generator_finite, "конечный генератор");
    runTest(test_generator_infinite, "бесконечный генератор");
    runTest(test_generator_try_get_next, "TryGetNext");
    runTest(test_generator_insert, "Generator::Insert (позиция)");
    runTest(test_generator_append, "Generator::Append");
    runTest(test_generator_bulk_insert, "bulk Insert");
    runTest(test_generator_ctor_with_insert, "конструктор с предзаданной вставкой");
    runTest(test_generator_ctor_with_bulk_insert, "конструктор");
    runTest(test_generator_bounded_memory, "генератор не растит кэш владельца");
    runTest(test_generator_fibonacci_window, "Фибоначчи через окно генератора");
    runTest(test_generator_remove_by_value, "Remove по значению (одиночный и пакетный)");
    runTest(test_generator_remove_by_value_infinite_throws, "Remove по значению у бесконечной бросает");
    runTest(test_empty_getfirst_is_index_out_of_range, "пустой список — IndexOutOfRange по спецификации");
    runTest(test_lazy_prepend_infinite, "Prepend в бесконечную");
    runTest(test_lazy_insert_infinite, "InsertAt в бесконечную");
    runTest(test_lazy_concat_finite_infinite, "Concat finite+infinite");
    runTest(test_lazy_concat_infinite_left, "Concat infinite+finite");
    runTest(test_lazy_map_infinite, "Map бесконечной");
    runTest(test_lazy_zip_finite, "Zip конечных");
    runTest(test_lazy_zip_finite_infinite, "Zip конечной с бесконечной");
    runTest(test_lazy_zip_infinite, "Zip двух бесконечных");
    runTest(test_sequence_read_stream, "SequenceReadStream");
    runTest(test_sequence_read_stream_not_open, "Read без Open");
    runTest(test_sequence_write_stream, "SequenceWriteStream");
    runTest(test_lazy_read_stream_infinite, "LazyReadStream бесконечный");
    runTest(test_caesar_codec, "CaesarCodec");
    runTest(test_xor_codec, "XorCodec");
    runTest(test_coder_caesar_encode, "Caesar кодирование");
    runTest(test_coder_roundtrip_caesar, "Caesar туда-обратно");
    runTest(test_coder_roundtrip_xor, "XOR туда-обратно");
    runTest(test_coder_empty, "пустой ввод");
    runTest(test_coder_various_buffer_sizes, "разные размеры буфера");
    runTest(test_coder_bad_buffer, "буфер размера 0");
    runTest(test_coder_encode_n_lazy, "EncodeN из ленивого потока");
    runTest(test_file_streams_roundtrip, "FileReadStream/FileWriteStream туда-обратно");
    runTest(test_file_stream_seek, "FileReadStream::Seek");
    runTest(test_stress_one_million, "1M символов (Caesar+ленивый поток)");
    runTest(test_stress_roundtrip_500k, "500k символов туда-обратно");

    std::cout << "Итого: " << passed << " пройдено, " << failed << " провалено\n";
}