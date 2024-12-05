#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <memory_resource>
#include <version>

#if !defined(__cpp_if_consteval) || (__cpp_if_consteval < 202106L)
#error "requires __cpp_if_consteval"
#endif

#if !defined(__cpp_size_t_suffix) || (__cpp_size_t_suffix < 202011L)
#error "requires __cpp_size_t_suffix"
#endif

namespace bizwen
{
template <typename CharT, typename Traits = ::std::char_traits<CharT>, typename Allocator = ::std::allocator<CharT>>
class alignas(CharT *) basic_string
{
    static_assert(::std::is_same_v<char, CharT> || ::std::is_same_v<wchar_t, CharT> ||
                  ::std::is_same_v<char8_t, CharT> || ::std::is_same_v<char16_t, CharT> ||
                  ::std::is_same_v<char32_t, CharT>);
    static_assert(::std::is_same_v<Traits, ::std::char_traits<CharT>>);

  public:
    using traits_type = Traits;
    using value_type = CharT;
    using allocator_type = Allocator;
    using size_type = ::std::size_t;
    using difference_type = ::std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = value_type const &;
    using pointer = typename ::std::allocator_traits<Allocator>::pointer;
    using const_pointer = typename ::std::allocator_traits<Allocator>::const_pointer;

    static inline constexpr size_type npos = size_type(-1);

  private:
    /**
     * @brief type of long string
     */
    struct ls_type_
    {
        pointer begin_{};
        CharT *end_{};
        CharT *last_{};

        constexpr auto begin() const noexcept
        {
            return ::std::to_address(begin_);
        }

        constexpr auto end() const noexcept
        {
            return ::std::to_address(end_);
        }
    };

    // -2 is due to the null terminator and size_flag
    /**
     * @brief short_str_max_ is the max length of short string
     */
    static inline constexpr ::std::size_t short_str_max_{sizeof(CharT *) * 4uz / sizeof(CharT) - 2uz};

    /**
     * @brief union storage long string and short string
     */
#pragma pack(push, 1)
    union storage_type_ {
        ::std::array<CharT, short_str_max_ + 1uz /* null terminator */> ss_{};
        ls_type_ ls_;
    };
#pragma pack(pop)
    // https://github.com/microsoft/STL/issues/1364
#if __has_cpp_attribute(msvc::no_unique_address)
    [[msvc::no_unique_address]] Allocator allocator_{};
#else
    [[no_unique_address]] Allocator allocator_{};
#endif

    /**
     * @brief storage of string
     */
    storage_type_ stor_{};

    /**
     * @brief flag > 0: short string, length of string is size_flag
     * @brief flag = 0: empty string
     * @brief flag = MAX: long string, length of string is end - begin
     * @brief If the string migrates from short to long, then any operation other than
     * @brief move and shrink_to_fit will not make it revert back to short
     */
    alignas(CharT) unsigned char size_flag_{};

    // sizeof(basic_string<T>) is always equal to sizeof(void*) * 4

    using atraits_t_ = ::std::allocator_traits<Allocator>;

    /**
     * @return ::std::out_of_range
     */
    static constexpr auto out_of_range() noexcept
    {
        return ::std::out_of_range("pos/index is out of range, please check it.");
    }

    /**
     * @brief check if the two ranges overlap, and if overlap, new memory needs to allocate
     **/
    static constexpr bool overlap(CharT const *first_inner, CharT const *last_inner, CharT const *first_outer,
                                  CharT const *last_outer) noexcept
    {
        if !consteval
        {
            // [utilities.comparisons.general] strict total order over pointers.
            ::std::ranges::greater greater;
            ::std::ranges::less less;

            if (less(first_inner, first_outer) || greater(first_inner, last_outer))
            {
                assert(less(last_inner, first_outer) || greater(last_inner, last_outer));
                // input range overlaps with *this but is larger
                return false;
            }
        }
        // always assume overlap during constant evaluation
        return true;
    }

    constexpr auto &long_str_() noexcept
    {
        return stor_.ls_;
    }

    constexpr auto &short_str_() noexcept
    {
        return stor_.ss_;
    }

    constexpr auto &long_str_() const noexcept
    {
        return stor_.ls_;
    }

    constexpr auto &short_str_() const noexcept
    {
        return stor_.ss_;
    }

    /**
     * @brief convert to long string and set the flag
     */
    constexpr void long_str_(ls_type_ const &ls) noexcept
    {
        stor_ = storage_type_{.ls_ = ls};
        size_flag_ = static_cast<unsigned char>(-1);
        *ls.end() = CharT{};
    }

    /**
     * @brief convert to short string and resize
     */
    constexpr void short_str_(size_type size) noexcept
    {
        stor_ = storage_type_{};
        size_flag_ = static_cast<unsigned char>(size);
        short_str_()[size] = CharT{};
    }

    constexpr bool is_long_() const noexcept
    {
        return size_flag_ == static_cast<unsigned char>(-1);
    }

    constexpr bool is_short_() const noexcept
    {
        return !is_long_();
    }

    constexpr void resize_shrink_(bool is_long, size_type n) noexcept
    {
        if (is_long)
        {
            assert(size_flag_ == static_cast<unsigned char>(-1));
            auto &ls = long_str_();
            ls.end_ = ls.begin() + n;
            *ls.end_ = CharT{};
        }
        else
        {
            assert(size_flag_ != static_cast<unsigned char>(-1));
            size_flag_ = static_cast<unsigned char>(n);
            short_str_()[size_flag_] = CharT{};
        }
    }

    constexpr size_type size_() const noexcept
    {
        return is_short_() ? size_flag_ : long_str_().end() - long_str_().begin();
    }

    // ********************************* begin volume ******************************
  public:
    constexpr size_type size() const noexcept
    {
        return size_();
    }

    constexpr size_type length() const noexcept
    {
        return size_();
    }

    constexpr bool empty() const noexcept
    {
        return !size_();
    }

    /**
     * @return size_type{ -1 } / sizeof(CharT) / 2uz - 1uz
     */
    constexpr size_type max_size() const noexcept
    {
        return size_type(-1) / sizeof(CharT) / 2uz - 1uz /* null terminator */;
    }

    constexpr size_type capacity() const noexcept
    {
        return is_short_() ? short_str_max_ : long_str_().last_ - long_str_().begin();
    }

    /**
     * @brief shrink only occur on short string is enough to storage
     */
    constexpr void shrink_to_fit() noexcept
    {
        if (is_long_() && size_() <= short_str_max_)
        {
            // copy to local
            auto const ls = long_str_();
            short_str_(size_());
            ::std::ranges::copy(ls.begin(), ls.end(), short_str_().data());
            dealloc_(ls);
        }
    }

    friend struct construct_guard_;

    struct construct_guard_
    {
        basic_string *self;

        constexpr void release() noexcept
        {
            self = nullptr;
        }

        constexpr ~construct_guard_()
        {
            if (self)
                self->dealloc_(self->is_long_());
        }
    };

