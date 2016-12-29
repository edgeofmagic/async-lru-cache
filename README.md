# async-lru-cache

## A fast, robust C++ implementation of an LRU cache suitable for use with asynchronous event-loop environments

### Why?

I needed a simple cache for another project. There are many available C++ implementations of a simple LRU cache,
but I had specific requirements and preferences that weren't addressed by any of the existing implementations I could find.

## My Requirements

#### Compatible with an asynchronous event loop environment

The project in question employs an asynchronous event processing loop. Events (such as incoming messages, timers, etc.) trigger event handlers.
There are no calls to blocking operations; if an operation can't proceed until some external asynchronous action completes, the code
that starts the action provides a handler (typically in the form of a lambda expression) that will be invoked by the event processing
loop when the asynchronous action completes. In my target applications, the cache typically keeps local copies of values that are maintained
in a remote process, so a cache miss entails an asynchronous request to the remote process. Most of the available implementations had
functional interfaces, for example:

```` cpp
value = cache.get(key);
/* use value ... */
````
where the get() method can't return a value until the code that handles the cache miss completes, so that the call to get() would
block.

Alternatively, a non-blocking cache interface might look something like this:
```` cpp
cache.get(key, [] (const V& value) { /* use value ... */ });
````
where the lambda expression (the second argument to get()) is executed when the value associated with key becomes available,
either immediately in the case of a cache hit, or when the remote request completes in the case of a miss.

#### Transparency

Most of the available cache implementations provided *get* and *put* operations, where the get operation had some way of indiciating
that a cache miss occurred. This interface style places the burden of detecting and handing a cache miss on the calling context.
The problem of indicating a cache miss creates further complication--if the get operation returns a reference to the cached value,
the only reasonable way to indicate a cache miss is to throw an exception. Otherwise, the get operation can return a (potentially null)
 pointer to the value, either creating ownership issues or forcing the use of smart (shared) pointers for values.

Ideally, a cache should be transparent at the point of use, where the get operation handles a cache miss transparently,
and the calling context only has to deal with cases where the key is ill-formed, or the key doesn't exist in the store underlying the cache.

#### Minimizing assumptions about value types

The key types in the target project tend to be simple (typically strings), and the value types tend to be somewhat more complex
(for example, an object that encapsulates a database row). It was important that the cache not impose any requirement for a particular
constructor or assignment operator on the implementation of the value types to be cached.

#### Robust error handling

In an asynchronous environment, the results of an operation are often passed as parameters to a lambda function that provides
closure for an action, rather than returning them to the calling context. In fact, the calling context is often the asynchronous
event processing loop. Results and error codes typically get passed down the stack, rather than up it (back the the caller). For
this reason, execptions are not particularly useful in asychronous environments. 

I have grown to prefer the standard library facility for error handling embodied in the std::error_code and std::error_category classes.
In this case (the cache template), the use of std::error_code allows arbitrary error codes to be passed through the cache
interface, including codes belonging to application-specific error categories. 

#### Reduce (or eliminate) opportunities for memory leaks and memory ownership complications

The previous requirement for minimizing assumptions about value types necessitates passing some kind of object pointer from
the miss handler to the cache. Otherwise, the cache is forced to construct a copy of the object in its map, requiring the value
type to support a copy constructor (or something along those lines.) The cache interface relies on unique pointers to make
memory ownership unambiguous, and to provide for implicit deletion of the pointer if an error occurs before the cache can insert
it into the map data structure, and the pointer goes out of scope.

## Using the Cache

#### Define a handler for cache misses

The cache invokes a *miss handler* when the key you're looking for isn't found in the cache.

```` cpp
#include "lru_cache.h"

using namespace utils;

using cache_type = lru_cache<std::string, std::string>; // this alias (or typedef) comes in handy

cache_type::miss_handler handler = [] (const std::string& key, cache_type::miss_handler_reply_f reply)
{
	// create a unique pointer to the value assocated with the key

	std::string value{"the value associated with "};
	value += key;
	auto val_uptr = std::unique_ptr<std::string>(value);

	// invoke the reply functor, passing the value and an error code

	reply(val_uptr, std::error_code()); // the null-constructed error_code means no error
}

````

Of course, creating the value will usually be more interesting and problematic than this example. In particular,
it may entail making a call on an asynchronous I/O interface to read the value from a database or get it from a 
remote server. More on this later.

#### Instantiate the template:

```` cpp

cache_type the_cache(handler, 100); // Pass the miss handler and cache capacity to the constructor

````

#### Call get():

```` cpp
std::string key{"something"};
the_cache.get(key, [&] (cache_type::const_iterator it, std::error_code err)
{
	// The iterator points to the value, or past the end of the cache if no value was available,
	// in which case err should contain a meaningful error code.

	if (!err && it != the_cache.cend())
	{
		std::cout << "Found value for key " << key << ": " << *it << std::endl;
	}
	else
	{
		// ... deal with error condition
	}
});
````

