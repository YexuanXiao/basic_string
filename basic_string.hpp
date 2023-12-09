#include <memory>
#include <string_view>
#include <array>
#include <concepts>
#include <utility>
#include <cassert>
#include <compare>
#include <stdexcept>
#include <cstring>
#include <iterator>
#include <version>

namespace bizwen
{
    template <typename T> struct is_character: std::bool_constant<std::is_same_v<char, std::remove_cv_t<T>> || std::is_same_v<signed char, std::remove_cv_t<T>> || std::is_same_v<unsigned char, std::remove_cv_t<T>> || std::is_same_v<wchar_t, std::remove_cv_t<T>> || std::is_same_v<char8_t, std::remove_cv_t<T>> || std::is_same_v<char16_t, std::remove_cv_t<T>> || std::is_same_v<char32_t, std::remove_cv_t<T>>>
    {
    };

    template <typename T> constexpr bool is_character_v = is_character<T>::value;

    template <typename T>
    concept character = is_character_v<T>;

    template <character CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>> class basic_string
    {
    private:
        /*
         * @brief type of null tag
         */
        enum class null_t : char
        {
            null_v
        };

        using null_t::null_v;

        /**
         * @brief type of long string
         */
        struct ls_type_
        {
            CharT* begin_{};
            CharT* end_{};
            CharT* last_{};
        };

        /**
         * @brief union storage long string and short string
         */
        union storage_type_ {
            std::array<CharT, sizeof(CharT*) * 4 / sizeof(CharT) - 1> ss_;
            ls_type_ ls_;
        };

        /**
         * https://github.com/microsoft/STL/issues/1364
         */
#ifdef _MSC_VER
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
         * @brief flag = -1: long string, length of string is end - begin
         * @brief flag = -2: null string
         */
        signed char size_flag_{};

        /**
         * @brief short_string_max_ is the max length of short string
         */
        static inline constexpr std::size_t short_string_max_{ sizeof(storage_type_::ss_) / sizeof(CharT*) - 1 };
        using atraits_t_ = std::allocator_traits<Allocator>;

        constexpr bool is_long_() const noexcept
        {
            assert(("string is null", !is_null_()));
            return size_flag_ == -1;
        }

        constexpr bool is_short_() const noexcept
        {
            assert(("string is null", !is_null_()));
            return size_flag_ > 0;
        }

        constexpr bool is_empty_() const noexcept
        {
            assert(("string is null", !is_null_()));
            return !size_flag_;
        }

        constexpr bool is_null_() const noexcept
        {
            return size_flag_ == -2;
        }

        constexpr std::size_t size_() const noexcept
        {
            assert(("string is null", !is_null_()));
            if (is_long_())
            {
                auto&& ls = stor_.ls_;
                return ls.end_ - ls.begin_;
            }
            else
            {
                return size_flag_;
            }
        }

    public:
        using traits_type = Traits;
        using value_type = CharT;
        using allocator_type = Allocator;
        using size_type = std::allocator_traits<Allocator>::size_type;
        using difference_type = std::allocator_traits<Allocator>::difference_type;
        using reference = value_type&;
        using const_reference = value_type const&;
        using pointer = std::allocator_traits<Allocator>::pointer;
        using const_pointer = std::allocator_traits<Allocator>::const_pointer;

        static inline constexpr size_type npos = -1;

        /**
         * @brief check if string is null
         * @brief the only way to create a null string is through the null_t constructor
         */
        constexpr bool null() const noexcept
        {
            return is_null_();
        }

        /**
         * @brief transform null string to empty string
         * @brief operator= and transform is the only legal modification operations for a null string
         */
        constexpr void transform_null_to_empty() noexcept
        {
            assert(("only null string can call transform null to empty", !is_null_()));
            size_flag_ = 0;
        }

        // ********************************* begin volume ******************************

        constexpr bool empty() const noexcept
        {
            return is_empty_();
        }

        constexpr std::size_t size() const noexcept
        {
            return size_();
        }

        constexpr std::size_t length() const noexcept
        {
            return size_();
        }

        /**
         * @return size_type{ -1 } / 2 on 64-bit arch
         * @return size_type{ -1 } - 2 on 32-bit arch
         */
        constexpr size_type max_size() const noexcept
        {
            if constexpr (sizeof(size_type) > 4)
                return size_type{ -1 } / 2;
            else
                return size_type{ -1 } - 2;
        }

        constexpr size_type capacity() const noexcept
        {
            if (is_long_())
            {
                auto&& ls = stor_.ls_;
                return ls.last_ - ls.begin_ - 1;
            }
            else
            {
                return short_string_max_;
            }
        }

        /**
         * @brief never shrink
         */
        constexpr void shrink_to_fit() const noexcept
        {
            return;
        }

        // ********************************* begin element access ******************************

    private:
        /**
         * @brief internal use
         * @return a pointer to the first element
         */
        constexpr CharT const* begin_() const noexcept
        {
            assert(("string is null", !is_null_()));
            if (is_long_())
            {
                return stor_.ls_.begin_;
            }
            else
            {
                return stor_.ss_.data();
            }
        }

        /**
         * @brief internal use
         * @return a pointer to the first element
         */
        constexpr CharT* begin_() noexcept
        {
            return const_cast<CharT*>(const_cast<basic_string const&>(*this).begin_());
        }

        /**
         * @brief internal use
         * @return a pointer to the next position of the last element
         */
        constexpr CharT* end_() noexcept
        {
            return begin_() + size_();
        }

        /**
         * @brief internal use
         * @return a pointer to the next position of the last element
         */
        constexpr CharT const* end_() const noexcept
        {
            return begin_() + size_();
        }

    public:
        constexpr CharT const* data() const noexcept
        {
            return begin_();
        }

        constexpr CharT* data() noexcept
        {
            return begin_();
        }

        constexpr CharT const* c_str() const noexcept
        {
            return begin_();
        }

        constexpr const_reference at(size_type pos) const
        {
            if (pos >= size_())
                throw std::out_of_range{ {} };
            return *(begin_() + pos);
        }

        constexpr reference at(size_type pos)
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this).at(pos));
        }

        constexpr const_reference operator[](size_type pos) const noexcept
        {
            assert(("pos >= size, please check the arg", pos < size_()));
            return *(begin_() + pos);
        }

        constexpr reference operator[](size_type pos) noexcept
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this)[pos]);
        }

        constexpr const CharT& front() const noexcept
        {
            assert(("string is empty", !is_empty_()));
            return *begin_();
        }

        constexpr CharT& front()
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this).front());
        }

        constexpr const CharT& back() const noexcept
        {
            assert(("string is empty", !is_empty_()));
            return *(end_() - 1);
        }

        constexpr CharT& back()
        {
            return const_cast<reference>(const_cast<basic_string const&>(*this).back());
        }

        constexpr operator std::basic_string_view<CharT, Traits>() const noexcept
        {
            return std::basic_string_view<CharT, Traits>(begin_(), end_());
        }

        // ********************************* begin iterator type ******************************

    private:
        struct iterator_type_
        {
            using difference_type = std::ptrdiff_t;
            using value_type = CharT;
            using pointer = std::add_pointer_t<value_type>;
            using reference = std::add_lvalue_reference_t<CharT>;
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::contiguous_iterator_tag;

            CharT* current_{};

#ifndef NDEBUG
            basic_string* target_{};
#endif

            constexpr void check() const noexcept
            {
#ifndef NDEBUG
                if (current_ && current_ <= target_->end_() && current_ >= target_->begin_())
                    ;
                else
                    assert(("iterator is invalidated", false));
#endif
            }

            friend class basic_string;

            iterator_type_() noexcept = default;
            iterator_type_(iterator_type_ const&) noexcept = default;
            iterator_type_(iterator_type_&&) noexcept = default;
            iterator_type_& operator=(iterator_type_ const&) & noexcept = default;
            iterator_type_& operator=(iterator_type_&&) & noexcept = default;

#ifndef NDEBUG
            constexpr iterator_type_(CharT* current, basic_string* target) : current_(current), target_(target)
            {
            }
#else
            constexpr iterator_type_(CharT* current) : current_(current)
            {
            }
#endif

            constexpr iterator_type_ operator+(difference_type n) const& noexcept
            {
                auto temp = *this;
                temp.current_ += n;
                temp.check();
                return temp;
            }

            constexpr iterator_type_ operator-(difference_type n) const& noexcept
            {
                auto temp = *this;
                temp.current_ -= n;
                temp.check();
                return temp;
            }

            constexpr friend iterator_type_ operator+(difference_type n, iterator_type_ const& rhs) noexcept
            {
                auto temp = rhs;
                temp.current_ += n;
                temp.check();
                return temp;
            }

            constexpr friend iterator_type_ operator-(difference_type n, iterator_type_ const& rhs) noexcept
            {
                auto temp = rhs;
                temp.current_ -= n;
                temp.check();
                return temp;
            }

            constexpr friend difference_type operator-(iterator_type_ const& lhs, iterator_type_ const& rhs) noexcept
            {
                assert(("iter belongs to different strings", lhs.target_ == rhs.target_));
                return lhs.current_ - rhs.current_;
            }

            constexpr iterator_type_& operator+=(difference_type n) & noexcept
            {
                current_ += n;
                check();
                return *this;
            }

            constexpr iterator_type_& operator-=(difference_type n) & noexcept
            {
                current_ -= n;
                check();
                return *this;
            }

            constexpr iterator_type_& operator++() & noexcept
            {
                ++current_;
                check();
                return *this;
            }

            constexpr iterator_type_& operator--() & noexcept
            {
                --current_;
                check();
                return *this;
            }

            constexpr iterator_type_ operator++(int) const& noexcept
            {
                ++current_;
                check();
                return *this;
            }

            constexpr iterator_type_ operator--(int) const& noexcept
            {
                --current_;
                check();
                return *this;
            }

            constexpr CharT& operator[](difference_type n) const noexcept
            {
#ifndef NDEBUG
                iterator_type_ end = (*this) + n;
                end.check();
#endif
                return *(current_ + n);
            }

            constexpr CharT& operator*() const noexcept
            {
                return *current_;
            }

            constexpr CharT* operator->() const noexcept
            {
                return current_;
            }

            friend constexpr std::strong_ordering operator<=>(iterator_type_ const&, iterator_type_ const&) noexcept = default;
        };

        // ********************************* begin iterator function ******************************

    public:
        using iterator = iterator_type_;
        using const_iterator = std::basic_const_iterator<iterator_type_>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr iterator begin() noexcept
        {
#ifndef NDEBUG
            return { begin_(), this };
#else
            return { begin_() };
#endif
        }

        constexpr iterator end() noexcept
        {
#ifndef NDEBUG
            return iterator_type_{ end_(), this };
#else
            return iterator_type_{ end_() };
#endif
        }

        constexpr const_iterator begin() const noexcept
        {
#ifndef NDEBUG
            return iterator_type_{ const_cast<CharT*>(begin_()), const_cast<basic_string*>(this) };
#else
            return iterator_type_{ const_cast<CharT*>(begin_()) };
#endif
        }

        constexpr const_iterator end() const noexcept
        {
#ifndef NDEBUG
            return iterator_type_{ const_cast<CharT*>(end_()), const_cast<basic_string*>(this) };
#else
            return iterator_type_{ const_cast<CharT*>(end_()) };
#endif
        }

        constexpr const_iterator cbegin() noexcept
        {
            return begin();
        }

        constexpr const_iterator cend() noexcept
        {
            return end();
        }

        // ********************************* begin memory management ******************************

    private:
        /**
         * @brief allocates memory and automatically adds 1 to store trailing zero
         * @brief strong exception safety guarantee
         * @brief not responsible for reclaiming memory
         * @brief never shrink
         * @param n, expected number of characters
         */
        constexpr void allocate_plus_one_(size_type n)
        {
            assert(("string is null", !is_null_()));
            // strong exception safe grantee
            if (n <= short_string_max_ && !is_long_())
            {
                size_flag_ = static_cast<signed char>(n);
                return;
            }

            ++n;

#if defined(__cpp_lib_allocate_at_least) && (__cpp_lib_allocate_at_least >= 202302L)
            auto&& [ptr, count] = atraits_t_.allocate_at_least(allocator_, n);
            stor_.ls_ = { ptr, nullptr, ptr + count };
#else
            auto&& ptr = atraits_t_::allocate(allocator_, n);
            stor_.ls_ = { ptr, nullptr, ptr + n };
#endif
            size_flag_ = -1;
        }

        /**
         * @brief dealloc the memory of long string
         * @brief static member function
         * @param ls, allocated long string
         */
        constexpr void dealloc_(ls_type_& ls) noexcept
        {
            assert(("string is null", !is_null_()));
            atraits_t_::deallocate(allocator_, ls.begin_, ls.last_ - ls.begin_);
        }

        /**
         * @brief conditionally sets size correctly. only legal if n < capacity()
         * @brief write 0 to the tail at the same time
         * @brief never shrink
         * @param n, expected string length
         */
        constexpr void resize_(size_type n) noexcept
        {
            assert(("n > capacity()", n <= capacity()));
            if (is_long_())
            {
                // if n = 0, keep storage avilable
                auto&& ls = stor_.ls_;
                ls.end_ = ls.begin_ + n;
                // return advance
                *ls.end_ = CharT{};
            }
            else
            {
                size_flag_ = static_cast<signed int>(n);
                stor_.ss_[n] = CharT{};
            }
        }

        /**
         * @brief fill characters to *this
         * @brief not doing anything else
         * @param begin begin of characters
         * @param end end of characters
         */
        constexpr void fill_(CharT const* begin, CharT const* end) noexcept
        {
            assert(("string is null", !is_null_()));
            assert(("cannot storage string in current allocated storage", static_cast<size_type>(end - begin) <= capacity()));
            std::copy(begin, end, begin_());
        }

        /**
         * @brief caculate the length of c style string
         * @param begin begin of characters
         * @return length
         */
        constexpr static size_type c_style_string_length_(CharT const* begin) noexcept
        {
#if defined(__cpp_if_consteval) && (__cpp_if_consteval >= 202106L)
            if !consteval
#else
            if constexpr (!std::is_constant_evaluated())
#endif
            {
                if constexpr (std::is_same_v<char, CharT>)
                {
                    return std::strlen(begin);
                }
                else if (std::is_same_v<wchar_t, CharT>)
                {
                    return std::wcslen(begin);
                }
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
         * @brief this function provide for assign and operator=
         * @param begin begin of characters
         * @param end end of characters
         */
        constexpr void assign_(CharT const* begin, CharT const* end)
        {
            auto size = end - begin;
            if (capacity() < static_cast<size_type>(size))
            {
                if (is_long_())
                {
                    auto ls = stor_.ls_;
                    allocate_plus_one_(size);
                    dealloc_(ls);
                }
                else
                {
                    allocate_plus_one_(size);
                }
            }

            fill_(begin, end);
            resize_(size);
        }

    public:
        /**
         * @brief reserve memory
         * @brief strong exception safety guarantee
         * @brief never shrink
         * @param new_cap new capacity
         */
        constexpr void reserve(size_type new_cap)
        {
            if (capacity() < new_cap)
            {
                if (is_long_())
                {
                    auto ls = stor_.ls_;
                    allocate_plus_one_(new_cap);
                    fill_(ls.begin_, ls.end_);
                    dealloc_(ls);
                    resize_(ls.end_ - ls.begin_);
                }
                else
                {
                    auto ss = stor_.ss_;
                    auto size = size_flag_;
                    auto data = ss.data();
                    allocate_plus_one_(new_cap);
                    fill_(data, data + size);
                    resize(size);
                }
            }
        }

        /**
         * @brief resize string length
         * @brief strong exception safety guarantee
         * @brief never shrink
         * @param count new size
         * @param ch character to fill
         */
        constexpr void resize(size_type count, CharT ch)
        {
            if (count > size_())
            {
                reserve(count);
                auto end = end_();
                std::fill(end, end + count, ch);
            }
            resize_(count);
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
         * @brief equal to resize(0)
         * @brief never shrink
         */
        constexpr void clear() noexcept
        {
            resize_(0);
        }

        /**
         * @brief use size * 2 for growth
         * @param ch character to fill
         */
        constexpr void push_back(CharT ch)
        {
            auto size = size_();
            if (capacity() == size)
            {
                // long string
                if (is_long_())
                {
                    auto ls = stor_.ls_;
                    allocate_plus_one_(size * 2);
                    fill_(ls.begin_, ls.end_);
                    dealloc_(ls);
                    *(ls.end_ - 1) = ch;
                }
                else
                {
                    auto ss = stor_.ss_;
                    auto data = ss.data();
                    allocate_plus_one_(size * 2);
                    fill_(ss, ss + size);
                    *(ss + size) = ch;
                }
            }
            resize_(size + 1);
        }

        // ********************************* begin swap ******************************

        constexpr void swap(basic_string& other) noexcept
        {
            assert(("string is null", !is_null_()));
            auto&& self = *this;
            std::ranges::swap(self.allocator_, other.allocator_);
            std::ranges::swap(self.stor_, other.stor_);
            std::ranges::swap(self.size_flag_, other.size_flag_);
        }

        friend void swap(basic_string& self, basic_string& other) noexcept
        {
            self.swap(other);
        }

        // ********************************* begin constructor ******************************

        constexpr basic_string() noexcept = default;

        constexpr basic_string(null_t) noexcept
        {
            size_flag_ = -2;
        }

        constexpr basic_string(size_type n, CharT ch)
        {
            allocate_plus_one_(n);
            auto begin = begin_();
            auto end = begin + n;
            std::fill(begin, end, ch);
            resize_(n);
        }

        constexpr basic_string(const basic_string& other, size_type pos, size_type count) : allocator_(other.allocator_)
        {
            assert(("pos too long", other.size_() > pos));
            auto size = other.size_();
            count = (size < pos + count) ? size - pos : count;
            allocate_plus_one_(count);
            auto&& begin = other.begin_();
            fill_(begin + pos, begin + pos + count);
            resize_(count);
        }

        constexpr basic_string(const basic_string& other, size_type pos) : basic_string(other, pos, other.size_() - (pos + 1))
        {
        }

        constexpr basic_string(basic_string&& other, size_type pos, size_type count)
        {
            auto size = other.size_();
            count = pos + count > size ? count : size - pos;
            assert(("count too long", other.size_() > count));

            if (pos == 0)
            {
                auto start = other.begin_();
                auto begin = start + pos;
                auto end = other.begin_() + pos + count;
                std::copy(begin, end, start);
                other.resize_(count);
            }

            other.swap(*this);
        }

        constexpr basic_string(basic_string&& other, size_type pos) : basic_string(std::move(other), pos, other.size_() - pos)
        {
        }

        constexpr basic_string(const CharT* s, size_type count)
        {
            allocate_plus_one_(count);
            fill_(s, s + count);
            resize_(count);
        }

        constexpr basic_string(const CharT* s) : basic_string(s, c_style_string_length_(s))
        {
        }

        template <class InputIt>
            requires std::input_iterator<InputIt>
        constexpr basic_string(InputIt first, InputIt last)
        {
            if constexpr (std::forward_iterator<InputIt>)
            {
                auto length = std::ranges::distance(first, last);
                allocate_plus_one_(length);
                std::ranges::copy(first, last, begin_());
                resize_(length);
            }
            else
            {
                for (; first != last; ++first)
                    push_back(*first);
            }
        }

        constexpr basic_string(const basic_string& other) : allocator_(other.allocator_)
        {
            auto size = other.size_();
            allocate_plus_one_(size);
            fill_(other.begin_(), other.end_());
            resize_(size);
        }

        constexpr basic_string(basic_string&& other) noexcept
        {
            other.swap(*this);
        }

        constexpr basic_string(std::initializer_list<CharT> ilist) : basic_string(std::data(ilist), ilist.size())
        {
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string(const StringViewLike& t) : basic_string(std::basic_string_view<CharT, Traits>{ t }.data(), std::basic_string_view<CharT, Traits>{ t }.size())
		{
		}

        // clang-format on

        template <class StringViewLike>
            requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>>
        constexpr basic_string(const StringViewLike& t, size_type pos, size_type n) : basic_string(assert(("pos + n larger than size", std::basic_string_view<CharT, Traits>{ t }.size() >= pos + n)), std::basic_string_view<CharT, Traits>{ t }.data() + pos, n)
        {
        }

#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
        // tagged constructors to construct from container compatible range
        template <class R>
            requires std::ranges::range<R> && requires {
                typename R::value_type;
                requires std::same_as<typename R::value_type, CharT>;
            }
        constexpr basic_string(std::from_range_t, R&& rg)
        {
            if constexpr (std::ranges::sized_range<R>)
            {
                auto size = std::ranges::size(rg);
                allocate_plus_one_(size);
                std::ranges::copy(std::ranges::begin(rg), std::ranges::end(rg), begin_());
                resize_(size);
            }
            else
            {
                for (auto first = std::ranges::begin(rg), last = std::ranges::end(rg); first != last; ++first)
                    push_back(*first);
            }
        }
#endif // __cpp_lib_containers_ranges

        // ********************************* begin append ******************************

        /**
         * @brief this version provide for InputIt version of assign, other version of append and operator+=
         */
        template <class InputIt>
            requires std::input_iterator<InputIt>
        constexpr basic_string& append(InputIt first, InputIt last)
        {
            if constexpr (std::forward_iterator<InputIt>)
            {
                auto size = size_();
                auto length = std::ranges::distance(first, last);
                auto new_size = size + length;
                reserve(size * 2 > static_cast<size_type>(new_size) ? size * 2 : new_size);
                std::ranges::copy(first, last, begin_() + size);
                resize_(new_size);
            }
            else
            {
                for (; first != last; ++first)
                    push_back(*first);
            }
            return *this;
        }

        // ********************************* begin assign ******************************

        constexpr basic_string& assign(size_type count, CharT ch)
        {
            reserve(count);
            auto begin = begin_();
            auto end = begin + count;
            std::fill(begin, end, ch);
            resize_(count);
        }

        constexpr basic_string& assign(const basic_string& str)
        {
            assign_(str.begin_(), str.end_());
            return *this;
        }

        constexpr basic_string& assign(const basic_string& str, size_type pos, size_type count = npos)
        {
            auto size = str.size_();
            assign_(str.begin_() + pos, str.begin_() + pos + count);
            return *this;
        }

        constexpr basic_string& assign(basic_string&& str) noexcept
        {
            str.swap(*this);
            return *this;
        }

        constexpr basic_string& assign(const CharT* s, size_type count)
        {
            assign_(s, s + count);
            return *this;
        }

        constexpr basic_string& assign(const CharT* s)
        {
            assign_(s, s + c_style_string_length_(s));
            return *this;
        }

        template <class InputIt>
            requires std::input_iterator<InputIt>
        constexpr basic_string& assign(InputIt first, InputIt last)
        {
            resize_(0);
            return append(first, last);
        }

        constexpr basic_string& assign(std::initializer_list<CharT> ilist)
        {
            auto data = std::data(ilist);
            assign_(data, data + ilist.size());
            return *this;
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		basic_string& assign(const StringViewLike& t)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			auto data = sv.data();
			assign_(data, data + sv.size());
			return *this;
		}

		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& assign(const StringViewLike& t, size_type pos, size_type count = npos)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			auto size = sv.size();
			if (pos > size)
				throw std::out_of_range{ {} };
			count = (size < pos + count || count == npos) ? size - pos : count;
			auto data = sv.data();
			assign_(data + pos, data + pos + count);
			return *this;
		}

        // clang-format on

        // ********************************* begin operator= ******************************

        basic_string(std::nullptr_t) = delete;
        constexpr basic_string& operator=(std::nullptr_t) = delete;

        constexpr basic_string& operator=(basic_string&& other) noexcept
        {
            other.swap(*this);
            return *this;
        }

        constexpr basic_string& operator=(const basic_string& str)
        {
            assign_(str.begin_(), str.end_());
            return *this;
        }

        constexpr basic_string& operator=(const CharT* s)
        {
            return assign(s);
        }

        constexpr basic_string& operator=(CharT ch)
        {
            resize_(1);
            (*begin_()) = ch;
            return *this;
        }

        constexpr basic_string& operator=(std::initializer_list<CharT> ilist)
        {
            return assign(ilist);
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& operator=(const StringViewLike& t)
		{
			return assign(t);
		}

        // clang-format on

        // ********************************* begin compare ******************************

        friend constexpr std::strong_ordering operator<=>(basic_string const& lhs, basic_string const& rhs)
        {
            if (lhs.size_() > rhs.size_())
                return std::strong_ordering::greater;
            else if (lhs.size_() < rhs.size_())
                return std::strong_ordering::less;
            for (auto begin = lhs.begin_(), end = lhs.end_(), start = rhs.begin_(); begin != end; ++begin, ++start)
            {
                if (*begin > *start)
                    return std::strong_ordering::greater;
                else if (*begin < *start)
                    return std::strong_ordering::less;
            }
            return std::strong_ordering::equal;
        }

        friend constexpr bool operator==(basic_string const& lhs, basic_string const& rhs)
        {
            if (lhs.size_() != rhs.size_())
                return false;
            for (auto begin = lhs.begin_(), end = lhs.end_(), start = rhs.begin_(); begin != end; ++begin, ++start)
            {
                if (*begin != *start)
                    return false;
            }
            return true;
        }

        // ********************************* begin append ******************************

        constexpr basic_string& append(const CharT* s, size_type count)
        {
            return append(s, s + count);
        }

        constexpr basic_string& append(const basic_string& str)
        {
            return append(str.data(), str.size_());
        }

        constexpr basic_string& append(const basic_string& str, size_type pos, size_type count = npos)
        {
            auto size = str.size_();
            if (pos > size)
                throw std::out_of_range{ {} };
            count = (size < pos + count || count == npos) ? size - pos : count;
            return append(str.begin_() + pos, count);
        }

        constexpr basic_string& append(const CharT* s)
        {
            return append(s, c_style_string_length_(s));
        }

        constexpr basic_string& append(std::initializer_list<CharT> ilist)
        {
            return append(std::data(ilist), ilist.size());
        }

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& append(const StringViewLike& t)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			return append(sv.data(), sv.size());
		}

		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& append(const StringViewLike& t, size_type pos, size_type count = npos)
		{
			std::basic_string_view<CharT, Traits> sv = t;
			auto size = sv.size();
			if (pos > size)
				throw std::out_of_range{ {} };
			count = (size < pos + count || count == npos) ? size - pos : count;
			return append(sv.data() + pos, count);
		}

        // clang-format on

        // ********************************* begin operator+= ******************************

        // clang-format off
		template <class StringViewLike>
			requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>> && (!std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& operator+=(const StringViewLike& t)
		{
			return append(t);
		}

        // clang-format on

        constexpr basic_string& operator+=(const basic_string& str)
        {
            return append(str.begin_(), str.end_());
        }

        constexpr basic_string& operator+=(CharT ch)
        {
            push_back(ch);
            return *this;
        }

        constexpr basic_string& operator+=(const CharT* s)
        {
            return append(s, c_style_string_length_(s));
        }

        constexpr basic_string& operator+=(std::initializer_list<CharT> ilist)
        {
            auto data = std::data(ilist);
            return append(data, ilist.size());
        }

        // ********************************* begin search ******************************

    private:
        constexpr bool static equal_(CharT const* begin, CharT const* end, CharT const* first, CharT const* last) noexcept
        {
            if (last - first != end - begin)
                return false;
            for (; begin != end; ++begin, ++first)
            {
                if (*first != *begin)
                {
                    return false;
                }
            }
            return true;
        }

    public:
        constexpr bool starts_with(std::basic_string_view<CharT, Traits> sv) const noexcept
        {
            auto size = sv.size();
            auto data = sv.data();
            auto begin = begin_();
            if (size > size_())
                return false;
            return equal_(data, data + size, begin, begin + size);
        }

        constexpr bool starts_with(CharT ch) const noexcept
        {
            if (empty() || *begin_() != ch)
                return false;
            return true;
        }

        constexpr bool starts_with(const CharT* s) const
        {
            auto length = c_style_string_length_(s);
            auto begin = begin_();
            if (length > size_())
                return false;
            return equal_(s, s + length, begin, begin + length);
        }

        constexpr bool ends_with(std::basic_string_view<CharT, Traits> sv) const noexcept
        {
            auto size = sv.size();
            auto data = sv.data();
            auto end = end_();
            if (size > size_())
                return false;
            return equal_(data, data + size, end - size, end);
        }

        constexpr bool ends_with(CharT ch) const noexcept
        {
            if (empty() || *end_() != ch)
                return false;
            return true;
        }

        constexpr bool ends_with(CharT const* s) const
        {
            auto length = c_style_string_length_(s);
            auto end = end_();
            if (length > size_())
                return false;
            return equal_(s, s + length, end - length, end);
        }

#if defined(__cpp_lib_string_contains) && (__cpp_lib_string_contains >= 202011L)
        constexpr bool contains(std::basic_string_view<CharT, Traits> sv) const noexcept
        {
            auto size = sv.size();
            auto begin = begin_();
            auto data = sv.data();
            if (size_() < size)
                return false;
            // opti for starts_with
            if (equal_(begin, begin + size, data, data + size))
                return true;
            return std::basic_string_view<CharT, Traits>{ begin + 1, end_() }.contains(sv);
        }

        constexpr bool contains(CharT ch) const noexcept
        {
            if (empty())
                return false;
            for (auto begin = begin_(), end = end_(); begin != end; ++begin)
            {
                if (*begin == ch)
                    return true;
            }
            return false;
        }

        constexpr bool contains(const CharT* s) const noexcept
        {
            return std::basic_string_view<CharT, Traits>{ begin_(), end_() }.contains(std::basic_string_view<CharT, Traits>{ s, s + c_style_string_length_(s) });
        }
#endif
        // ********************************* begin insert ******************************

        constexpr void insert(CharT* pos, CharT const* first, CharT const* last)
        {
            assert(("pos isn't in this string", begin_() >= pos));
            auto size = size_();
            auto length = last - first;
            auto end = end_();
            if (pos > end)
                throw std::out_of_range{ {} };
            reserve(size + length);
            std::copy_backward(pos, end, end + length);
            std::copy(pos, pos + length, first);
            resize_(size + length);
        }

        constexpr basic_string& insert(size_type index, size_type count, CharT ch)
        {
            auto size = size_();
            if (index > size)
                throw std::out_of_range{ {} };
            reserve(size + count);
            auto pos = begin_() + index;
            auto end = pos + size - index;
            auto last = end + count;
            std::copy_backward(pos, end, last);
            std::fill(pos, pos + count, ch);
            resize_(size + count);
            return *this;
        }

        constexpr basic_string& insert(size_type index, const CharT* s, size_type count)
        {
            auto start = begin_() + index;
            insert_(start, s, s + count);
            return *this;
        }

        constexpr basic_string& insert(size_type index, const CharT* s)
        {
            return insert(index, s, c_style_string_length_(s));
        }

        constexpr basic_string& insert(size_type index, const basic_string& str)
        {
            insert_(begin_() + index, str.begin_(), str.end_());
            return *this;
        }

        constexpr basic_string& insert(size_type index, const basic_string& str, size_type s_index, size_type count = npos)
        {
            auto s_size = str.size_();
            if (s_index > s_size)
                throw std::out_of_range{ {} };
            if (count == npos)
                count = s_size;
            count = count + s_index > s_size ? s_size : count;
            auto s_start = str.begin_() + index;
            insert_(begin_() + index, s_start, s_start + count);
            return *this;
        }

        constexpr iterator insert(const_iterator pos, CharT ch)
        {
            auto size = size_();
            if (pos > size)
                throw std::out_of_range{ {} };
            reserve(size + 1);
            auto start = pos.base().current_;
            auto end = end_();
            std::copy_backward(pos, end, end + 1);
            *pos = ch;
            resize_(size + 1);
#ifndef NDEBUG
            return { start, this };
#else
            return { start };
#endif
        }

        constexpr iterator insert(const_iterator pos, size_type count, CharT ch)
        {
            auto start = pos.base().current_;
            auto index = start - begin_();
            insert(index, count, ch);
#ifndef NDEBUG
            return { start, this };
#else
            return { start };
#endif
        }

        template <class InputIt> constexpr iterator insert(const_iterator pos, InputIt first, InputIt last);

        constexpr iterator insert(const_iterator pos, std::initializer_list<CharT> ilist)
        {
            auto i_data = ilist.data();
            auto start = pos.base().current_;
            insert_(start, i_data, i_data + ilist.size());
#ifndef NDEBUG
            return { start, this };
#else
            return { start };
#endif
        }

        // clang-format off
        template <class StringViewLike>
            requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>>&& std::is_convertible_v<const StringViewLike&, const CharT*>
        constexpr basic_string& insert(size_type pos, const StringViewLike& t)
        {
            std::basic_string_view<CharT, Traits> sv = t;
            auto t_data = t.data();
            insert_(begin_() + pos, t_data,t_data + t.size());
            return *this;
        }

        template <class StringViewLike>
            requires std::is_convertible_v<const StringViewLike&, std::basic_string_view<CharT, Traits>>&& std::is_convertible_v<const StringViewLike&, const CharT*>
        constexpr basic_string& insert(size_type pos, const StringViewLike& t, size_type t_index, size_type count = npos)
        {
            std::basic_string_view<CharT, Traits> sv = t;
            auto sv_size = sv.size();
            if (t_index > sv_size)
                throw std::out_of_range{ {} };
            auto sv_data = sv.data();
            if (count == npos)
                count = sv_size;
            count = count + t_index > sv_size ? sv_size : count;
            insert_(begin_() + pos, sv_data + t_index,sv_data + t_index + count);
            return *this;
        }

        // clang-format on

        // ********************************* begin erase ******************************

    private:

        constexpr void erase_(CharT* first, CharT const* last) noexcept
        {
            assert(("first or last is not in this string", first >= begin_() && last <= end_()));
            std::copy(last, end_(), first);
            resize_(size() - (last - first));
        }

    public:

        constexpr basic_string& erase(size_type index = 0, size_type count = npos) noexcept
        {
            auto size = size_();
            if (index > size)
                throw std::out_of_range{ {} };
            count = std::min(npos, std::min(size_() - index, count));
            auto begin = begin_();
            erase_(begin + index, begin + index + count);
            return *this;
        }

        constexpr iterator erase(const_iterator position) noexcept
        {
            auto start = position.base().current_;
            erase_(start, start + 1);
#ifndef NDEBUG
            return { start, this };
#else
            return { start };
#endif
        }

        constexpr iterator erase(const_iterator first, const_iterator last) noexcept
        {
            auto start = first.base().current_;
            auto end = last.base().current_;
            erase_(start, end);
#ifndef NDEBUG
            return { end, this };
#else
            return { end };
#endif
        }

        // ********************************* begin destructor ******************************

        constexpr ~basic_string()
        {
            if (is_long_())
            {
                dealloc_(stor_.ls_);
            }
        }
    };

    template <character CharT, class Traits, class Allocator> template <class InputIt> constexpr typename basic_string<CharT, Traits, Allocator>::iterator basic_string<CharT, Traits, Allocator>::insert(const_iterator pos, InputIt first, InputIt last)
    {
        auto size = size_();
        auto start = pos.base().current_;
        assert(("pos isn't in this string", start >= begin_()));
        auto end = end_();
        if (start > end)
            throw std::out_of_range{ {} };
        if constexpr (std::forward_iterator<InputIt>)
        {
            auto length = std::distance(first, last);
            reserve(size + length);
            std::copy_backward(start, end, end + length);
            std::ranges::copy(first, last, start);
            resize_(size + length);
        }
        else
        {
            basic_string temp{ start, end };
            resize_(pos - begin_());
            for (; first != last; ++first)
                push_back(*first);
            append_(temp.begin_(), temp.end_());
        }
        return *this;
    }
}