    // ********************************* begin element access ******************************

  private:
    /**
     * @return a pointer to the first element
     */
    constexpr CharT const *begin_() const noexcept
    {
        return is_short_() ? short_str_().data() : long_str_().begin();
    }

    /**
     * @return a pointer to the first element
     */
    constexpr CharT *begin_() noexcept
    {
        return is_short_() ? short_str_().data() : long_str_().begin();
    }

    /**
     * @return a pointer to the next position of the last element
     */
    constexpr CharT const *end_() const noexcept
    {
        return is_short_() ? short_str_().data() + size_flag_ : long_str_().end();
    }

    /**
     * @return a pointer to the next position of the last element
     */
    constexpr CharT *end_() noexcept
    {
        return is_short_() ? short_str_().data() + size_flag_ : long_str_().end();
    }

  public:
    constexpr CharT const *data() const noexcept
    {
        return begin_();
    }

    constexpr CharT *data() noexcept
    {
        return begin_();
    }

    constexpr CharT const *c_str() const noexcept
    {
        return begin_();
    }

    constexpr const_reference at(size_type pos) const
    {
        return pos < size_() ? *(begin_() + pos) : throw out_of_range();
    }

    constexpr reference at(size_type pos)
    {
        return pos < size_() ? *(begin_() + pos) : throw out_of_range();
    }

    constexpr const_reference operator[](size_type pos) const noexcept
    {
        assert(pos <= size_());

        return *(begin_() + pos);
    }

    constexpr reference operator[](size_type pos) noexcept
    {
        assert(pos <= size_());

        return *(begin_() + pos);
    }

    constexpr const_reference front() const noexcept
    {
        assert(!empty());

        return *begin_();
    }

    constexpr reference front()
    {
        assert(!empty());

        return *begin_();
    }

    constexpr const_reference back() const noexcept
    {
        assert(!empty());

        return *(end_() - 1uz);
    }

    constexpr reference back()
    {
        assert(!empty());

        return *(end_() - 1uz);
    }

    constexpr operator ::std::basic_string_view<value_type, traits_type>() const noexcept
    {
        return {begin_(), end_()};
    }

    // ********************************* begin iterator type ******************************

  private:
    struct iterator_type_
    {
        using difference_type = ::std::ptrdiff_t;
        using value_type = CharT;
        using pointer = CharT *;
        using reference = CharT &;
        using iterator_category = ::std::random_access_iterator_tag;
        using iterator_concept = ::std::contiguous_iterator_tag;

      private:
        pointer current_{};

        // the C++ standard states that all iterators of string become invalid after move,
        // so the address of the string can be stored directly.
#ifndef NDEBUG
        basic_string *target_{};
#endif

        // Call after iteration.
        constexpr void check() const noexcept
        {
#ifndef NDEBUG
            // if current_ is not nullptr, then target must not be nullptr.
            assert(current_ && current_ <= target_->end_() && current_ >= target_->begin_());
            // iterator is invalidated
#endif
        }

        friend class basic_string;

      public:
        iterator_type_() noexcept = default;
        iterator_type_(iterator_type_ const &) noexcept = default;
        iterator_type_(iterator_type_ &&) noexcept = default;
        iterator_type_ &operator=(iterator_type_ const &) & noexcept = default;
        iterator_type_ &operator=(iterator_type_ &&) & noexcept = default;

      private:
#ifndef NDEBUG
        constexpr iterator_type_(pointer current, basic_string *target) noexcept : current_(current), target_(target)
        {
        }
#else
        constexpr iterator_type_(pointer current) noexcept : current_(current)
        {
        }
#endif

      public:
        constexpr iterator_type_ operator+(difference_type n) const & noexcept
        {
            auto temp = *this;
            temp.current_ += n;
            temp.check();

            return temp;
        }

        constexpr iterator_type_ operator-(difference_type n) const & noexcept
        {
            auto temp = *this;
            temp.current_ -= n;
            temp.check();

            return temp;
        }

        constexpr friend iterator_type_ operator+(difference_type n, iterator_type_ const &rhs) noexcept
        {
            auto temp = rhs;
            temp.current_ += n;
            temp.check();

            return temp;
        }

        constexpr friend iterator_type_ operator-(difference_type n, iterator_type_ const &rhs) noexcept
        {
            auto temp = rhs;
            temp.current_ -= n;
            temp.check();

            return temp;
        }

        constexpr friend difference_type operator-(iterator_type_ const &lhs, iterator_type_ const &rhs) noexcept
        {
            assert(lhs.target_ == rhs.target_);
            // iterator belongs to different strings
            return lhs.current_ - rhs.current_;
        }

        constexpr iterator_type_ &operator+=(difference_type n) & noexcept
        {
            current_ += n;
            check();

            return *this;
        }

        constexpr iterator_type_ &operator-=(difference_type n) & noexcept
        {
            current_ -= n;
            check();

            return *this;
        }

        constexpr iterator_type_ &operator++() & noexcept
        {
            ++current_;
            check();

            return *this;
        }

        constexpr iterator_type_ &operator--() & noexcept
        {
            --current_;
            check();

            return *this;
        }

        constexpr iterator_type_ operator++(int) & noexcept
        {
            iterator_type_ temp{*this};
            ++current_;
            check();

            return temp;
        }

        constexpr iterator_type_ operator--(int) & noexcept
        {
            iterator_type_ temp{*this};
            --current_;
            check();

            return temp;
        }

        constexpr value_type &operator[](difference_type n) const noexcept
        {
#ifndef NDEBUG
            iterator_type_ end = (*this) + n;
            end.check();
#endif
            return *(current_ + n);
        }

        constexpr value_type &operator*() const noexcept
        {
            return *current_;
        }

        constexpr value_type *operator->() const noexcept
        {
            return current_;
        }

        friend constexpr ::std::strong_ordering operator<=>(iterator_type_ const &,
                                                            iterator_type_ const &) noexcept = default;
    };

    // ********************************* begin iterator function ******************************

  public:
    using iterator = iterator_type_;
    using const_iterator = ::std::basic_const_iterator<iterator_type_>;
    using reverse_iterator = ::std::reverse_iterator<iterator>;
    using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

    constexpr iterator begin() noexcept
    {
#ifndef NDEBUG
        return {begin_(), this};
#else
        return {begin_()};
#endif
    }

    constexpr iterator end() noexcept
    {
#ifndef NDEBUG
        return {end_(), this};
#else
        return {end_()};
#endif
    }

    constexpr const_iterator begin() const noexcept
    {
#ifndef NDEBUG
        return iterator_type_{const_cast<value_type *>(begin_()), const_cast<basic_string *>(this)};
#else
        return iterator_type_{begin_()};
#endif
    }