## Details

The cache implementation is contained in a single header file, lru_cache.h.

#### Template parameters

The template parameters are almost identical to std::unordered_map, allowing you to specify a custom hash or equality function 
for the key type, if necessary. Otherwise, just instantiate the template specifying key and value types. 
It's useful declare a typedef or alias for the cache type, but not necessary. In the constructor, specify the capacity of the cache.

```` cpp
template <class Key, class T, class Hash = std::hash<Key>, class KeyEquals = std::equal_to<Key>>
class lru_cache
{
public:

	using key_t = Key;
	using value_t = T;
	using value_uptr_t = std::unique_ptr<T>;
	...
````

#### Iterator

The template defines a const_iterator type consistent with standard library const forward iterators. It has two purposes:

1. The iterator can be tested for validity (by comparing it to the past-the-end iterator value for the cache) 
before dereferencing, without exposing pointers to the cached values.

2. The iterator allows the cache contents to be examined in usage order, for diagnostic and testing purposes.

#### Key type

The key type must have functions for hash and equal_to available that match the default template parameters, 
or appropriate functions must be supplied as parameters when the template is instantiated. If a type can be used as a key for 
std::unordered_map, it will serve as a key for the cache template. In addition, the key type must support a copy constructor.

#### Value type

The value type is not required to support any particular form of constructor or assignment operator. 
The only requirement is that the miss handler, based on the key, must be able to obtain or construct an appropriate 
instance of the value type in the form of a unique pointer (see below).

#### Miss handler

The application must supply a miss handler--a function that will be invoked by the cache when the value for a particular 
key is not present in the cache when requested (that is, a cache miss occurred). The miss handler is a void function that takes two parameters. 
The first is the key in question (as a const reference), and the second is a miss handler reply function, which are specified as type alias declarations:

```` cpp
template <class Key, class T, class Hash = std::hash<Key>, class KeyEquals = std::equal_to<Key>>
class lru_cache
{
public:

	using miss_handler_reply_f = std::function< void (value_uptr_t, const std::error_code&) >;
	using miss_handler_f = std::function< void (const Key&, miss_handler_reply_f) >;
````

The miss handler must do the following:

* Use the key value to retrieve/construct/conjure the appropriate value. This value must take the form 
of a unique pointer to an instance of the value type when passed to the miss handler reply function.

* Invoke the miss handler reply function (second paramter of the miss handler), passing said unique pointer and an error code.

* If the requested value isn't available, the miss_handler should pass a unique pointer whose value is nullptr, 
and a non-zero error code that the application can recognise as an appropriate error condition. Otherwise, 
the default (zero-valued) error_code of std::system_category should be passed.

#### Constructor

The constructor takes three parameters---miss handler, cache capacity, and load factor.

* Cache capacity is the maximum number of entries that the cache can hold. If the cache is full when a cache miss occurs 
(which causes the value produced by the miss handler to be inserted), the least recently used entry in the cache is evicted.
A cache cannot be re-sized after construction.

* Load factor (a float value) sets the effective load factor for the specified capacity. 
The cache implementation constructs the underlying unordered map with a bucket count set to the specified cache capacity 
divided by the load factor. The load factor parameter has a default value of 0.75, and the value is forced into the range (0.5, 0.95).

#### Example

A small (and rather silly) but complete example is provided in the examples subdirectory.

#### Design Decisions

*The signatures for get and the miss handler seem awkward. What's the deal?*

They seem awkward until you try to use them in an asynchronous, non-blocking environment. In my target applications, 
the cache is nearly always storing local copies of objects that obtained from a remote service, or in-memory copies of 
objects stored in some kind of database. In these cases, the miss handler must employ an asynchronous calling interface 
to either network services or the database. The details of doing this depend on the particular environment being used t
o manage asynchronous I/O. Typically, the miss handler must supply the environment with a callback function and some 
context data (or both in the form of a lambda expression with its captured values). The miss handler reply is the essential 
element of context that fulfills this role.

*Why use unique pointers in the miss handler?*

The use of a unique pointer makes the ownership of the memory for the value unambiguous. It also puts the responsibility for constructing the value instance completely on the application context. The cache never needs to construct an instance of the value type, so it imposes no requirement for any particular form of constructor.

*Why pass the value as a const iterator to the get callback lambda?*

1. The interface must be able to represent a non-existent (null) instance of the value type, but
2. I didn't want to expose a pointer to the value being managed by the map encapsulated in the cache.
3. The calling context for get (including the callback lambda) should not be able to modify the value managed by the cache.

Passing a const iterator achieves all three goals nicely.

#### To do

* Provide a working example of using the cache in a real asynchronous environment.


