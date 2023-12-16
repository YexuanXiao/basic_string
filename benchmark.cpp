#include "basic_string.hpp"
#include <vector>
#include <list>

template <typename T>
void test()
{
	using bstring = T;
    
    // begin test constructors
    {
        bstring a(30, '0');
        bstring b(31, '1');
        bstring c(b, 1);
        bstring e(a, 2, 4);
        bstring f(a, 1, 30);
        bstring h(std::move(a), 1, 30);
        bstring i(std::move(b), 1, 30);
        bstring j("12345678", 8);
        bstring k(j.data());
        std::vector<char> ll{ 'a', 'b', 'c', 'd', 'e' };
        bstring l{ ll.begin(), ll.end() };
        std::list<char> mm{ ll.begin(), ll.end() };
        bstring m{ mm.begin(), mm.end() };
        m.resize(31);
        bstring n(m);
        bstring o(std::move(n));
        bstring p{ 'a', 'b', 'c', 'd', 'e' };
        bstring q{ std::string_view{ "1234567890123456789012345678901234567890" } };
        bstring r{ std::string_view{ "1234567890123456789012345678901234567890" }, 1, 20 };
    }
    // end test constructors
    // begin test operator=
    {
        bstring a(40, 'a');
        bstring b{};
        b = a;
        bstring c(50, 'c');
        c = a;
        b = std::move(c);
        c = b.data();
        c = 'c';
        b = { 'a', 'b', 'c' };
        c = std::string_view{ "1234567890123456789012345678901" };
    }
    // end test operator=
    // begin test assign
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.assign(40, 'l');
        l.assign(20, 's');
        l.assign(s);
        s.assign(l, 1, 40);
        l.assign(s, 1, 20);
        l.assign(std::move(s));
        l.assign(100, '1');
        s.assign(l.data(), 20);
        l.assign(s.data());
        std::vector<char> ll(40, 'd');
        l.assign(ll.begin(), ll.end());
        s.assign({ 'a', 'b', 'c' });
        l.assign(std::string_view{ "1234567890123456789012345678901" });
        s.assign(std::string_view{ "1234567890123456789012345678901" }, 30);
    }
    // end test assign
    // begin test iterator
    {
        bstring l = "1234567890123456789012345678901234567890";
        bstring ll{ l.begin(), l.end() };
        std::vector<char> lll{ ll.begin(), ll.end() };
    }
    // end test iterator
    // begin test insert
    {
        bstring l = "abcdefg";
        l.insert(7, 30, '0');
        l.insert(7, "1234567890");
        l.insert(7, "09876543210", 10);
        bstring ll{ "1234567890" };
        l.insert(7, ll);
        l.insert(7, ll, 7, 10);
        l.insert(l.begin() + 7, 'a');
        l.insert(l.begin() + 7, 9, 'b');
        bstring lll{ "hijklmn" };
        l.insert(l.begin() + 7, lll.begin(), lll.end());
        l.insert(l.begin() + 7, { '1', '2', '3' });
        std::string_view llll{ "1234567890" };
        l.insert(7, llll);
        l.insert(7, llll, 4, 8);
    }
    // end test insert
    // begin test erase
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.erase(10, 20);
        l.erase(0, 10);
        s = bstring(20, 's');
        s.erase(s.begin() + 10);
        l = bstring(40, 'l');
        l.erase(l.begin() + 10, l.begin() + 20);
    }
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.erase(10, 10);
        l.erase(0, 10);
        s.erase(s.begin());
        l.erase(l.begin(), l.end());
    }
    // end test erase
    // begin test push_back, pop_back
    {
        bstring s(30, 's');
        bstring l(40, 'l');
        s.push_back('l');
        l.push_back('l');
        s.pop_back();
        l.pop_back();
    }
    // end test push_back, pop_back
    // begin test append
    {
        bstring s(20, 's');
        bstring l(40, 'l');
        s.append(10, 's');
        s.append(10, 'l');
        l.append(10, 'l');
        l.append(s);
        s.append(l, 10, 10);
        s.append("0123456789", 10);
        s.append("9876543210");
        bstring ss{};
        ss.append(s.begin() + 50, s.end());
        ss.append({ 'a', 'b', 'c', 'd' });
        ss.append(std::string_view{ "fghijk" });
        ss.append(std::string_view{ "12345678901234567890" }, 10,10);
    }
    // end test append
    // begin test search
    {
        bstring l("1234567890abcdefghijklmnopqrstuvwxyz");
        auto r = l.starts_with('2');
        r = l.starts_with('1');
        r = l.starts_with(std::string_view{ "123" });
        r = l.starts_with(std::string_view{ "321" });
        r = l.starts_with("1234567890abcdefghijklmnopqrstuvwxyz");
        r = l.ends_with('y');
        r = l.ends_with('z');
        r = l.ends_with(std::string_view{ "xyz" });
        r = l.ends_with(std::string_view{ "zyx" });
        r = l.ends_with("1234567890abcdefghijklmnopqrstuvwxyz");
        r = l.contains("}");
        r = l.contains("abc");
        r = l.contains("1234567890abcdefghijklmnopqrstuvwxyz");
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
    }
}
#include <chrono>
#include <iostream>
int main()
{
    for (size_t i = 1000000; i--;)
    {
        test<bizwen::basic_string<char>>();

    }
    for (size_t i = 1000000; i--;)
    {
        test<std::basic_string<char>>();
    }
    auto pre = std::chrono::high_resolution_clock::now();
    for (size_t i = 1000000; i--;)
    {
        test<bizwen::basic_string<char>>();
    }
    auto now = std::chrono::high_resolution_clock::now();
    auto bizwen = now - pre;
    pre = std::chrono::high_resolution_clock::now();
    for (size_t i = 1000000; i--;)
    {
        test<std::basic_string<char>>();
    }
    now = std::chrono::high_resolution_clock::now();
    auto standard = now - pre;
    std::cout << bizwen << ' ' << standard << std::endl;
    std::cin.get();
}