    constexpr const_iterator end() const noexcept
    {
#ifndef NDEBUG
        return iterator_type_{const_cast<value_type *>(end_()), const_cast<basic_string *>(this)};
#else
        return iterator_type_{end_()};
#endif
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

    constexpr auto rbegin() noexcept
    {
        return reverse_iterator{end()};
    }
    constexpr auto rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }
    constexpr auto rend() noexcept
    {
        return reverse_iterator{begin()};
    }
    constexpr auto rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }
    constexpr auto crbegin() const noexcept
    {
        return const_reverse_iterator{cend()};
    }
    constexpr auto crend() const noexcept
    {
        return const_reverse_iterator{cbegin()};
    }

    // ********************************* begin memory management ******************************

  private:
    /**
     * @brief allocates memory and automatically adds 1 to store null terminator
     * @param n, expected number of characters
     */
    constexpr ls_type_ allocate_(size_type cap, size_type size)
    {
#if defined(__cpp_lib_allocate_at_least) && (__cpp_lib_allocate_at_least >= 202302L)
        auto const [ptr, count] = atraits_t_::allocate_at_least(
            allocator_, static_cast<atraits_t_::size_type>(cap + 1uz /* null terminator */));

        if consteval
        {
            for (auto i = 0uz; i != cap + 1uz; ++i)
                ::std::construct_at(&ptr[i]);
        }

        return {ptr, ::std::to_address(ptr) + size, ::std::to_address(ptr) + count - 1uz /* null terminator */};
#else
        auto const ptr =
            atraits_t_::allocate(allocator_, static_cast<atraits_t_::size_type>(cap + 1uz /* null terminator */));

        if consteval
        {
            for (auto i = 0uz; i != cap + 1uz; ++i)
                ::std::construct_at(&ptr[i]);
        }

        return {ptr, ::std::to_address(ptr) + size, ::std::to_address(ptr) + cap};
#endif
    }

    /**
     * @brief dealloc the memory of long string
     * @param ls, allocated long string
     */
    constexpr void dealloc_(ls_type_ const &ls) noexcept
    {
        atraits_t_::deallocate(
            allocator_, ls.begin_,
            static_cast<atraits_t_::size_type>(ls.last_ - ::std::to_address(ls.begin_) + 1uz /* null terminator */));
    }

    constexpr void dealloc_(bool is_long) noexcept
    {
        // avoid access the inactive member of union
        if (is_long)
            dealloc_(long_str_());
    }

    /**
     * @brief caculate the length of c style string
     * @param begin begin of characters
     * @return length
     */
    constexpr static size_type c_string_length_(CharT const *begin) noexcept
    {
        if !consteval
        {
            if constexpr (::std::is_same_v<char, CharT> || ::std::is_same_v<char8_t, CharT>)
                return ::std::strlen(reinterpret_cast<char const *>(begin));
            else if constexpr (::std::is_same_v<wchar_t, CharT>)
                return ::std::wcslen(begin);
        }

        auto end = begin;

        for (; *end != CharT{}; ++end)
            ;

        return end - begin;
    }

    /**
     * @brief assign characters to *this
     * @brief this function can be called with any legal state
     * @brief strong exception safety guarantee
     */
    constexpr void assign_(CharT const *first, CharT const *last)
    {
        auto const new_size = static_cast<size_type>(last - first);
        auto const is_long = is_long_();

        // there is no need to worry about whether the ranges overlap,
        // because ::std::ranges::copy will handle it correctly
        if (capacity() >= new_size)
        {
            if (auto const begin = begin_(); begin != first)
                ::std::ranges::copy(first, last, begin);

            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(first, last, ls.begin_);
            dealloc_(is_long);
            long_str_(ls);
        }
    }

