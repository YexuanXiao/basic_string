#pragma once

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
#include <cwchar>

#if defined(__cpp_if_consteval) && (__cpp_if_consteval >= 202106L)
#define BIZWEN_CONSTEVAL consteval
#else
#define BIZWEN_CONSTEVAL (::std::is_constant_evaluated())
#endif

namespace bizwen
{
	template <typename T>
	struct is_character: ::std::bool_constant<::std::is_same_v<char, ::std::remove_cv_t<T>> || ::std::is_same_v<signed char, ::std::remove_cv_t<T>> || ::std::is_same_v<unsigned char, ::std::remove_cv_t<T>> || ::std::is_same_v<wchar_t, ::std::remove_cv_t<T>> || ::std::is_same_v<char8_t, ::std::remove_cv_t<T>> || ::std::is_same_v<char16_t, ::std::remove_cv_t<T>> || ::std::is_same_v<char32_t, ::std::remove_cv_t<T>>>
	{
	};

	template <typename T>
	constexpr bool is_character_v = is_character<T>::value;

	template <typename T>
	concept character = is_character_v<T>;

	template <character CharT, class Traits = ::std::char_traits<CharT>, class Allocator = ::std::allocator<CharT>>
	class alignas(alignof(int*)) basic_string
	{
	private:
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
		 * @brief short_string_max_ is the max length of short string
		 */
		static inline constexpr ::std::size_t short_string_max_{ sizeof(CharT*) * 4 / sizeof(CharT) - 2 };

		/**
		 * @brief union storage long string and short string
		 */
#pragma pack(1)

		union storage_type_
		{
			::std::array<CharT, short_string_max_ + 1> ss_;
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
		 * @brief flag = MAX: long string, length of string is end - begin
		 */
		alignas(alignof(CharT)) unsigned char size_flag_{};

		using atraits_t_ = ::std::allocator_traits<Allocator>;

		static inline char exception_string_[] = "parameter is out of range, please check it.";

		constexpr bool is_long_() const noexcept
		{
			return size_flag_ == decltype(size_flag_)(-1);
		}

		constexpr bool is_short_() const noexcept
		{
			return size_flag_ != decltype(size_flag_)(-1);
		}

		constexpr bool is_empty_() const noexcept
		{
			return !size_flag_;
		}

