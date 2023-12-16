#include "basic_string.hpp"
#include "cstdio"
#include <vector>
#include <list>

template<typename T> void tsize(T const& t, std::size_t size)
{
    static int count = 0;
    ++count;
    auto tsize = t.size();
    auto len = std::strlen(t.data());
    assert(tsize == size);
    std::printf("test %d, size: %zu\n%s\n", count, size, t.data());
}
// clang-format off
// macro to simplify test
#define TRY try {
// must throw, if not, terminate it
#define CATCH std::terminate(); } catch(std::out_of_range &){}

// clang-format on
int main()
{
	using bstring = bizwen::basic_string<char>;
    
    // begin test constructors
    {
        bstring a(30, '0');
        tsize(a, 30);
        assert(a.capacity() == 30);
        bstring b(31, '1');
        assert(b.capacity() > 30);
        bstring c(b, 1);
        tsize(c, 30);
        TRY bstring d(a, 31);
        CATCH
        bstring e(a, 2, 4);
        tsize(e, 4);
        bstring f(a, 1, 30);
        tsize(f, 29);
        TRY bstring g(a, 31, 1);
        CATCH
        bstring h(std::move(a), 1, 30);
        tsize(h, 29);
        bstring i(std::move(b), 1, 30);
        tsize(i, 30);
        assert(i.capacity() > 30);
        bstring j("12345678", 8);
        tsize(j, 8);
        bstring k(j.data());
        tsize(k, 8);
        std::vector<char> ll{ 'a', 'b', 'c', 'd', 'e' };
        bstring l{ ll.begin(), ll.end() };
        tsize(l, 5);
        std::list<char> mm{ ll.begin(), ll.end() };
        bstring m{ mm.begin(), mm.end() };
        tsize(m, 5);
        m.resize(31);
        tsize(m, 31);
        bstring n(m);
        tsize(n, 31);
        bstring o(std::move(n));
        tsize(o, 31);
        bstring p{ 'a', 'b', 'c', 'd', 'e' };
        tsize(p, 5);
        bstring q{ std::string_view{ "1234567890123456789012345678901234567890" } };
        tsize(q, 40);
        bstring r{ std::string_view{ "1234567890123456789012345678901234567890" }, 1, 20 };
        TRY bstring s{ std::string_view{ "1234567890123456789012345678901234567890" }, 41, 1 };
        CATCH
    }
    // end test constructors
    // begin test operator=
    {
        bstring a(40, 'a');
        bstring b{};
        b = a;
        tsize(b, 40);
        bstring c(50, 'c');
        tsize(c, 50);
        c = a;
        tsize(c, 40);
        b = std::move(c);
        tsize(b, 40);
        tsize(c, 40);
        c = b.data();
        tsize(c, 40);
        c = 'c';
        tsize(c, 1);
        b = { 'a', 'b', 'c' };
        tsize(b, 3);
        c = std::string_view{ "1234567890123456789012345678901" };
        tsize(c, 31);
    }
    // end test operator=
    // begin test assign
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.assign(40, 'l');
        tsize(s, 40);
        l.assign(20, 's');
        tsize(l, 20);
        l.assign(s);
        tsize(l, 40);
        s.assign(l, 1, 40);
        tsize(s, 39);
        l.assign(s, 1, 20);
        tsize(l, 20);
        TRY s.assign(l, 21, 1);
        CATCH
        l.assign(std::move(s));
        tsize(s, 20);
        tsize(l, 39);
        l.assign(100, '1');
        s.assign(l.data(), 20);
        tsize(s, 20);
        l.assign(s.data());
        tsize(l, 20);
        std::vector<char> ll(40, 'd');
        l.assign(ll.begin(), ll.end());
        tsize(l, 40);
        s.assign({ 'a', 'b', 'c' });
        tsize(s, 3);
        l.assign(std::string_view{ "1234567890123456789012345678901" });
        tsize(l, 31);
        s.assign(std::string_view{ "1234567890123456789012345678901" }, 30);
        tsize(s, 1);
        TRY s.assign(std::string_view{ "1234567890123456789012345678901" }, 32);
        CATCH
    }
    // end test assign
    // begin test iterator
    {
        bstring l = "1234567890123456789012345678901234567890";
        bstring ll{ l.begin(), l.end() };
        tsize(ll, 40);
        std::vector<char> lll{ ll.begin(), ll.end() };
        assert(lll.size() == 40);
    }
    // end test iterator
    // begin test insert
    {
        bstring l = "abcdefg";
        l.insert(7, 30, '0');
        tsize(l, 37);
        TRY l.insert(38, 5, '0');
        CATCH
        l.insert(7, "1234567890");
        tsize(l, 47);
        l.insert(7, "09876543210", 10);
        tsize(l, 57);
        bstring ll{ "1234567890" };
        l.insert(7, ll);
        tsize(l, 67);
        l.insert(7, ll, 7, 10);
        tsize(l, 70);
        l.insert(l.begin() + 7, 'a');
        tsize(l, 71);
        l.insert(l.begin() + 7, 9, 'b');
        tsize(l, 80);
        bstring lll{ "hijklmn" };
        l.insert(l.begin() + 7, lll.begin(), lll.end());
        tsize(l, 87);
        l.insert(l.begin() + 7, { '1', '2', '3' });
        tsize(l, 90);
        std::string_view llll{ "1234567890" };
        l.insert(7, llll);
        tsize(l, 100);
        l.insert(7, llll, 4, 8);
        tsize(l, 106);
    }
    // end test insert
    // begin test erase
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        TRY s.erase(21, 10);
        CATCH
        s.erase(10, 20);
        tsize(s, 10);
        l.erase(0, 10);
        tsize(l, 30);
        s = bstring(20, 's');
        s.erase(s.begin() + 10);
        tsize(s, 19);
        l = bstring(40, 'l');
        l.erase(l.begin() + 10, l.begin() + 20);
        tsize(l, 30);
    }
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.erase(10, 10);
        tsize(s, 10);
        l.erase(0, 10);
        tsize(l, 30);
        s.erase(s.begin());
        tsize(s, 9);
        l.erase(l.begin(), l.end());
        tsize(l, 0);
    }
    // end test erase
    // begin test push_back, pop_back
    {
        bstring s(30, 's');
        bstring l(40, 'l');
        s.push_back('l');
        tsize(s, 31);
        l.push_back('l');
        tsize(l, 41);
        s.pop_back();
        tsize(s, 30);
        l.pop_back();
        tsize(l, 40);
    }
    // end test push_back, pop_back
    // begin test append
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.append(10, 's');
        tsize(s, 30);
        s.append(10, 'l');
        tsize(s, 40);
        l.append(10, 'l');
        tsize(l, 50);
        l.append(s);
        tsize(l, 90);
        s.append(l, 10, 10);
        tsize(s, 50);
        s.append("0123456789", 10);
        tsize(s, 60);
        s.append("9876543210");
        tsize(s, 70);
        bstring ss{};
        ss.append(s.begin() + 50, s.end());
        tsize(ss, 20);
        ss.append({ 'a', 'b', 'c', 'd' });
        tsize(ss, 24);
        ss.append(std::string_view{ "fghijk" });
        tsize(ss, 30);
        ss.append(std::string_view{ "12345678901234567890" }, 10,10);
        tsize(ss, 40);
        TRY ss.append(std::string_view{ "12345678901234567890" }, 21, 10);
        CATCH
    }
    // end test append
    // begin test search
    {
        bstring l("1234567890abcdefghijklmnopqrstuvwxyz");
        auto r = l.starts_with('2');
        assert(r == false);
        r = l.starts_with('1');
        assert(r == true);
        r = l.starts_with(std::string_view{ "123" });
        assert(r == true);
        r = l.starts_with(std::string_view{ "321" });
        assert(r == false);
        r = l.starts_with("1234567890abcdefghijklmnopqrstuvwxyz");
        assert(r == true);
        r = l.ends_with('y');
        assert(r == false);
        r = l.ends_with('z');
        assert(r == true);
        r = l.ends_with(std::string_view{ "xyz" });
        assert(r == true);
        r = l.ends_with(std::string_view{ "zyx" });
        assert(r == false);
        r = l.ends_with("1234567890abcdefghijklmnopqrstuvwxyz");
        assert(r == true);
        r = l.contains("}");
        assert(r == false);
        r = l.contains("abc");
        assert(r == true);
        r = l.contains("1234567890abcdefghijklmnopqrstuvwxyz");
        assert(r == true);
    }
    // end test search
    // begin test compare
    {
        bstring greater("1234567890abcdefghijklmnopqrstuvwxyz");
        bstring less1("1234567890abcdefghijklmnopqrstuvwxy");
        bstring less2("1234567890abcdefghijklmnopqrstuvwxyy");
        auto e = std::strong_ordering::equal;
        auto g = std::strong_ordering::greater;
        auto l = std::strong_ordering::less;
        assert((greater == greater) == true);
        assert((greater == less1) == false);
        assert((greater == less2) == false);
        assert((greater <=> greater) == e);
        assert((greater <=> less1) == g);
        assert((greater <=> less2) == g);
        assert((less2 <=> greater) == l);
        assert((greater == "1234567890abcdefghijklmnopqrstuvwxyz") == true);
        assert((greater == "1234567890abcdefghijklmnopqrstuvwxy") == false);
        assert((greater == "1234567890abcdefghijklmnopqrstuvwxy") == false);
        assert((greater <=> "1234567890abcdefghijklmnopqrstuvwxyz") == e);
        assert((greater <=> "1234567890abcdefghijklmnopqrstuvwxy") == g);
        assert((greater <=> "1234567890abcdefghijklmnopqrstuvwxyy") == g);
        assert((less2 <=> "1234567890abcdefghijklmnopqrstuvwxyz") == l);
    }
}