    /**
     * @brief insert characters to *this
     * @brief this function can be called with any legal state
     * @brief strong exception safety guarantee
     */
    constexpr void insert_(size_type index, CharT const *first, CharT const *last)
    {
        if (index > size_())
            throw out_of_range();

        auto const length = static_cast<size_type>(last - first);
        auto const new_size = size_() + length;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size && !overlap(first, last, begin, end))
        {
            ::std::ranges::copy_backward(begin + index, end, begin + new_size);
            ::std::ranges::copy(first, last, begin + index);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + index, ls.begin());
            ::std::ranges::copy(begin + index, end, ls.begin() + index + length);
            ::std::ranges::copy(first, last, ls.begin() + index);
            dealloc_(is_long);
            long_str_(ls);
        }
    }

    constexpr void replace_(size_type pos, size_type count, CharT const *first, CharT const *last)
    {
        if (pos > size_())
            throw out_of_range();

        count = ::std::ranges::min(size_() - pos, count);
        auto const length = static_cast<size_type>(last - first);
        auto const new_size = size_() - count + length;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size && !overlap(first, last, begin, end))
        {
            if (count < length)
                ::std::ranges::copy(begin + pos + count, end, begin + pos + length);
            else
                ::std::ranges::copy_backward(begin + pos + count, end, begin + new_size);

            ::std::ranges::copy(first, last, begin + pos);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + pos, ls.begin());
            ::std::ranges::copy(first, last, ls.begin() + pos);
            ::std::ranges::copy(begin + pos + count, end, ls.begin() + pos + length);
            dealloc_(is_long);
            long_str_(ls);
        }
    }

    constexpr void reserve_(size_type new_cap)
    {
        assert(new_cap > capacity());
        auto const size = size_();
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();
        auto const ls = allocate_(new_cap, size);
        ::std::ranges::copy(begin, end, ls.begin());
        dealloc_(is_long);
        long_str_(ls);
    }

    constexpr void reserve_and_drop_(size_type new_cap)
    {
        assert(new_cap > capacity());
        auto const size = size_();
        auto const is_long = is_long_();
        auto const ls = allocate_(new_cap, size);
        dealloc_(is_long);
        long_str_(ls);
    }

  public:
    constexpr ~basic_string()
    {
        dealloc_(is_long_());
    }

    /**
     * @brief reserve memory
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param new_cap new capacity
     */
    constexpr void reserve(size_type new_cap)
    {
        if (new_cap <= capacity())
            return;

        reserve_(new_cap);
    }

    /**
     * @brief resize string length
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param count new size
     * @param ch character to fill
     */
    constexpr void resize(size_type count, value_type ch)
    {
        auto const size = size_();

        if (count <= size)
            return resize_shrink_(is_long_(), count);

        reserve(count);
        ::std::ranges::fill(end_(), end_() + (count - size), ch);
        resize_shrink_(is_long_(), count);
    }

    /**
     * @brief resize string length
     * @brief strong exception safety guarantee
     * @brief never shrink
     * @param count new size
     */
    constexpr void resize(size_type count)
    {
        resize(count, CharT{});
    }

    /**
     * @brief never shrink_to_fit
     */
    constexpr void clear() noexcept
    {
        resize_shrink_(is_long_(), 0uz);
    }

    /**
     * @brief use size * 1.5 for growth
     * @param ch character to fill
     */
    constexpr void push_back(CharT ch)
    {
        auto const size = size_();

        if (capacity() == size)
            reserve_(size * 2uz - size / 2uz);

        *end_() = ch;
        resize_shrink_(is_long_(), size + 1uz);
    }

    // ********************************* begin swap ******************************

  private:
    constexpr void swap_without_ator(basic_string &other) noexcept
    {
        auto &self = *this;
        ::std::ranges::swap(self.stor_, other.stor_);
        ::std::ranges::swap(self.size_flag_, other.size_flag_);
    }

  public:
    constexpr void swap(basic_string &other) noexcept
    {
        if constexpr (atraits_t_::propagate_on_container_swap::value)
            ::std::ranges::swap(other.allocator_, other.allocator_);
        else
            assert(other.allocator_ == allocator_);

        other.swap_without_ator(*this);
    }

    friend void swap(basic_string &lhs, basic_string &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // ********************************* begin constructor ******************************

    constexpr void construct_(::std::size_t n)
    {
        if (short_str_max_ < n)
            long_str_(allocate_(n, n));
        else
            resize_shrink_(false, n);
    }

  public:
    constexpr basic_string() noexcept(noexcept(allocator_type())) = default;

    constexpr explicit basic_string(allocator_type const &a) noexcept : allocator_(a)
    {
    }

    constexpr basic_string(size_type count, value_type ch, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        construct_(count);
        ::std::ranges::fill(begin_(), end_(), ch);
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string(U first, V last, const allocator_type &alloc = allocator_type()) : allocator_(alloc)
    {
        construct_guard_ g{this};

        if constexpr (::std::random_access_iterator<U>)
        {
            construct_(last - first);
            ::std::ranges::copy(first, last, begin_());
        }
        else
        {
            for (; first != last; ++first)
                push_back(*first);
        }

        g.release();
    }

#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
    // tagged constructors to construct from container compatible range
    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string(::std::from_range_t, R &&rg, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        auto first = ::std::ranges::begin(rg);
        auto last = ::std::ranges::end(rg);

        construct_guard_ g{this};

        if constexpr (::std::ranges::sized_range<R>)
        {
            construct_(::std::ranges::size(rg));

            if constexpr (::std::ranges::forward_range<R>)
            {
                ::std::ranges::copy(::std::ranges::begin(rg), ::std::ranges::end(rg), begin_());
            }
            else
            {
                for (auto begin = begin_(); first != last; ++first, ++begin)
                    *begin = *first;
            }
        }
        else
        {
            for (; first != last; ++first)
                push_back(*first);
        }

        g.release();
    }
#endif

    constexpr basic_string(CharT const *s, size_type count, allocator_type const &a = allocator_type()) : allocator_(a)
    {
        construct_(count);
        ::std::ranges::copy(s, s + count, begin_());
    }

    constexpr basic_string(CharT const *s, allocator_type const &a = allocator_type())
        : basic_string(s, c_string_length_(s), a)
    {
    }

    constexpr basic_string(::std::nullptr_t) = delete;

    // clang-format off
    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const &, CharT const*>)
    explicit constexpr basic_string(StringViewLike const & t, allocator_type const& a = allocator_type())
	     : basic_string(::std::basic_string_view<value_type, traits_type>{ t }.data(), ::std::basic_string_view<value_type, traits_type>{ t }.size(), a)
    {
    }
    // clang-format on

    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>>
    constexpr basic_string(StringViewLike const &t, size_type pos, size_type n,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos > sv.size())
            throw out_of_range();

        n = ::std::ranges::min(sv.size() - pos, n);
        construct_(n);
        ::std::ranges::copy(sv.data() + pos, sv.data() + pos + n, begin_());
    }

    constexpr basic_string(basic_string const &other)
        : allocator_(atraits_t_::select_on_container_copy_construction(other.allocator_))
    {
        construct_(other.size_());
        ::std::ranges::copy(other.begin_(), other.end_(), begin_());
    }

    constexpr basic_string(basic_string &&other) noexcept : allocator_(other.allocator_)
    {
        other.swap_without_ator(*this);
    }

    constexpr basic_string(const basic_string &other, allocator_type const &a) : allocator_(a)
    {
        construct_(other.size_());
        ::std::ranges::copy(other.begin_(), other.end_(), begin_());
    }

    constexpr basic_string(basic_string const &other, size_type pos, allocator_type const &a = allocator_type())
        : basic_string(other, pos, npos /* end */, a)
    {
    }

    constexpr basic_string(basic_string &&other, allocator_type const &a) : allocator_(a)
    {
        assert(allocator_ == a);

        if (other.allocator_ == allocator_)
        {
            other.swap_without_ator(*this);
        }
        else
        {
            construct_(other.size_());
            ::std::ranges::copy(other.begin_(), other.end_(), begin_());
        }
    }

    constexpr basic_string(basic_string const &other, size_type pos, size_type count,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        if (pos > other.size_())
            throw out_of_range();

        count = ::std::ranges::min(other.size_() - pos, count);
        construct_(count);
        ::std::ranges::copy(other.begin_() + pos, other.begin_() + pos + count, begin_());
    }

    constexpr basic_string(basic_string &&other, size_type pos, size_type count,
                           allocator_type const &a = allocator_type())
        : allocator_(a)
    {
        assert(allocator_ == a);

        if (pos > other.size_())
            throw out_of_range();

        count = ::std::ranges::min(other.size_() - pos, count);

        if (other.allocator_ == allocator_)
        {
            if (pos != 0uz)
                ::std::ranges::copy(other.begin_() + pos, other.begin_() + pos + count, other.begin_());

            other.resize_shrink_(other.is_long_(), count);
            stor_ = other.stor_;
            size_flag_ = other.size_flag_;
            other.stor_ = storage_type_{};
            other.short_str_(0uz);
        }
        else
        {
            construct_(count);
            ::std::ranges::copy(other.begin_() + pos, other.begin_() + pos + count, begin_());
        }
    }

    constexpr basic_string(::std::initializer_list<value_type> ilist, allocator_type const &a = allocator_type())
        : basic_string(ilist.begin(), ilist.size(), a)
    {
    }

    // ********************************* begin assign ******************************

  private:
    constexpr void copy_ator_and_move_(basic_string &str) noexcept
    {
        dealloc_(is_long_());
        allocator_ = str.allocator_;
        stor_ = str.stor_;
        size_flag_ = str.size_flag_;
        str.stor_ = {};
        str.size_flag_ = 0u;
    }

  public:
    constexpr basic_string &assign(const basic_string &str)
    {
        if (&str == this)
            return *this;

        if constexpr (atraits_t_::propagate_on_container_copy_assignment::value)
        {
            if (allocator_ != str.allocator_)
            {
                basic_string temp{str, str.allocator_};
                copy_ator_and_move_(temp);

                return *this;
            }
        }

        assign_(str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &assign(basic_string &&str) noexcept(
        atraits_t_::propagate_on_container_move_assignment::value || atraits_t_::is_always_equal::value)
    {
        if constexpr (atraits_t_::propagate_on_container_move_assignment::value)
        {
            copy_ator_and_move_(str);
        }
        else
        {
            if constexpr (!atraits_t_::is_always_equal::value)
            {
                if (allocator_ == str.allocator_)
                {
                    str.swap_without_ator(*this);

                    return *this;
                }
            }

            assign_(str.begin_(), str.end_());
        }

        return *this;
    }

    constexpr basic_string &assign(size_type count, CharT ch)
    {
        if (capacity() < count)
            reserve_and_drop_(count);

        auto const is_long = is_long_();
        ::std::ranges::fill(begin_(), begin_() + count, ch);
        resize_shrink_(is_long, count);

        return *this;
    }

    constexpr basic_string &assign(CharT const *s, size_type count)
    {
        assign_(s, s + count);

        return *this;
    }

    constexpr basic_string &assign(CharT const *s)
    {
        assign_(s, s + c_string_length_(s));

        return *this;
    }

    // clang-format off
    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const &, CharT const*>)
    constexpr basic_string& assign(StringViewLike const& t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        assign_(sv.data(), sv.data()+sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const &, CharT const*>)
    constexpr basic_string& assign(StringViewLike const & t, size_type pos, size_type count = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos > sv.size())
            throw out_of_range();

        count = ::std::ranges::min(sv.size() - pos, count);
        assign_(sv.data() + pos, sv.data() + pos + count);

        return *this;
    }
    // clang-format on

    constexpr basic_string &assign(const basic_string &str, size_type pos, size_type count = npos)
    {
        if (pos > str.size_())
            throw out_of_range();

        count = ::std::ranges::min(str.size_() - pos, count);
        assign_(str.begin_() + pos, str.begin_() + pos + count);

        return *this;
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string &assign(U first, V last)
    {
        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            assign_(::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            assign_(temp.begin_(), temp.end_());
        }

        return *this;
    }

    constexpr basic_string &assign(::std::initializer_list<CharT> ilist)
    {
        assign_(ilist.begin(), ilist.end());

        return *this;
    }

    // ********************************* begin operator= ******************************

    constexpr basic_string &operator=(const basic_string &str)
    {
        return assign(str);
    }

    constexpr basic_string &operator=(basic_string &&other) noexcept(
        atraits_t_::propagate_on_container_move_assignment::value || atraits_t_::is_always_equal::value)
    {
        return assign(::std::move(other));
    }

    constexpr basic_string &operator=(CharT const *s)
    {
        return assign(s);
    }

    constexpr basic_string &operator=(CharT ch)
    {
        resize_shrink_(is_long_(), 1uz);
        *begin_() = ch;

        return *this;
    }

    constexpr basic_string &operator=(::std::initializer_list<CharT> ilist)
    {
        return assign(ilist);
    }

    // clang-format off
    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const &, CharT const*>)
    constexpr basic_string& operator=(StringViewLike const & t)
    {
        return assign(t);
    }
    // clang-format on

    constexpr basic_string &operator=(::std::nullptr_t) = delete;

    // ********************************* begin compare ******************************

    friend constexpr ::std::strong_ordering operator<=>(basic_string const &lhs, basic_string const &rhs) noexcept
    {
        auto const lsize = lhs.size_();
        auto const rsize = rhs.size_();

        for (auto begin = lhs.begin_(), end = begin + ::std::ranges::min(lsize, rsize), start = rhs.begin_();
             begin != end; ++begin, ++start)
        {
            if (*begin > *start)
                return ::std::strong_ordering::greater;
            else if (*begin < *start)
                return ::std::strong_ordering::less;
        }

        return lsize <=> rsize;
    }

    friend constexpr ::std::strong_ordering operator<=>(basic_string const &lhs, CharT const *rhs) noexcept
    {

        auto const rsize = basic_string::c_string_length_(rhs);
        auto const lsize = lhs.size_();

        for (auto begin = lhs.begin_(), end = begin + ::std::ranges::min(lsize, rsize), start = rhs; begin != end;
             ++begin, ++start)
        {
            if (*begin > *start)
                return ::std::strong_ordering::greater;
            else if (*begin < *start)
                return ::std::strong_ordering::less;
        }

        return lsize <=> rsize;
    }

  private:
    constexpr bool static equal_(CharT const *begin, CharT const *end, CharT const *first, CharT const *last) noexcept
    {
        if (last - first != end - begin)
            return false;

        if consteval
        {
            for (; begin != end; ++begin, ++first)
            {
                if (*first != *begin)
                    return false;
            }

            return true;
        }
        else
        {
            return begin == end || ::std::memcmp(begin, first, (end - begin) * sizeof(CharT)) == 0;
        }
    }

  public:
    friend constexpr bool operator==(basic_string const &lhs, basic_string const &rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs.begin_(), rhs.end_());
    }

    // Used to resolve ambiguation with operator== provided by std::basic_string_view.
    friend constexpr bool operator==(basic_string const &lhs,
                                     ::std::basic_string_view<value_type, traits_type> rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs.data(), rhs.data() + rhs.size());
    }

    friend constexpr bool operator==(basic_string const &lhs, CharT const *rhs) noexcept
    {
        return equal_(lhs.begin_(), lhs.end_(), rhs, rhs + c_string_length_(rhs));
    }

    // ********************************* begin append ******************************

  private:
    constexpr void append_(CharT const *first, CharT const *last)
    {
        auto const length = static_cast<size_type>(last - first);
        auto const size = size_();
        auto const begin = begin_();
        auto const end = end_();
        auto const new_size = size + length;
        auto const is_long = is_long_();

        if (capacity() >= new_size && !overlap(first, last, begin, end))
        {
            ::std::ranges::copy(first, last, end);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, end, ls.begin());
            ::std::ranges::copy(first, last, ls.begin() + size);
            dealloc_(is_long);
            long_str_(ls);
        }
    }

  public:
    constexpr basic_string &append(size_type count, CharT ch)
    {
        auto const size = size_();
        reserve(size + count);
        ::std::ranges::fill(end_(), end_() + count, ch);
        resize_shrink_(is_long_(), size + count);

        return *this;
    }

    constexpr basic_string &append(CharT const *s, size_type count)
    {
        append_(s, s + count);

        return *this;
    }

    constexpr basic_string &append(CharT const *s)
    {
        append_(s, s + c_string_length_(s));

        return *this;
    }

    // clang-format off
    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const &, CharT const*>)
    constexpr basic_string& append(StringViewLike const & t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        append_(sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const &, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const &, CharT const*>)
    constexpr basic_string& append(StringViewLike const & t, size_type pos, size_type count = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos > sv.size())
            throw out_of_range();

        count = ::std::ranges::min(sv.size() - pos, count);
        append_(sv.data() + pos,sv.data() + pos + count);

        return *this;
    }
    // clang-format on

    constexpr basic_string &append(const basic_string &str)
    {
        append_(str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &append(const basic_string &str, size_type pos, size_type count = npos)
    {
        if (pos > str.size_())
            throw out_of_range();

        count = ::std::ranges::min(str.size_() - pos, count);
        append_(str.begin_() + pos, str.begin_() + pos + count);

        return *this;
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string &append(U first, V last)
    {
        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            append_(::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            append_(temp.begin_(), temp.end_());
        }

        return *this;
    }

    constexpr basic_string &append(::std::initializer_list<CharT> ilist)
    {
        append_(ilist.begin(), ilist.end());

        return *this;
    }

    // ********************************* begin operator+= ******************************

    constexpr basic_string &operator+=(const basic_string &str)
    {
        return append(str);
    }

    constexpr basic_string &operator+=(CharT ch)
    {
        push_back(ch);

        return *this;
    }

    constexpr basic_string &operator+=(CharT const *s)
    {
        return append(s);
    }

    constexpr basic_string &operator+=(::std::initializer_list<CharT> ilist)
    {
        return append(ilist);
    }

    // clang-format off
    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const&, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const&, CharT const*>)
    constexpr basic_string& operator+=(StringViewLike const& t)
    {
        return append(t);
    }
    // clang-format on

    // ********************************* begin search ******************************

  public:
    constexpr bool starts_with(::std::basic_string_view<value_type, traits_type> sv) const noexcept
    {
        return sv.size() <= size_() && equal_(sv.data(), sv.data() + sv.size(), begin_(), begin_() + sv.size());
    }

    constexpr bool starts_with(CharT ch) const noexcept
    {
        return *begin_() == ch;
    }

    constexpr bool starts_with(CharT const *s) const
    {
        auto const length = c_string_length_(s);

        return length <= size_() && equal_(s, s + length, begin_(), begin_() + length);
    }

    constexpr bool ends_with(::std::basic_string_view<value_type, traits_type> sv) const noexcept
    {
        return sv.size() <= size_() && equal_(sv.data(), sv.data() + sv.size(), end_() - sv.size(), end_());
    }

    constexpr bool ends_with(CharT ch) const noexcept
    {
        return !empty() && *(end_() - 1) == ch;
    }

    constexpr bool ends_with(CharT const *s) const
    {
        auto const length = c_string_length_(s);

        return length <= size_() && equal_(s, s + length, end_() - length, end_());
    }

#if defined(__cpp_lib_string_contains) && (__cpp_lib_string_contains >= 202011L)
    constexpr bool contains(::std::basic_string_view<value_type, traits_type> sv) const noexcept
    {
        return ::std::basic_string_view<value_type, traits_type>{begin_(), end_()}.contains(sv);
    }

    constexpr bool contains(CharT ch) const noexcept
    {
        for (auto begin = begin_(), end = end_(); begin != end; ++begin)
        {
            if (*begin == ch)
                return true;
        }

        return false;
    }

    constexpr bool contains(CharT const *s) const noexcept
    {
        return ::std::basic_string_view<value_type, traits_type>{begin_(), end_()}.contains(
            ::std::basic_string_view<value_type, traits_type>{s, s + c_string_length_(s)});
    }
#endif

    // ********************************* begin substr ******************************

    constexpr basic_string substr(size_type pos = 0uz, size_type count = npos) const &
    {
        return basic_string{*this, pos, count};
    }

    constexpr basic_string substr(size_type pos = 0uz, size_type count = npos) &&
    {
        return basic_string{std::move(*this), pos, count};
    }

    // ********************************* begin insert ******************************

    constexpr basic_string &insert(size_type index, size_type count, value_type ch)
    {
        if (index > size_())
            throw out_of_range();

        auto const new_size = size_() + count;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size)
        {
            ::std::ranges::copy_backward(begin + index, end, end + count);
            ::std::ranges::fill(begin + index, begin + index + count, ch);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + index, ls.begin());
            ::std::ranges::copy(begin + index, end, ls.begin() + index + count);
            ::std::ranges::fill(ls.begin() + index, ls.begin() + index + count, ch);
            dealloc_(is_long);
            long_str_(ls);
        }

        return *this;
    }

    constexpr basic_string &insert(size_type index, CharT const *s)
    {
        insert_(index, s, s + c_string_length_(s));

        return *this;
    }

    constexpr basic_string &insert(size_type index, CharT const *s, size_type count)
    {
        insert_(index, s, s + count);

        return *this;
    }

    constexpr basic_string &insert(size_type index, const basic_string &str)
    {
        insert_(index, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &insert(size_type index, const basic_string &str, size_type s_index, size_type count = npos)
    {
        if (s_index > str.size_())
            throw out_of_range();

        count = ::std::ranges::min(str.size_() - s_index, count);
        insert_(index, str.begin_() + s_index, str.begin_() + s_index + count);

        return *this;
    }

    constexpr iterator insert(const_iterator pos, CharT ch)
    {
        assert(pos.base().current_ >= begin_() && pos.base().current_ <= end_());
        auto const size = size_();
        auto const index = pos.base().current_;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= size + 1uz)
        {
            ::std::ranges::copy_backward(index, end, end + 1uz);
            *index = ch;
            resize_shrink_(is_long, size + 1);

#ifndef NDEBUG
            return {index, this};
#else
            return {index};
#endif
        }
        else
        {
            auto const ls = allocate_(size * 2uz - size / 2uz, size + 1uz);
            ::std::ranges::copy(begin, index, ls.begin());
            auto const new_index = ls.begin() + (index - begin);
            *new_index = ch;
            ::std::ranges::copy(index, end, new_index + 1uz);
            dealloc_(is_long);
            long_str_(ls);

#ifndef NDEBUG
            return {new_index, this};
#else
            return {new_index};
#endif
        }
    }

    constexpr iterator insert(const_iterator pos, size_type count, CharT ch)
    {
        auto const index = pos.base().current_ - begin_();
        insert(index, count, ch);

#ifndef NDEBUG
        return {begin_() + index, this};
#else
        return {begin_() + index};
#endif
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr iterator insert(const_iterator pos, U first, V last)
    {
        assert(pos.base().current_ >= begin_() && pos.base().current_ <= end_());
        auto const index = pos.base().current_ - begin_();

        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            insert_(index, ::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            basic_string temp{first, last, allocator_};
            insert_(index, temp.begin_(), temp.end_());
        }

#ifndef NDEBUG
        return {begin_() + index, this};
#else
        return {begin_() + index};
#endif
    }

    constexpr iterator insert(const_iterator pos, ::std::initializer_list<CharT> ilist)
    {
        auto const index = pos.base().current_ - begin_();
        insert_(index, ilist.begin(), ilist.begin() + ilist.size());

#ifndef NDEBUG
        return {begin_() + index, this};
#else
        return {begin_() + index};
#endif
    }

    // clang-format off
    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const&, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const&, CharT const*>)
    constexpr basic_string& insert(size_type pos, StringViewLike const& t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        insert_(pos, sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<StringViewLike const&, ::std::basic_string_view<value_type, traits_type>> && (!::std::is_convertible_v<StringViewLike const&, CharT const*>)
    constexpr basic_string& insert(size_type pos, StringViewLike const& t, size_type t_index, size_type count = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (t_index > sv.size())
            throw out_of_range();

        count = ::std::ranges::min(sv.size() - t_index, count);
        insert_(pos, sv.data() + t_index, sv.data() + t_index + count);

        return *this;
    }
    // clang-format on

    // ********************************* begin erase ******************************

  private:
    constexpr void erase_(CharT *first, CharT const *last) noexcept
    {
        assert(first >= begin_() && last <= end_());
        auto const is_long = is_long_();
        auto const size = size_();
        ::std::ranges::copy(last, end_(), first);
        resize_shrink_(is_long, size - (last - first));
    }

  public:
    constexpr basic_string &erase(size_type index = 0uz, size_type count = npos)
    {
        if (index > size_())
            throw out_of_range();

        count = ::std::ranges::min(size_() - index, count);
        erase_(begin_() + index, begin_() + index + count);

        return *this;
    }

    constexpr iterator erase(const_iterator position) noexcept
    {
        auto const start = position.base().current_;
        assert(start >= begin_() && start <= end_());
        auto const is_long = is_long_();
        auto const size = size_();
        ::std::ranges::copy(start + 1uz, end_(), start);
        resize_shrink_(is_long, size - 1uz);

#ifndef NDEBUG
        return {start, this};
#else
        return {start};
#endif
    }

    constexpr iterator erase(const_iterator first, const_iterator last) noexcept
    {
        auto const start = first.base().current_;
        erase_(start, last.base().current_);

#ifndef NDEBUG
        return {start, this};
#else
        return {start};
#endif
    }

    // ********************************* begin pop_back ******************************

    constexpr void pop_back() noexcept
    {
        assert(!empty());
        resize_shrink_(is_long_(), size_() - 1uz);
    }

    // ********************************* begin replace ******************************

    constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str)
    {
        replace_(pos, count, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, const basic_string &str)
    {
        auto const start = first.base().current_;
        assert(start >= begin_() && start <= end_() && first <= last);
        replace_(start - begin_(), last - first, str.begin_(), str.end_());

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, const basic_string &str, size_type pos2,
                                    size_type count2 = npos)
    {
        if (pos2 > str.size_())
            throw out_of_range();

        count2 = ::std::ranges::min(count2, str.size_() - pos2);
        replace_(pos, count, str.begin_() + pos2, str.begin_() + pos2 + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, CharT const *cstr, size_type count2)
    {
        replace_(pos, count, cstr, cstr + count2);

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, CharT const *cstr, size_type count2)
    {
        assert(first >= begin() && last <= end() && first <= last);
        replace_(first.base().current_ - begin_(), last - first, cstr, cstr + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, CharT const *cstr)
    {
        replace_(pos, count, cstr, cstr + c_string_length_(cstr));

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, CharT const *cstr)
    {
        assert(first >= begin() && last <= end() && first <= last);
        replace_(first.base().current_ - begin_(), last - first, cstr, cstr + c_string_length_(cstr));

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, ::std::initializer_list<CharT> ilist)
    {
        assert(first <= last);
        replace_(first.base().current_ - begin_(), last - first, ilist.begin(), ilist.begin() + ilist.size());

        return *this;
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<value_type, traits_type>> &&
                 (!::std::is_convertible_v<const StringViewLike &, CharT const *>)
    constexpr basic_string &replace(size_type pos, size_type count, const StringViewLike &t)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;
        replace_(pos, count, sv.data(), sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<value_type, traits_type>> &&
                 (!::std::is_convertible_v<const StringViewLike &, CharT const *>)
    constexpr basic_string &replace(const_iterator first, const_iterator last, const StringViewLike &t)
    {
        assert(first >= begin() && last <= end() && first <= last);
        ::std::basic_string_view<value_type, traits_type> sv = t;
        auto sv_data = sv.data();
        replace_(first.base().current_ - begin_(), static_cast<size_type>(last - first), sv.data(),
                 sv.data() + sv.size());

        return *this;
    }

    template <typename StringViewLike>
        requires ::std::is_convertible_v<const StringViewLike &, ::std::basic_string_view<value_type, traits_type>> &&
                 (!::std::is_convertible_v<const StringViewLike &, CharT const *>)
    constexpr basic_string &replace(size_type pos, size_type count, const StringViewLike &t, size_type pos2,
                                    size_type count2 = npos)
    {
        ::std::basic_string_view<value_type, traits_type> sv = t;

        if (pos2 > sv.size())
            throw out_of_range();

        count2 = ::std::ranges::min(sv.size() - pos2, count2);
        replace_(pos, count, sv.data() + pos2, sv.data() + pos2 + count2);

        return *this;
    }

    constexpr basic_string &replace(size_type pos, size_type count, size_type count2, CharT ch)
    {
        if (pos > size_())
            throw out_of_range();

        count = ::std::ranges::min(size_() - pos, count);
        auto const size = size_();
        auto const new_size = size - count + count2;
        auto const begin = begin_();
        auto const end = end_();
        auto const is_long = is_long_();

        if (capacity() >= new_size)
        {
            if (count > count2)
                ::std::ranges::copy(begin + pos + count, end, begin + pos + count2);
            else if (count < count2)
                ::std::ranges::copy_backward(begin + pos + count, end, begin + new_size);

            ::std::ranges::fill(begin + pos, begin + pos + count2, ch);
            resize_shrink_(is_long, new_size);
        }
        else
        {
            auto const ls = allocate_(new_size, new_size);
            ::std::ranges::copy(begin, begin + pos, ls.begin());
            ::std::ranges::copy(begin + pos + count, end, ls.begin() + pos + count2);
            ::std::ranges::fill(ls.begin() + pos, ls.begin() + pos + count2, ch);
            dealloc_(is_long);
            long_str_(ls);
        }

        return *this;
    }

    constexpr basic_string &replace(const_iterator first, const_iterator last, size_type count2, CharT ch)
    {
        assert(first <= last);
        replace(first.base().current_ - begin_(), last - first, count2, ch);

        return *this;
    }

    template <typename U, typename V>
        requires ::std::input_iterator<U> && (!::std::is_integral_v<V>)
    constexpr basic_string &replace(const_iterator first, const_iterator last, U first2, V last2)
    {
        assert(first <= last);
        if constexpr (::std::contiguous_iterator<U> && ::std::is_same_v<value_type, ::std::iter_value_t<U>>)
        {
            replace_(first.base().current_ - begin_(), last - first, ::std::to_address(first2),
                     ::std::to_address(first2) + ::std::ranges::distance(first2, last2));
        }
        else
        {
            basic_string temp{first2, last2, allocator_};
            replace_(first.base().current_ - begin_(), last - first, temp.begin_(), temp.end_());
        }

        return *this;
    }

    // ********************************* begin assign_range/insert_range/append_range ******************************
#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string &assign_range(R &&rg)
    {
        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first = ::std::ranges::begin(rg);
            auto const last = ::std::ranges::end(rg);
            assign_(::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            assign(basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return *this;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr iterator insert_range(const_iterator pos, R &&rg)
    {
        auto const index = pos - begin();

        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first = ::std::ranges::begin(rg);
            auto const last = ::std::ranges::end(rg);
            insert_(index, ::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            insert(index, basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return begin() + index;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string &append_range(R &&rg)
    {
        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first = ::std::ranges::begin(rg);
            auto const last = ::std::ranges::end(rg);
            append_(::std::to_address(first), ::std::to_address(first) + ::std::ranges::distance(first, last));
        }
        else
        {
            append(basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return *this;
    }

    template <::std::ranges::input_range R>
        requires ::std::convertible_to<::std::ranges::range_value_t<R>, CharT>
    constexpr basic_string &replace_with_range(const_iterator first, const_iterator last, R &&rg)
    {
        if constexpr (::std::ranges::contiguous_range<R> &&
                      ::std::is_same_v<value_type, ::std::ranges::range_value_t<R>>)
        {
            auto const first2 = ::std::ranges::begin(rg);
            auto const last2 = ::std::ranges::end(rg);
            replace_(first.base().current_ - begin_(), last - first, ::std::to_address(first2),
                     ::std::to_address(first2) + ::std::ranges::distance(first2, last2));
        }
        else
        {
            replace(first, last, basic_string(::std::from_range, ::std::forward<R>(rg), allocator_));
        }

        return *this;
    }
#endif

    template <class Operation>
    constexpr void resize_and_overwrite(size_type count, Operation op)
    {
        reserve(count);
        resize_shrink_(is_long_(), std::move(op)(begin_(), size_type(count)));
    }

    constexpr operator ::std::basic_string_view<CharT, Traits>() noexcept
    {
        return {begin_(), end_()};
    }

    constexpr allocator_type get_allocator() const noexcept
    {
        return allocator_;
    }
};

template <class InputIterator,
          class Alloc = ::std::allocator<typename ::std::iterator_traits<InputIterator>::value_type>>
basic_string(InputIterator, InputIterator, Alloc = Alloc())
    -> basic_string<typename ::std::iterator_traits<InputIterator>::value_type,
                    ::std::char_traits<typename ::std::iterator_traits<InputIterator>::value_type>, Alloc>;

template <::std::ranges::input_range R, class Alloc = ::std::allocator<::std::ranges::range_value_t<R>>>
basic_string(::std::from_range_t, R &&, Alloc = Alloc())
    -> basic_string<::std::ranges::range_value_t<R>, ::std::char_traits<::std::ranges::range_value_t<R>>, Alloc>;

template <class CharT, class Traits, class Alloc = ::std::allocator<CharT>>
explicit basic_string(::std::basic_string_view<CharT, Traits>, const Alloc & = Alloc())
    -> basic_string<CharT, Traits, Alloc>;

template <class CharT, class Traits, class Alloc = ::std::allocator<CharT>>
basic_string(::std::basic_string_view<CharT, Traits>, ::std::size_t, ::std::size_t, const Alloc & = Alloc())
    -> basic_string<CharT, Traits, Alloc>;

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs, const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        std::allocator_traits<Alloc>::select_on_container_copy_construction(lhs.get_allocator())};
    r.reserve(lhs.size() + rhs.size());
    r.append(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs,
    ::std::type_identity_t<::std::basic_string_view<CharT, Traits>> rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        std::allocator_traits<Alloc>::select_on_container_copy_construction(lhs.get_allocator())};
    r.reserve(lhs.size() + rhs.size());
    r.append(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    ::std::type_identity_t<::std::basic_string_view<CharT, Traits>> lhs,
    const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    bizwen::basic_string<CharT, Traits, Alloc> r{
        ::std::allocator_traits<Alloc>::select_on_container_copy_construction(rhs.get_allocator())};
    r.reserve(lhs.size() + rhs.size());
    r.append(lhs);
    r.append(rhs);

    return r;
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(bizwen::basic_string<CharT, Traits, Alloc> &&lhs,
                                                                      bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    lhs.append(rhs);

    return std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    bizwen::basic_string<CharT, Traits, Alloc> &&lhs, const bizwen::basic_string<CharT, Traits, Alloc> &rhs)
{
    lhs.append(rhs);

    return std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    bizwen::basic_string<CharT, Traits, Alloc> &&lhs, ::std::type_identity_t<std::basic_string_view<CharT, Traits>> rhs)
{
    lhs.append(rhs);

    return std::move(lhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    const bizwen::basic_string<CharT, Traits, Alloc> &lhs, bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    rhs.insert(0uz, lhs);

    return std::move(rhs);
}

template <class CharT, class Traits, class Alloc>
inline constexpr bizwen::basic_string<CharT, Traits, Alloc> operator+(
    std::type_identity_t<std::basic_string_view<CharT, Traits>> lhs, bizwen::basic_string<CharT, Traits, Alloc> &&rhs)
{
    rhs.insert(0uz, lhs);

    return std::move(rhs);
}

template <class CharT, class Traits, class Alloc, class U = CharT>
inline constexpr typename basic_string<CharT, Traits, Alloc>::size_type erase(basic_string<CharT, Traits, Alloc> &c,
                                                                              const U &value)
{
    auto const r = std::ranges::size(std::ranges::remove(c, value));
    c.resize(c.size() - r);

    return r;
}

template <class CharT, class Traits, class Alloc, class Pred>
inline constexpr typename basic_string<CharT, Traits, Alloc>::size_type erase_if(basic_string<CharT, Traits, Alloc> &c,
                                                                                 Pred pred)
{
    auto const r = std::ranges::size(std::ranges::remove_if(c, pred));
    c.resize(c.size() - r);

    return r;
}

using string = bizwen::basic_string<char>;
using wstring = bizwen::basic_string<wchar_t>;
using u8string = bizwen::basic_string<char8_t>;
using u16string = bizwen::basic_string<char16_t>;
using u32string = bizwen::basic_string<char32_t>;

static_assert(sizeof(string) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(wstring) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(u8string) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(u16string) == sizeof(char8_t *) * 4uz);
static_assert(sizeof(u32string) == sizeof(char8_t *) * 4uz);
static_assert(::std::contiguous_iterator<string::iterator>);

namespace pmr
{
template <class CharT, class Traits = ::std::char_traits<CharT>>
using basic_string = basic_string<CharT, Traits, ::std::pmr::polymorphic_allocator<CharT>>;

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
using u8string = basic_string<char8_t>;
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;
} // namespace pmr
} // namespace bizwen

namespace std
{
template <typename CharT, typename Traits, typename Allocator>
struct hash<bizwen::basic_string<CharT, Traits, Allocator>>
{
    static constexpr ::std::size_t operator()(bizwen::basic_string<CharT, Traits, Allocator> const &str) noexcept
    {
        using view = ::std::basic_string_view<CharT, Traits>;
        ::std::hash<view> hasher;

        return hasher(view(str));
    }
};
} // namespace std