		constexpr ::std::size_t size_() const noexcept
		{
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
		using size_type = typename ::std::allocator_traits<Allocator>::size_type;
		using difference_type = typename ::std::allocator_traits<Allocator>::difference_type;
		using reference = value_type&;
		using const_reference = value_type const&;
		using pointer = typename ::std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename ::std::allocator_traits<Allocator>::const_pointer;

		static inline constexpr size_type npos = -1;

		// ********************************* begin volume ******************************

		constexpr bool empty() const noexcept
		{
			return is_empty_();
		}

		constexpr ::std::size_t size() const noexcept
		{
			return size_();
		}

		constexpr ::std::size_t length() const noexcept
		{
			return size_();
		}

		/**
		 * @return size_type{ -1 } / sizeof(CharT) / 2
		 */
		constexpr size_type max_size() const noexcept
		{
			return size_type{ -1 } / sizeof(CharT) / 2;
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
		 * @brief shrink only occur on short string is enough to storage
		 */
		constexpr void shrink_to_fit() noexcept
		{
			if (size_() <= short_string_max_ && is_long_())
			{
				auto ls = stor_.ls_;

				if BIZWEN_CONSTEVAL
				{
					stor_.ss_ = decltype(stor_.ss_){};
					::std::copy(ls.begin_, ls.end_, stor_.ss_.data());
				}
				else
				{
					::std::memcpy(stor_.ss_.data(), ls.begin_, (ls.end_ - ls.begin_) * sizeof(CharT));
				}

				dealloc_(ls);
			}
		}

		// ********************************* begin element access ******************************

	private:
		/**
		 * @brief internal use
		 * @return a pointer to the first element
		 */
		constexpr CharT const* begin_() const noexcept
		{
			if (is_long_())
				return stor_.ls_.begin_;
			else
				return stor_.ss_.data();
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
		constexpr CharT const* end_() const noexcept
		{
			if (is_long_())
				return stor_.ls_.end_;
			else
				return stor_.ss_.data() + size_flag_;
		}

		/**
		 * @brief internal use
		 * @return a pointer to the next position of the last element
		 */
		constexpr CharT* end_() noexcept
		{
			return const_cast<CharT*>(const_cast<basic_string const&>(*this).end_());
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
				throw ::std::out_of_range{ exception_string_ };

			return *(begin_() + pos);
		}

		constexpr reference at(size_type pos)
		{
			return const_cast<CharT&>(const_cast<basic_string const&>(*this).at(pos));
		}

		constexpr const_reference operator[](size_type pos) const noexcept
		{
			assert(("pos >= size, please check the arg", pos < size_()));

			return *(begin_() + pos);
		}

		constexpr reference operator[](size_type pos) noexcept
		{
			return const_cast<CharT&>(const_cast<basic_string const&>(*this)[pos]);
		}

		constexpr const CharT& front() const noexcept
		{
			assert(("string is empty", !is_empty_()));

			return *begin_();
		}

		constexpr CharT& front()
		{
			return const_cast<CharT&>(const_cast<basic_string const&>(*this).front());
		}

		constexpr const CharT& back() const noexcept
		{
			assert(("string is empty", !is_empty_()));

			return *(end_() - 1);
		}

		constexpr CharT& back()
		{
			return const_cast<CharT&>(const_cast<basic_string const&>(*this).back());
		}

		constexpr operator ::std::basic_string_view<CharT, Traits>() const noexcept
		{
			return ::std::basic_string_view<CharT, Traits>(begin_(), end_());
		}

		// ********************************* begin iterator type ******************************

	private:
		struct iterator_type_
		{
			using difference_type = ::std::ptrdiff_t;
			using value_type = CharT;
			using pointer = ::std::add_pointer_t<value_type>;
			using reference = ::std::add_lvalue_reference_t<CharT>;
			using iterator_category = ::std::random_access_iterator_tag;
			using iterator_concept = ::std::contiguous_iterator_tag;

		private:
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

		public:
			iterator_type_() noexcept = default;
			iterator_type_(iterator_type_ const&) noexcept = default;
			iterator_type_(iterator_type_&&) noexcept = default;
			iterator_type_& operator=(iterator_type_ const&) & noexcept = default;
			iterator_type_& operator=(iterator_type_&&) & noexcept = default;

		private:
#ifndef NDEBUG
			constexpr iterator_type_(CharT* current, basic_string* target)
			    : current_(current)
			    , target_(target)
			{
			}
#else
			constexpr iterator_type_(CharT* current)
			    : current_(current)
			{
			}
#endif

		public:
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

			constexpr iterator_type_ operator++(int) & noexcept
			{
				iterator_type_ temp{ *this };
				++current_;
				check();
				return temp;
			}

			constexpr iterator_type_ operator--(int) & noexcept
			{
				iterator_type_ temp{ *this };
				--current_;
				check();
				return temp;
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

			friend constexpr ::std::strong_ordering operator<=>(iterator_type_ const&, iterator_type_ const&) noexcept = default;
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
			// strong exception safe grantee
			if (n <= short_string_max_ && !is_long_())
			{
				size_flag_ = static_cast<signed char>(n);
				return;
			}

			++n;

#if defined(__cpp_lib_allocate_at_least) && (__cpp_lib_allocate_at_least >= 202302L)
			auto [ptr, count] = atraits_t_::allocate_at_least(allocator_, n);
			stor_.ls_ = { ptr, nullptr, ptr + count };
#else
			auto ptr = atraits_t_::allocate(allocator_, n);
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
				// gcc thinks it's out of range,
				// so make gcc happy
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
				stor_.ss_[n] = CharT{};
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
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
			assert(("cannot storage string in current allocated storage", static_cast<size_type>(end - begin) <= capacity()));

			if BIZWEN_CONSTEVAL
			{
				::std::copy(begin, end, begin_());
			}
			else
			{
				::std::memmove(begin_(), begin, (end - begin) * sizeof(CharT));
			}
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
			if (!::std::is_constant_evaluated())
#endif
			{
				if constexpr (::std::is_same_v<char, CharT>)
				{
					return ::std::strlen(begin);
				}
				else if constexpr (::std::is_same_v<wchar_t, CharT>)
				{
					return ::std::wcslen(begin);
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
		constexpr void assign_(CharT const* first, CharT const* last)
		{
			auto size = last - first;

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

			fill_(first, last);
			resize_(size);
		}

		/**
		 * @brief insert characters to *this
		 * @brief this function can be called with any legal state
		 * @brief strong exception safety guarantee
		 */
		constexpr void insert_(size_type index, CharT const* first, CharT const* last)
		{
			auto size = size_();

			if (index > size)
				throw ::std::out_of_range{ exception_string_ };

			auto length = last - first;
			auto new_size = size + length;
			auto begin = begin_();
			auto end = begin + size;
			auto start = begin + index;

			// if start is not in range and capacity > length + size
			if ((start < first || start > last) && capacity() > new_size)
			{

				if BIZWEN_CONSTEVAL
				{
					::std::copy_backward(start, end, end + length);
				}
				else
				{
					::std::memmove(end, start, length * sizeof(CharT));
				}

				// re-cacl first and last, because range is in *this
				if (first >= start && last <= end)
				{
					first += index;
					last += index;
				}

				if BIZWEN_CONSTEVAL
				{
					::std::copy(first, last, start);
				}
				else
				{
					::std::memmove(start, first, length * sizeof(CharT));
				}
			}
			else
			{
				basic_string temp{};
				temp.allocate_plus_one_(new_size);
				auto temp_begin = temp.begin_();
				auto temp_start = temp_begin + index;
				auto temp_end = temp_begin + size;

				if BIZWEN_CONSTEVAL
				{
					::std::copy(begin, start, temp_begin);
					::std::copy(first, last, temp_start);
					::std::copy(start, end, temp_start + length);
				}
				else
				{
					::std::memcpy(temp_begin, begin, index * sizeof(CharT));
					::std::memcpy(temp_start, first, length * sizeof(CharT));
					::std::memcpy(temp_start + length, start, (end - start) * sizeof(CharT));
				}

				temp.swap(*this);
			}

			resize_(new_size);
		}

		constexpr void replace_(size_type pos, size_type count, CharT const* first2, CharT const* last2)
		{
			auto size = size_();

			if (pos > size)
				throw ::std::out_of_range{ exception_string_ };

			auto begin = begin_();
			auto first1 = begin + pos;
			auto last1 = begin + ::std::min(pos + count, size);
			auto length1 = last1 - first1;
			auto length2 = last2 - first2;
			auto new_size = size + length1 - length2;
			auto end = begin + size;

			if (!(last1 < first2 || last2 < first1) && new_size <= capacity())
			{
				auto diff = length1 - length2;

				if BIZWEN_CONSTEVAL
				{
					if (diff > 0)
						::std::copy(last1, end, last1 - diff);
					else if (diff < 0)
						::std::copy_backward(last1, end, end - diff);
				}
				else
				{
					if (diff > 0)
						::std::memmove(last1 + diff, last1, diff * sizeof(CharT));
					else if (diff < 0)
						::std::memmove(last1 - diff, last1, -diff * sizeof(CharT));
				}

				if (first2 >= last1 && last2 <= end)
				{
					first2 += diff;
					last2 += diff;
				}

				if BIZWEN_CONSTEVAL
				{
					::std::copy(first2, last2, first1);
				}
				else
				{
					::std::memmove(first1, first2, length2 * sizeof(CharT));
				}
			}
			else
			{
				basic_string temp{};
				temp.allocate_plus_one_(new_size);
				auto temp_begin = temp.begin_();
				auto temp_start = temp.begin_() + (first1 - begin);

				if BIZWEN_CONSTEVAL
				{
					::std::copy(begin, first1, temp_begin);
					::std::copy(first2, last2, temp_start);
					::std::copy(last1, end, temp_start + length2);
				}
				else
				{
					::std::memcpy(begin, temp_begin, first1 - begin * sizeof(CharT));
					::std::memcpy(temp_start, first2, length2 * sizeof(CharT));
					::std::memcpy(temp_start + length2, last1, (end - last1) * sizeof(CharT));
				}

				temp.swap(*this);
			}

			resize_(new_size);
		}

	public:
		constexpr ~basic_string()
		{
			if (is_long_())
				dealloc_(stor_.ls_);
		}

		/**
		 * @brief reserve memory
		 * @brief strong exception safety guarantee
		 * @brief never shrink
		 * @param new_cap new capacity
		 */
		constexpr void reserve(size_type new_cap)
		{
			if (capacity() >= new_cap)
				return;

			if (is_long_())
			{
				auto ls = stor_.ls_;
				allocate_plus_one_(new_cap);
				fill_(ls.begin_, ls.end_);
				// even though I'm only subtracting two pointers without r/w
				// gcc still thinks it's use after free, so make gcc happy
				auto size = ls.end_ - ls.begin_;
				dealloc_(ls);
				// resize_(ls.end_ - ls.begin_);
				resize_(size);
			}
			else
			{
				auto ss = stor_.ss_;
				auto size = size_flag_;
				auto data = ss.data();
				allocate_plus_one_(new_cap);
				fill_(data, data + size);
				resize_(size);
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
			auto size = size_();

			if (count > size)
			{
				reserve(count);
				auto begin = begin_();
				::std::fill(begin + size, begin + count, ch);
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
		 * @brief use size * 1.5 for growth
		 * @param ch character to fill
		 */
		constexpr void push_back(CharT ch)
		{
			auto size = size_();

			if (capacity() == size)
				reserve(size * 2 - size / 2);

			*end_() = ch;
			resize_(size + 1);
		}

		// ********************************* begin swap ******************************

	private:
		constexpr void swap_without_ator(basic_string& other) noexcept
		{
			auto&& self = *this;
			::std::ranges::swap(self.stor_, other.stor_);
			::std::ranges::swap(self.size_flag_, other.size_flag_);
		}

	public:
		constexpr void swap(basic_string& other) noexcept
		{
			if constexpr (::std::allocator_traits<Allocator>::propagate_on_container_swap::value)
				::std::ranges::swap(other.allocator_, other.allocator_);
			else
				assert(other.allocator_ == allocator_);

			other.swap_without_ator(*this);
		}

		friend void swap(basic_string& self, basic_string& other) noexcept
		{
			self.swap(other);
		}

		// ********************************* begin constructor ******************************
	public:
		constexpr basic_string() noexcept = default;

		constexpr basic_string(allocator_type const& a) noexcept
		    : allocator_(a)
		{
		}

		constexpr basic_string(size_type n, CharT ch, allocator_type const& a = allocator_type())
		    : allocator_(a)
		{
			allocate_plus_one_(n);
			auto begin = begin_();
			auto end = begin + n;
			::std::fill(begin, end, ch);
			resize_(n);
		}

		constexpr basic_string(const basic_string& other, size_type pos, size_type count, allocator_type const& a = allocator_type())
		    : allocator_(a)
		{
			auto other_size = other.size_();

			if (pos > other_size)
				throw ::std::out_of_range{ exception_string_ };

			count = ::std::min(other_size - pos, count);
			allocate_plus_one_(count);
			auto start = other.begin_() + pos;
			fill_(start, start + count);
			resize_(count);
		}

		constexpr basic_string(const basic_string& other, size_type pos, allocator_type const& a = allocator_type())
		    : basic_string(other, pos, other.size_() - pos, a)
		{
		}

		constexpr basic_string(const CharT* s, size_type count, allocator_type const& a = allocator_type())
		    : allocator_(a)
		{
			allocate_plus_one_(count);
			fill_(s, s + count);
			resize_(count);
		}

		constexpr basic_string(const CharT* s, allocator_type const& a = allocator_type())
		    : basic_string(s, c_style_string_length_(s), a)
		{
		}

		template <class InputIt>
		    requires ::std::input_iterator<InputIt>
		constexpr basic_string(InputIt first, InputIt last)
		{
			if constexpr (::std::random_access_iterator<InputIt>)
			{
				auto length = ::std::ranges::distance(first, last);
				allocate_plus_one_(length);
				::std::ranges::copy(first, last, begin_());
				resize_(length);
			}
			else
			{
				for (; first != last; ++first)
					push_back(*first);
			}
		}

		constexpr basic_string(const basic_string& other)
		    : allocator_(::std::allocator_traits<Allocator>::select_on_container_copy_construction(other.allocator_))
		{
			auto other_size = other.size_();
			allocate_plus_one_(other_size);
			fill_(other.begin_(), other.end_());
			resize_(other_size);
		}

		constexpr basic_string(const basic_string& other, allocator_type const& a)
		    : allocator_(a)
		{
			auto other_size = other.size_();
			allocate_plus_one_(other_size);
			fill_(other.begin_(), other.end_());
			resize_(other_size);
		}

		constexpr basic_string(basic_string&& other, size_type pos, size_type count, allocator_type const& a = allocator_type())
		    : allocator_(a)
		{
			auto other_size = other.size_();

			if (pos > other_size)
				throw ::std::out_of_range{ exception_string_ };

			count = ::std::min(other_size - pos, count);

			if (pos != 0)
			{
				auto other_begin = other.begin_();
				auto start = other_begin + pos;
				auto last = start + count;

				if BIZWEN_CONSTEVAL
				{
					::std::copy(start, last, other_begin);
				}
				else
				{
					::std::memmove(other_begin, start, (last - start) * sizeof(CharT));
				}
			}

			other.resize_(count);
			other.swap(*this);
		}

		constexpr basic_string(basic_string&& other) noexcept
		{
			allocator_ = other.allocator_;
			other.swap_without_ator(*this);
		}

		constexpr basic_string(basic_string&& other, allocator_type const& a)
		    : allocator_(a)
		{
			if (other.allocator_ == a)
			{
				other.swap_without_ator(*this);
			}
			else
			{
				basic_string temp{ other.data(), other.size(), a };
				temp.swap(*this);
				other.swap(temp);
			}
		}

		constexpr basic_string(::std::initializer_list<CharT> ilist, allocator_type const& a = allocator_type())
		    : basic_string(::std::data(ilist), ilist.size(), a)
		{
		}

		// clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string(const StringViewLike& t, allocator_type const& a = allocator_type())
		     : basic_string(::std::basic_string_view<CharT, Traits>{ t }.data(), ::std::basic_string_view<CharT, Traits>{ t }.size(),a)
        {
        }

		// clang-format on

		template <class StringViewLike>
		    requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>>
		constexpr basic_string(const StringViewLike& t, size_type pos, size_type n, allocator_type const& a = allocator_type())
		    : allocator_(a)
		{
			::std::basic_string_view<CharT, Traits> sv = t;
			auto data = sv.data();
			auto sv_size = sv.size();

			if (pos > sv_size)
				throw ::std::out_of_range{ exception_string_ };

			n = ::std::min(sv_size - pos, n);
			allocate_plus_one_(n);
			fill_(data, data + n);
			resize_(n);
		}

#if defined(__cpp_lib_containers_ranges) && (__cpp_lib_containers_ranges >= 202202L)
		// tagged constructors to construct from container compatible range
		template <class R>
		    requires ::std::ranges::range<R> && requires {
			    typename R::value_type;
			    requires ::std::same_as<typename R::value_type, CharT>;
		    }
		constexpr basic_string(::std::from_range_t, R&& rg, allocator_type const& a = allocator_type())
		    : allocator_(a)
		{
			if constexpr (::std::ranges::sized_range<R>)
			{
				auto size = ::std::ranges::size(rg);
				allocate_plus_one_(size);
				::std::ranges::copy(::std::ranges::begin(rg), ::std::ranges::end(rg), begin_());
				resize_(size);
			}
			else
			{
				for (auto first = ::std::ranges::begin(rg), last = ::std::ranges::end(rg); first != last; ++first)
					push_back(*first);
			}
		}
#endif

		// ********************************* begin append ******************************

		/**
		 * @brief this version provide for InputIt version of assign, other version of append and operator+=
		 */
		template <class InputIt>
		    requires ::std::input_iterator<InputIt>
		constexpr basic_string& append(InputIt first, InputIt last)
		{
			if constexpr (::std::random_access_iterator<InputIt>)
			{
				auto size = size_();
				auto length = ::std::ranges::distance(first, last);
				auto new_size = size + length;
				reserve(::std::max(size * 2, new_size));
				::std::ranges::copy(first, last, begin_() + size);
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
			::std::fill(begin, end, ch);
			resize_(count);

			return *this;
		}

		constexpr basic_string& assign(const basic_string& str)
		{

			if (std::addressof(str) == this)
				return *this;

			if constexpr (::std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value)
			{
				if (allocator_ != str.allocator_)
				{
					basic_string temp{ allocator_ };
					temp.swap(*this);
					allocator_ = str.allocator_;
				}
			}

			assign_(str.begin_(), str.end_());

			return *this;
		}

		constexpr basic_string& assign(const basic_string& str, size_type pos, size_type count = npos)
		{
			auto str_size = str.size_();

			if (pos > str_size)
				throw ::std::out_of_range{ exception_string_ };

			count = ::std::min(npos, ::std::min(str_size - pos, count));
			auto str_begin = begin_();
			assign_(str_begin + pos, str_begin + pos + count);

			return *this;
		}

		constexpr basic_string& assign(basic_string&& str) noexcept
		{
			if constexpr (::std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value)
			{
				other.swap(*this);
			}
			else
			{
				if (allocator_ == other.allocator_)
					other.swap_without_ator(*this);
				else
					assign_(other.begin_(), other.end_());
			}

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
		    requires ::std::input_iterator<InputIt>
		constexpr basic_string& assign(InputIt first, InputIt last)
		{
			resize_(0);

			return append(first, last);
		}

		constexpr basic_string& assign(::std::initializer_list<CharT> ilist)
		{
			auto data = ::std::data(ilist);
			assign_(data, data + ilist.size());

			return *this;
		}

		// clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        basic_string& assign(const StringViewLike& t)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            auto data = sv.data();
            assign_(data, data + sv.size());

            return *this;
        }

        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& assign(const StringViewLike& t, size_type pos, size_type count = npos)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            auto sv_size = sv.size();

            if (pos > sv_size)
                throw ::std::out_of_range{ exception_string_ };

            count = ::std::min(npos, ::std::min(sv_size - pos, count));
            auto data = sv.data();
            assign_(data + pos, data + pos + count);

            return *this;
        }

		// clang-format on

		// ********************************* begin operator= ******************************

		basic_string(::std::nullptr_t) = delete;
		constexpr basic_string& operator=(::std::nullptr_t) = delete;

		constexpr basic_string& operator=(basic_string&& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<Allocator>::is_always_equal::value)
		{
			return assign(std::move(other));
		}

		constexpr basic_string& operator=(const basic_string& str)
		{
			return assign(str);
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

		constexpr basic_string& operator=(::std::initializer_list<CharT> ilist)
		{
			return assign(ilist);
		}

		// clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& operator=(const StringViewLike& t)
        {
            return assign(t);
        }

		// clang-format on

		// ********************************* begin compare ******************************

		friend constexpr ::std::strong_ordering operator<=>(basic_string const& lhs, basic_string const& rhs) noexcept
		{
			auto lsize = lhs.size_();
			auto rsize = rhs.size_();

			if BIZWEN_CONSTEVAL
			{
				for (auto begin = lhs.begin_(), end = begin + ::std::min(lsize, rsize), start = rhs.begin_(); begin != end; ++begin, ++start)
				{
					if (*begin > *start)
						return ::std::strong_ordering::greater;
					else if (*begin < *start)
						return ::std::strong_ordering::less;
				}

				if (lsize > rsize)
					return ::std::strong_ordering::greater;
				else if (lsize < rsize)
					return ::std::strong_ordering::less;

				return ::std::strong_ordering::equal;
			}
			else
			{
				auto res = basic_string::traits_type::compare(lhs.begin_(), rhs.begin_(), ::std::min(rsize, lsize));
				if (res > 0)
					return ::std::strong_ordering::greater;
				else if (res < 0)
					return ::std::strong_ordering::less;

				if (lsize > rsize)
					return ::std::strong_ordering::greater;
				else if (lsize < rsize)
					return ::std::strong_ordering::less;
				else
					return ::std::strong_ordering::equal;
			}
		}

		friend constexpr ::std::strong_ordering operator<=>(basic_string const& lhs, CharT const* rhs) noexcept
		{
			auto start = rhs;
			auto rsize = basic_string::c_style_string_length_(start);
			auto lsize = lhs.size_();

			if BIZWEN_CONSTEVAL
			{
				for (auto begin = lhs.begin_(), end = begin + ::std::min(lsize, rsize); begin != end; ++begin, ++start)
				{
					if (*begin > *start)
						return ::std::strong_ordering::greater;
					else if (*begin < *start)
						return ::std::strong_ordering::less;
				}

				if (lsize > rsize)
					return ::std::strong_ordering::greater;
				else if (lsize < rsize)
					return ::std::strong_ordering::less;
				else
					return ::std::strong_ordering::equal;
			}
			else
			{
				auto res = basic_string::traits_type::compare(lhs.begin_(), start, ::std::min(rsize, lsize));
				if (res > 0)
					return ::std::strong_ordering::greater;
				else if (res < 0)
					return ::std::strong_ordering::less;

				if (lsize > rsize)
					return ::std::strong_ordering::greater;
				else if (lsize < rsize)
					return ::std::strong_ordering::less;
				else
					return ::std::strong_ordering::equal;
			}
		}

		friend constexpr bool operator==(basic_string const& lhs, basic_string const& rhs) noexcept
		{
			auto lsize = lhs.size_();
			auto rsize = rhs.size_();

			if (lsize != rsize)
				return false;

			if BIZWEN_CONSTEVAL
			{
				for (auto begin = lhs.begin_(), end = begin + ::std::min(lsize, rsize), start = rhs.begin_(); begin != end; ++begin, ++start)
				{
					if (*begin != *start)
						return false;
				}

				return true;
			}
			else
			{
				return !basic_string::traits_type::compare(lhs.begin_(), rhs.begin_(), ::std::min(rsize, lsize));
			}
		}

		friend constexpr bool operator==(basic_string const& lhs, CharT const* rhs) noexcept
		{
			auto start = rhs;
			auto rsize = basic_string::c_style_string_length_(start);
			auto lsize = lhs.size_();

			if (lsize != rsize)
				return false;

			if BIZWEN_CONSTEVAL
			{
				for (auto begin = lhs.begin_(), end = begin + ::std::min(lsize, rsize); begin != end; ++begin, ++start)
				{
					if (*begin != *start)
						return false;
				}

				return true;
			}
			else
			{
				return !basic_string::traits_type::compare(lhs.begin_(), start, ::std::min(rsize, lsize));
			}
		}

		// ********************************* begin append ******************************

	private:
		constexpr void append_(CharT const* first, CharT const* last)
		{
			auto length = last - first;
			auto size = size_();
			reserve(size + length);
			auto end = begin_() + size;

			if BIZWEN_CONSTEVAL
			{
				::std::copy(first, last, end);
			}
			else
			{
				::std::memcpy(end, first, (last - first) * sizeof(CharT));
			}

			resize_(size + length);
		}

	public:
		constexpr basic_string& append(size_type count, CharT ch)
		{
			auto size = size_();
			reserve(size + count);
			auto end = begin_() + size;
			::std::fill(end, end + count, ch);
			resize_(size + count);

			return *this;
		}

		constexpr basic_string& append(const CharT* s, size_type count)
		{
			append_(s, s + count);

			return *this;
		}

		constexpr basic_string& append(const basic_string& str)
		{
			auto begin = str.begin_();
			append_(begin, begin + str.size_());

			return *this;
		}

		constexpr basic_string& append(const basic_string& str, size_type pos, size_type count = npos)
		{
			auto str_size = str.size_();

			if (pos > str_size)
				throw ::std::out_of_range{ exception_string_ };

			count = ::std::min(npos, ::std::min(str_size - pos, count));

			return append(str.begin_() + pos, count);
		}

		constexpr basic_string& append(const CharT* s)
		{
			append_(s, s + c_style_string_length_(s));

			return *this;
		}

		constexpr basic_string& append(::std::initializer_list<CharT> ilist)
		{
			auto data = ::std::data(ilist);
			append_(data, data + ilist.size());

			return *this;
		}

		// clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& append(const StringViewLike& t)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            auto data = sv.data();
            append_(data, data + sv.size());

            return *this;
        }

        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& append(const StringViewLike& t, size_type pos, size_type count = npos)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            auto sv_size = sv.size();

            if (pos > sv_size)
                throw ::std::out_of_range{ exception_string_ };

            count = ::std::min(npos, ::std::min(sv_size - count, count));

            return append(sv.data() + pos, count);
        }

		// clang-format on

		// ********************************* begin operator+= ******************************

		// clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& operator+=(const StringViewLike& t)
        {
            return append(t);
        }

		// clang-format on

		constexpr basic_string& operator+=(const basic_string& str)
		{
			return append(str);
		}

		constexpr basic_string& operator+=(CharT ch)
		{
			return push_back(ch);
		}

		constexpr basic_string& operator+=(const CharT* s)
		{
			return append(s);
		}

		constexpr basic_string& operator+=(::std::initializer_list<CharT> ilist)
		{
			return append(ilist);
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
		constexpr bool starts_with(::std::basic_string_view<CharT, Traits> sv) const noexcept
		{
			auto sv_size = sv.size();
			auto data = sv.data();
			auto begin = begin_();

			if (sv_size > size_())
				return false;

			return equal_(data, data + sv_size, begin, begin + sv_size);
		}

		constexpr bool starts_with(CharT ch) const noexcept
		{
			return *begin_() == ch;
		}

		constexpr bool starts_with(const CharT* s) const
		{
			auto length = c_style_string_length_(s);
			auto begin = begin_();

			if (length > size_())
				return false;

			return equal_(s, s + length, begin, begin + length);
		}

		constexpr bool ends_with(::std::basic_string_view<CharT, Traits> sv) const noexcept
		{
			auto sv_size = sv.size();
			auto sv_data = sv.data();
			auto end = end_();

			if (sv_size > size_())
				return false;

			return equal_(sv_data, sv_data + sv_size, end - sv_size, end);
		}

		constexpr bool ends_with(CharT ch) const noexcept
		{
			return *(end_() - 1) == ch;
		}

		constexpr bool ends_with(CharT const* s) const
		{
			auto length = c_style_string_length_(s);

			if (length > size_())
				return false;

			auto end = end_();

			return equal_(s, s + length, end - length, end);
		}

#if defined(__cpp_lib_string_contains) && (__cpp_lib_string_contains >= 202011L)
		constexpr bool contains(::std::basic_string_view<CharT, Traits> sv) const noexcept
		{
			auto size = sv.size();
			auto begin = begin_();
			auto data = sv.data();

			if (size_() < size)
				return false;

			// opti for starts_with
			if (equal_(begin, begin + size, data, data + size))
				return true;

			return ::std::basic_string_view<CharT, Traits>{ begin + 1, begin + size }.contains(sv);
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

		constexpr bool contains(const CharT* s) const noexcept
		{
			return ::std::basic_string_view<CharT, Traits>{ begin_(), end_() }.contains(::std::basic_string_view<CharT, Traits>{ s, s + c_style_string_length_(s) });
		}
#endif

		// ********************************* begin insert ******************************

		constexpr basic_string& insert(size_type index, size_type count, CharT ch)
		{
			auto size = size_();

			if (index > size)
				throw ::std::out_of_range{ exception_string_ };

			reserve(size + count);
			auto start = begin_() + index;
			auto end = start + size - index;
			auto last = end + count;

			if BIZWEN_CONSTEVAL
			{
				::std::copy_backward(start, end, last);
				::std::fill(start, start + count, ch);
			}
			else
			{
				::std::memmove(end, start, count * sizeof(CharT));
				::std::fill(start, start + count, ch);
			}

			resize_(size + count);

			return *this;
		}

		constexpr basic_string& insert(size_type index, const CharT* s, size_type count)
		{
			insert_(index, s, s + count);

			return *this;
		}

		constexpr basic_string& insert(size_type index, const CharT* s)
		{
			insert_(index, s, s + c_style_string_length_(s));

			return *this;
		}

		constexpr basic_string& insert(size_type index, const basic_string& str)
		{
			insert_(index, str.begin_(), str.end_());

			return *this;
		}

		constexpr basic_string& insert(size_type index, const basic_string& str, size_type s_index, size_type count = npos)
		{
			auto s_size = str.size_();

			if (s_index > s_size)
				throw ::std::out_of_range{ exception_string_ };

			count = ::std::min(npos, ::std::min(s_size - s_index, count));
			auto s_start = str.begin_() + s_index;
			insert_(index, s_start, s_start + count);

			return *this;
		}

		constexpr iterator insert(const_iterator pos, CharT ch)
		{
			auto size = size_();
			auto start = pos.base().current_;
			auto end = end_();
			auto index = start - begin_();
			reserve(size + 1);
			auto begin = begin_();
			end = begin + size;
			start = begin + index;

			if BIZWEN_CONSTEVAL
			{
				::std::copy_backward(start, end, end + 1);
			}
			else
			{
				::std::memmove(end, start, sizeof(CharT));
			}

			*start = ch;
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

		/**
		 * @brief this function use a temp basic_string, so it can't decl in class decl
		 */
		template <class InputIt>
		    requires ::std::input_iterator<InputIt>
		constexpr iterator insert(const_iterator pos, InputIt first, InputIt last)
		{
			assert(("pos isn't in this string", pos.base().current_ >= begin_() && pos.base().current_ <= end_()));

			auto size = size_();
			auto start = pos.base().current_;
			auto end = end_();
			auto index = start - begin_();

			if constexpr (::std::random_access_iterator<InputIt>)
			{
				auto length = ::std::distance(first, last);
				reserve(size + length);
				auto begin = begin_();
				::std::copy_backward(begin + index, begin + size, begin + size + length);
				::std::ranges::copy(first, last, begin + index);
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

#ifndef NDEBUG
			return { start, this };
#else
			return { start };
#endif
		}

		constexpr iterator insert(const_iterator pos, ::std::initializer_list<CharT> ilist)
		{
			auto i_data = ::std::data(ilist);
			auto start = pos.base().current_;

			insert_(start - begin_(), i_data, i_data + ilist.size());

#ifndef NDEBUG
			return { start, this };
#else
			return { start };
#endif
		}

		// clang-format off
        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& insert(size_type pos, const StringViewLike& t)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            auto sv_data = sv.data();
            insert_(pos, sv_data, sv_data + sv.size());

            return *this;
        }

        template <class StringViewLike>
            requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
        constexpr basic_string& insert(size_type pos, const StringViewLike& t, size_type t_index, size_type count = npos)
        {
            ::std::basic_string_view<CharT, Traits> sv = t;
            auto sv_size = sv.size();
            auto size = size_();

            if (t_index > sv_size)
                throw ::std::out_of_range{ exception_string_ };

            count = ::std::min(npos, ::std::min(sv_size - t_index, count));
            auto sv_data = sv.data();
            insert_(pos, sv_data + t_index, sv_data + t_index + count);

            return *this;
        }

		// clang-format on

		// ********************************* begin erase ******************************

	private:
		constexpr void erase_(CharT* first, CharT const* last) noexcept
		{
			assert(("first or last is not in this string", first >= begin_() && last <= end_()));

			if BIZWEN_CONSTEVAL
			{
				::std::copy(last, const_cast<basic_string const&>(*this).end_(), first);
			}
			else
			{
				::std::memmove(first, last, (const_cast<basic_string const&>(*this).end_() - last) * sizeof(CharT));
			}

			resize_(size() - (last - first));
		}

	public:
		constexpr basic_string& erase(size_type index = 0, size_type count = npos)
		{
			auto size = size_();

			if (index > size)
				throw ::std::out_of_range{ exception_string_ };

			count = ::std::min(npos, ::std::min(size - index, count));
			auto start = begin_() + index;
			erase_(start, start + count);

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
			erase_(start, last.base().current_);

#ifndef NDEBUG
			return { start, this };
#else
			return { start };
#endif
		}

		// ********************************* begin pop_back ******************************

		constexpr void pop_back() noexcept
		{
			assert(("string is empty", !is_empty_()));
			resize_(size_() - 1);
		}

		// ********************************* begin replace ******************************

		constexpr basic_string& replace(size_type pos, size_type count, const basic_string& str)
		{
			replace_(pos, count, str.begin_(), str.end_());

			return *pos;
		}

		constexpr basic_string& replace(const_iterator first, const_iterator last, const basic_string& str)
		{
			auto start = first.base().current_;
			replace_(start - begin_(), last - first, str.begin_(), str.end_());

			return *this;
		}

		constexpr basic_string& replace(size_type pos, size_type count, const basic_string& str, size_type pos2, size_type count2 = npos)
		{
			auto str_size = str.size_();

			if (pos2 > str_size)
				throw ::std::out_of_range{ exception_string_ };

			count2 = ::std::min(npos, ::std::min(count2, str_size - pos2));
			auto begin = str.begin_();
			replace_(pos, count, begin + count2, begin + count2 + pos2);

			return *this;
		}

		constexpr basic_string& replace(size_type pos, size_type count, const CharT* cstr, size_type count2)
		{
			replace_(pos, count, cstr, cstr + count2);

			return *this;
		}

		constexpr basic_string& replace(const_iterator first, const_iterator last, const CharT* cstr, size_type count2)
		{
			auto start = first.base().current_;
			replace_(start - begin_(), last - first, cstr, cstr + count2);

			return *this;
		}

		constexpr basic_string& replace(size_type pos, size_type count, const CharT* cstr)
		{
			replace_(pos, count, cstr, cstr + c_style_string_length_(cstr));

			return *pos;
		}

		constexpr basic_string& replace(const_iterator first, const_iterator last, const CharT* cstr)
		{
			auto start = first.base().current_;
			replace_(start - begin_(), last - first, cstr, cstr + c_style_string_length_(cstr));

			return *this;
		}

		constexpr basic_string& replace(const_iterator first, const_iterator last, ::std::initializer_list<CharT> ilist)
		{
			auto data = ::std::data(ilist);
			auto start = first.base().current_;
			replace_(start - begin_(), last - first, data, data + ilist.size());

			return *this;
		}

		template <class StringViewLike>
		    requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& replace(size_type pos, size_type count, const StringViewLike& t)
		{
			::std::basic_string_view<CharT, Traits> sv = t;
			auto data = sv.data();
			replace_(pos, count, data, data + sv.size());

			return *this;
		}

		template <class StringViewLike>
		    requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
		constexpr basic_string& replace(const_iterator first, const_iterator last, const StringViewLike& t)
		{
			::std::basic_string_view<CharT, Traits> sv = t;
			auto sv_data = sv.data();
			auto start = first.base().current_;
			replace_(start - begin_(), last - first, sv_data, sv_data + sv.size());

			return *this;
		}

		template <class StringViewLike>
		    requires ::std::is_convertible_v<const StringViewLike&, ::std::basic_string_view<CharT, Traits>> && (!::std::is_convertible_v<const StringViewLike&, const CharT*>)
		basic_string& replace(size_type pos, size_type count, const StringViewLike& t, size_type pos2, size_type count2 = npos)
		{
			::std::basic_string_view<CharT, Traits> sv = t;
			auto sv_size = sv.size();

			if (pos2 > sv_size)
				throw ::std::out_of_range{ exception_string_ };

			count2 = ::std::min(npos, ::std::min(sv_size - pos2, count2));

			auto data = sv.data();
			replace_(pos, count, data + pos2, data + pos2 + count2);

			return *this;
		}

		constexpr basic_string& replace(size_type pos, size_type count, size_type count2, CharT ch)
		{
			basic_string temp{ count2, ch };
			auto begin = begin_();
			replace_(pos, count, begin, begin + count2);

			return *this;
		}

		constexpr basic_string& replace(const_iterator first, const_iterator last, size_type count2, CharT ch)
		{
			basic_string temp{ count2, ch };
			auto begin = begin_();
			auto start = first.base().current_;
			replace_(start - begin_(), last - first, begin, begin + count2);

			return *this;
		}

		template <class InputIt>
		basic_string& replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2)
		    requires ::std::input_iterator<InputIt>
		{
			auto start = first.base().current_;

			if constexpr (::std::random_access_iterator<InputIt>)
			{
				auto data = ::std::addressof(*first2);
				auto length2 = ::std::distance(first2, last2);
				replace_(start - begin_(), last - first, data, data + length2);
			}
			else
			{
				basic_string temp{ first2, last2 };
				auto begin = temp.begin_();
				auto size = temp.size_();
				replace_(start - begin_(), last - first, begin, begin + size);
			}

			return *this;
		}
	};

	using string = bizwen::basic_string<char8_t>;
	using wstring = bizwen::basic_string<wchar_t>;
	using u8string = bizwen::basic_string<char8_t>;
	using u16string = bizwen::basic_string<char16_t>;
	using u32string = bizwen::basic_string<char32_t>;

	static_assert(sizeof(string) == sizeof(char8_t*) * 4);
	static_assert(sizeof(wstring) == sizeof(char8_t*) * 4);
	static_assert(sizeof(u8string) == sizeof(char8_t*) * 4);
	static_assert(sizeof(u16string) == sizeof(char8_t*) * 4);
	static_assert(sizeof(u32string) == sizeof(char8_t*) * 4);
	static_assert(::std::contiguous_iterator<string::iterator>);
}
