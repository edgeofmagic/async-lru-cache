/*
MIT License

Copyright © 2016 David Curtis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef guard_async_lru_cache_test_h
#define guard_async_lru_cache_test_h

#include "../include/lru_cache.h"
#include <iostream>
#include <vector>

class test_value
{
public:

	test_value() = delete;

	test_value(std::uint64_t n)
	:
	n_ptr_(new std::uint64_t(n))
	{}
	
	~test_value()
	{}
	
	bool operator==(const test_value& that) const
	{
		return *n_ptr_ == *(that.n_ptr_);
	}

	test_value(const test_value& that)
	:
	test_value(that.get())
	{}

	test_value& operator=(const test_value& that) = delete;
	
	test_value& operator=(test_value&& that) = delete;

	test_value(test_value&& that)
	{
		n_ptr_ = std::move(that.n_ptr_);
		that.n_ptr_ = nullptr;
	}
	
	std::uint64_t get() const
	{
		return *n_ptr_;
	}
	
private:
	std::unique_ptr<std::uint64_t>	n_ptr_;
};

class test_value_move_constructible
{
public:

	test_value_move_constructible(std::uint64_t n)
	:
	n_ptr_(new std::uint64_t(n))
	{}
	
	test_value_move_constructible(test_value_move_constructible&& that)
	{
		n_ptr_ = std::move(that.n_ptr_);
		that.n_ptr_ = nullptr;
	}
	
	~test_value_move_constructible()
	{}
	
	bool operator==(const test_value_move_constructible& that) const
	{
		return *n_ptr_ == *(that.n_ptr_);
	}

	test_value_move_constructible() = delete;

	test_value_move_constructible(const test_value_move_constructible& that) = delete;

	test_value_move_constructible& operator=(const test_value_move_constructible& that) = delete;
	
	test_value_move_constructible& operator=(test_value_move_constructible&& that) = delete;

	std::uint64_t get() const
	{
		return *n_ptr_;
	}
	
private:
	std::unique_ptr<std::uint64_t>	n_ptr_;
};


class test_value_copy_constructible
{
public:

	test_value_copy_constructible(std::uint64_t n)
	:
	n_ptr_(new std::uint64_t(n))
	{}
	
	test_value_copy_constructible(const test_value_copy_constructible& that)
	:
	test_value_copy_constructible(that.get())
	{}

	~test_value_copy_constructible()
	{}
	
	bool operator==(const test_value_copy_constructible& that) const
	{
		return *n_ptr_ == *(that.n_ptr_);
	}

	test_value_copy_constructible& operator=(const test_value_copy_constructible& that) = delete;
	
	test_value_copy_constructible& operator=(test_value_copy_constructible&& that) = delete;

	test_value_copy_constructible() = delete;

	test_value_copy_constructible(test_value_copy_constructible&& that) = delete;
	
	std::uint64_t get() const
	{
		return *n_ptr_;
	}
	
private:
	std::unique_ptr<std::uint64_t>	n_ptr_;
};

template<class V>
class test_fixture
{
public:
	using cache_type = utils::lru_cache<std::string, V>;
	
	test_fixture(const std::string& test_name, std::size_t limit)
	:
	test_name_(test_name),
	cache_(
		[] (const std::string& key, typename cache_type::miss_handler_reply_f reply) noexcept
		{
			convert(key, [=] (std::uint64_t num, const std::error_code& err)
			{
				if (!err)
				{
					reply(std::move(std::unique_ptr<V>(new V(num))), std::error_code());
				}
				else
				{
					reply(std::move(std::unique_ptr<V>(nullptr)), err);
				}
			});
		}, limit)
	{}
	
	cache_type& cache()
	{
		return cache_;
	}
	
	template<class Reply>
	static void convert(const std::string& strval, Reply reply)
	{
		try
		{
			auto numval = std::stoull(strval);
			reply(numval, std::error_code(0, std::system_category()));
		}
		catch (const std::exception& e)
		{
			reply(0ULL, std::make_error_code(std::errc::invalid_argument));
		}
	}
	
	bool expect_error(typename cache_type::const_iterator iter, const std::error_code& err)
	{
		bool result = true;
		
		if (iter != cache_.cend())
		{
			std::cout << test_name_ << " failed, unexpected iterator to past-the-end element" << std::endl;
			result = false;
		}
		
		if (!err)
		{
			std::cout << test_name_ << " failed, unexpected non-zero error code" << std::endl;
			result = false;
		}
		
		return result;
	}

	bool expect_value(typename cache_type::const_iterator iter, std::uint64_t expected)
	{
		bool retval = true;
		if (iter == cache_.cend())
		{
			std::cout << test_name_ << " failed, unexpected iterator to past-the-end element" << std::endl;
			retval = false;
		}
		else
		{
			if (iter->get() != expected)
			{
				std::cout << test_name_ << " failed, found value [" << iter->get() << "] at iterator, expected [" << expected << "]" << std::endl;
				retval = false;
			}
		}
		return retval;
	}
	
	void fill(std::size_t start, std::size_t end)
	{
		for (auto i = start; i < end; ++i)
		{
			cache_.get(std::to_string(i), [=] (typename cache_type::const_iterator hit, const std::error_code& err)
			{
				expect_value(hit, i);
			});
		}
	}
	
	bool list_integrity_check() const
	{
		bool retval = true;
		std::size_t count = 0;
		
		auto iter = cache_.cbegin();
		
		while (iter != cache_.cend())
		{
			if (!iter.check_linkage())
			{
				std::cout << test_name_ << " failed: list pointers corrupted" << std::endl;
				retval = false;
				break;
			}
			
			if (!(count < cache_.size()))
			{
				std::cout << test_name_ << " failed: list pointers corrupted, count of list elements exceeeds map size" << std::endl;
				retval = false;
				break;
			}
			count++;
			iter++;
		}
		
		if (retval)
		{
			if (count != cache_.size())
			{
				std::cout << test_name_ << " failed: count of list elements doesn't match map size" << std::endl;
				retval = false;
			}
		}
		
		return retval;
	}
	
	
	bool list_check(const std::vector<std::uint64_t>& expected)
	{
		bool result = true;
		std::size_t count = 0;
		
		auto it = cache_.cbegin();
		
		while (it != cache_.cend())
		{
			if (it->get() != expected[count])
			{
				std::cout << test_name_ << " failed at index " << count << ": expected " << expected[count] << ", found " << it->get() << std::endl;
				result = false;
			}
		
			count++;
			
			if (count > expected.size())
			{
				std::cout << test_name_ << " failed: list item count (" << count << ") exceeds number of expected items (" << expected.size() << ")" << std::endl;
				result = false;
				break;
			}
			else
			{
				it++;
			}
		}
		if (result && count != expected.size())
		{
			std::cout << test_name_ << " failed: list item count (" << count << ") doesn't match number of expected items(" << expected.size() << ")" << std::endl;
			result = false;
		}
		
		return result;
	}

	void lru_order_test()
	{
	
		using cache_type = utils::lru_cache<std::string, V>;
		
		std::cout << "starting " << test_name_ << ": lru order test" << std::endl;
		
		fill(0,5);
		
		list_check({4, 3, 2, 1, 0});
		list_integrity_check();
		
		cache().get("2", [=] (typename cache_type::const_iterator iter, const std::error_code& err)
		{
			expect_value(iter, 2);
		});
		
		list_check({2, 4, 3, 1, 0});
		list_integrity_check();
	}
	
	void evict_lru_test()
	{
		using cache_type = utils::lru_cache<std::string, V>;
		
		std::cout << "starting " << test_name_ << ": evict lru test" << std::endl;
		
		fill(0,5);
		
		list_check({4, 3, 2, 1, 0});
		list_integrity_check();
		
		cache().get("5", [=] (typename cache_type::const_iterator iter, const std::error_code& err)
		{
			expect_value(iter, 5);
		});
		
		list_check({5, 4, 3, 2, 1});
		list_integrity_check();
	}
	
	void miss_handler_error_test()
	{
		using cache_type = utils::lru_cache<std::string, V>;
		
		std::cout << "starting " << test_name_ << ": miss handler error test" << std::endl;
		
		cache().get("not_a_number", [=] (typename cache_type::const_iterator iter, const std::error_code& err)
		{
			expect_error(iter, err);
		});
		
		list_integrity_check();
	}

	void run()
	{
		lru_order_test();
		evict_lru_test();
		miss_handler_error_test();
	}
	
protected:

	cache_type cache_;
	std::string	test_name_;
};

#endif /* guard_async_lru_cache_test_h */
