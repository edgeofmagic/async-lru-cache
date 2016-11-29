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

#include <iostream>
#include "../include/lru_cache.h"

using namespace utils;

// The cache in this example maps strings containing unsigned decimal numbers
// to values that encapsulate the number as an unsigned long. Not particularly
// realistic, but it makes the miss handler compact.

// ex_value is a trivial value class for example purposes. Note that all of the generated
// constructors and assignment operators are deleted--the cache implementation
// doesn't depend on any of them.

class ex_value
{
public:
	ex_value() = delete;
	ex_value(const ex_value&) = delete;
	ex_value(ex_value&&) = delete;
	ex_value& operator=(const ex_value&) = delete;
	ex_value& operator=(ex_value&&) = delete;
	
	ex_value(unsigned long number) : number_(number) {}
	
	unsigned long get_number() const { return number_; }
	
private:
	unsigned long number_;
};

// An alias (or typedef) for the cache type comes in handy:

using ex_cache_type = lru_cache<std::string, ex_value>;

// dump_cache prints the cached values, in usage order, from
// the specified iterator position to the least recently used cache value (the end),
// illustrating the use of the cache interator:

void dump_cache(ex_cache_type::const_iterator it, ex_cache_type::const_iterator past_end)
{
	while (it != past_end)
	{
		std::cout << it->get_number() << std::endl;
		it++;
	}
}

int main(int argc, const char * argv[])
{
	// The constructor requires a miss_handler function, supplied here as a lambda expression.
	//
	// The miss handler should do the following:
	//	1. get/read/conjure the value associated with key, in the form of a unique pointer to the value type
	//	2. call the miss_handler_reply (the second argument of the miss handler), passing the unique pointer
	//		as an rvalue, and an empty (zero-valued) error code.
	//
	//	If the key is ill-formed or the value can't be obtained, the miss handler should call the reply
	//	with a null unique pointer and an appropriate error code.
	//
	//	In this example, the miss handler simply converts the key (assumed to hold a numeric value) to
	//	a number and constructs the value object with that number.
	//
	//	The second parameter is the capacity of the cache. If the cache is full, and the miss handler is called,
	//	the entry at the tail of the usage order list (the least recently used entry) is evicted, and the new
	//	entry is placed at the head of the list.
	
	ex_cache_type ex_cache([] (const std::string& key, ex_cache_type::miss_handler_reply_f reply) noexcept
	{
		// the conversion function std::stoull throws an exception if the string is malformed
		try
		{
			auto numval = std::stoull(key);
			std::cout << "in miss handler. key='" << key << "', value=" << numval << std::endl;
			
			// if the key was valid, construct an instance of ex_value and create a unique pointer to it,
			// then invoke the reply function with that value and a null error code.
			
			reply(std::unique_ptr<ex_value>(new ex_value(numval)), std::error_code());
		}
		catch (const std::exception& e)
		{
			// if the key was malformed, invoke the reply with a null unique pointer, and the error code
			// of your choice (from the category of your choice).
		
			std::cout << "in miss handler. invalid key '" << key << "'" << std::endl;
			reply(ex_cache_type::value_uptr_t(nullptr), std::make_error_code(std::errc::invalid_argument));
		}
	}, 3);


/*
The following series of cache operations should produce this output:

--------------------------------------------------------------------
in miss handler. invalid key 'one'
no value available for key='one', error code Invalid argument
in miss handler. key='1', value=1
get result for key='1', value=1
in miss handler. key='2', value=2
get result for key='2', value=2
in miss handler. key='3', value=3
get result for key='3', value=3
3
2
1
get result for key='1', value=1
get result for key='2', value=2
get result for key='3', value=3
in miss handler. key='4', value=4
get result for key='4', value=4
4
3
2
in miss handler. key='1', value=1
get result for key='1', value=1
1
4
3
Program ended with exit code: 0
--------------------------------------------------------------------
	
*/


	ex_cache.get("one", [&] (ex_cache_type::const_iterator it, std::error_code err)
	{
		if (!err && it != ex_cache.cend())
		{
			std::cout << "get result for key='one', value=" << it->get_number() << std::endl;
		}
		else
		{
			std::cout << "no value available for key='one', error code " << err.message() << std::endl;
		}
	});

	ex_cache.get("1", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		if (!err)
		{
			std::cout << "get result for key='1', value=" << it->get_number() << std::endl;
		}
		else
		{
			std::cout << "no value available for key='1', error code " << err.message() << std::endl;
		}
	});

	ex_cache.get("2", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='2', value=" << it->get_number() << std::endl;
	});
	
	ex_cache.get("3", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='3', value=" << it->get_number() << std::endl;
	});

	dump_cache(ex_cache.cbegin(), ex_cache.cend());

	ex_cache.get("1", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='1', value=" << it->get_number() << std::endl;
	});

	ex_cache.get("2", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='2', value=" << it->get_number() << std::endl;
	});
	
	ex_cache.get("3", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='3', value=" << it->get_number() << std::endl;
	});

	ex_cache.get("4", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='4', value=" << it->get_number() << std::endl;
	});

	dump_cache(ex_cache.cbegin(), ex_cache.cend());

	ex_cache.get("1", [] (ex_cache_type::const_iterator it, std::error_code err)
	{
		std::cout << "get result for key='1', value=" << it->get_number() << std::endl;
	});

	dump_cache(ex_cache.cbegin(), ex_cache.cend());
	
	
	using cache_type = lru_cache<std::string, std::string>;
	
	cache_type::miss_handler_f miss_handler = [] (const std::string& key, cache_type::miss_handler_reply_f reply) noexcept
	{
		std::string reverse;
		for (auto rit = key.rbegin(); rit != key.rend(); rit++)
		{
			reverse.push_back(*rit);
		}
		cache_type::value_uptr_t val_uptr(new std::string(std::move(reverse)));
		reply(std::move(val_uptr), std::error_code());
	};
	
	cache_type cache_2(miss_handler, 5);
	
	cache_2.get("cow", [&] (cache_type::const_iterator vit, const std::error_code& err)
	{
		if (!err && vit != cache_2.cend())
		{
			std::cout << "cow value is '" << *vit << "'" << std::endl;
		}
	});

    return 0;
}
