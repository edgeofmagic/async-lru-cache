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

#ifndef guard_utils_lru_cache_h
#define guard_utils_lru_cache_h

#include <unordered_map>
#include <memory>
#include <iterator>
#include <deque>
#include <vector>

namespace utils
{
	template <class Key, class T, class Hash = std::hash<Key>, class KeyEquals = std::equal_to<Key>>
	class lru_cache
	{
	public:
	
		using key_t = Key;
		using value_t = T;
		using value_uptr_t = std::unique_ptr<T>;
	
	protected:
	
		class node;
		
		using lru_map_t = std::unordered_map<Key, node, Hash, KeyEquals>;
		using map_iterator = typename lru_map_t::iterator;
		using map_entry = std::pair<const Key , node>;
		using entry_ptr = map_entry *;
		
		class node
		{
		public:
		
			inline node(std::unique_ptr<T> val_ptr)
			:
			value_{std::move(val_ptr)},
			older_{nullptr},
			newer_{nullptr}
			{}
			
			inline node()
			:
			value_{nullptr},
			older_{nullptr},
			newer_{nullptr}
			{}
		
			inline node(const node& that) = delete;
			
			inline node(node&& that) = delete;

			std::unique_ptr<T> value_;
			entry_ptr older_;
			entry_ptr newer_;
		};

	public:
	
		class const_iterator
		{
		private:
			entry_ptr ptr_;
			
			inline const_iterator(const entry_ptr ptr)
			:
			ptr_{ptr}
			{}
		
			friend class lru_cache;
			
		public:
			typedef std::forward_iterator_tag iterator_category;
			typedef T value_t;
			typedef const T& const_reference;
			typedef const T* const const_pointer;
			typedef ptrdiff_t difference_type;
		
			inline const_iterator(const const_iterator& that)
			:
			ptr_{that.ptr_}
			{}
			
			inline const_iterator()
			:
			ptr_{nullptr}
			{}
			
			inline const_iterator& operator=(const const_iterator& that)
			{
				ptr_ = that.ptr_;
				return *this;
			}
			
			inline ~const_iterator()
			{
				ptr_ = nullptr; // the iterator doesn't own this pointer, don't delete
			}
			
			inline const_iterator operator++()
			{
				ptr_ = ptr_->second.older_;
				return const_iterator{*this};
			}
			
			inline const_iterator operator++(int)
			{
				const_iterator retval{*this};
				ptr_ = ptr_->second.older_;
				return retval;
			}
			
			inline bool operator==(const const_iterator& that) const
			{
				return ptr_ == that.ptr_;
			}
			
			inline bool operator!=(const const_iterator& that) const
			{
				return ptr_ != that.ptr_;
			}
			
			inline const_reference operator*() const
			{
				return ptr_->second.value_.operator*();
			}
			
			inline const_pointer operator->() const
			{
				return ptr_->second.value_.operator->();
			}
			
			inline friend void swap(const_iterator& a, const_iterator& b)
			{
				const_iterator tmp{a};
				a = b;
				b = tmp;
			}
			
			// added for test purposes
			
			inline bool check_linkage() const
			{
				return (ptr_->second.older_->second.newer_ == ptr_) && (ptr_->second.newer_->second.older_ == ptr_);
			}
			
		};
		
		using get_reply_f = std::function< void (const_iterator, std::error_code) >;
		using miss_handler_reply_f = std::function< void (value_uptr_t, std::error_code) >;
		using miss_handler_f = std::function< void (const Key&, miss_handler_reply_f) >;
		
	protected:
		
		using pending_reply_list_t = std::vector<get_reply_f>;
		using pending_map_t = std::unordered_map<Key, pending_reply_list_t>;
		using pending_map_iterator_t = typename pending_map_t::iterator;
		using pending_reply_iterator_t = typename pending_reply_list_t::iterator;
		
	public:
		
		inline lru_cache(miss_handler_f miss_handler, std::size_t limit, float load = 0.75)
		:
		miss_handler_{miss_handler},
		map_{static_cast<std::size_t>( static_cast<float>(limit) / ((load < 0.5) ? 0.5 : ((load > 0.95) ? 0.95 : load))) + 1},
		sentinel_{},
		limit_{limit}
		{
			sentinel_.second.newer_ = &sentinel_;
			sentinel_.second.older_ = &sentinel_;
		}
		
		inline ~lru_cache()
		{}
		
		lru_cache() = delete;
		
		lru_cache(const lru_cache& that) = delete;
		
		lru_cache(lru_cache&& that) = delete;
		
		lru_cache& operator=(const lru_cache& that) = delete;
	
		lru_cache& operator=(lru_cache&& that) = delete;
		
		inline const_iterator cbegin() const
		{
			return const_iterator(sentinel_.second.older_);
		}
		
		inline const_iterator cend() const
		{
			return const_iterator(const_cast<const entry_ptr>(&sentinel_));
		}
		
		inline std::size_t size() const
		{
			return map_.size();
		}
		
		inline std::size_t limit() const
		{
			return limit_;
		}
		
		inline void flush()
		{
			map_.clear();
			sentinel_.second.newer_ = &sentinel_;
			sentinel_.second.older_ = &sentinel_;
			
			// pending_replies_ should decidedly NOT be cleared
		}
		
		void get(const Key& key, get_reply_f reply)
		{
			static const std::error_code no_error{0, std::system_category()};
			
			auto hit = map_.find(key);
			if (hit != map_.end())
			{
				touch(hit);
				reply(const_iterator{&(*hit)}, no_error);
			}
			else
			{
				auto pending_iter = pending_replies_.find(key);
				if (pending_iter != pending_replies_.end())
				{
					//	a previous call to miss_handler is still pending
					//	add this reply to the list for the key
					
					pending_iter->second.push_back(reply);
				}
				else
				{
					// create an entry in pending_replies for the key
					// with this reply in the list
					
					auto pending_emplaced = pending_replies_.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(pending_reply_list_t()));
					pending_iter = pending_emplaced.first;
					pending_iter->second.push_back(reply);
					
					// call the miss_handler

					miss_handler_(key,
					[this,key] (value_uptr_t val_uptr, std::error_code err = std::error_code())
					{
						const_iterator result_iter{cend()};
						
						if (val_uptr)
						{
							result_iter = add_entry(key, std::move(val_uptr));
						}
							
						auto pending_reply_iter = pending_replies_.find(key);
						for (auto& pending_reply : pending_reply_iter->second)
						{
							pending_reply(result_iter, err);
						}
						pending_replies_.erase(pending_reply_iter);
					});
				}
			}
		}
		
		inline const_iterator find(const Key& key) const
		{
			const_iterator retval;
			
			auto hit = map_.find(key);
			if (hit != map_.end())
			{
				retval = const_iterator{&(*hit)};
			}
			else
			{
				retval = cend();
			}
			
			return retval;
		}

		inline void invalidate(const Key& key)
		{
			auto fit = map_.find(key);
			if (fit != map_.end())
			{
				remove(fit);
			}
		}

	protected:
		
		inline const_iterator add_entry(const Key& key, std::unique_ptr<T> val_uptr)
		{
			auto emplaced = map_.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::move(val_uptr)));
			
			insert_at_head(&(*emplaced.first));
			enforce_limit();
			return const_iterator{&(*emplaced.first)};
		}


		inline void remove(map_iterator& it)
		{
			extract(&(*it));
			map_.erase(it);
		}

		inline void extract(entry_ptr node)
		{
			entry_ptr older = node->second.older_;
			entry_ptr newer = node->second.newer_;
			newer->second.older_ = older;
			older->second.newer_ = newer;
		}

		inline void insert_at_head(entry_ptr node)
		{
			entry_ptr front = sentinel_.second.older_;
			node->second.older_ = front;
			node->second.newer_ = &sentinel_;
			front->second.newer_ = node;
			sentinel_.second.older_ = node;
		}
		
		inline void evict_lru()
		{
			auto lru = sentinel_.second.newer_;
			auto found = map_.find(lru->first);
			remove(found);
		}

		inline void touch(entry_ptr node)
		{
			extract(node);
			insert_at_head(node);
		}

		inline void touch(const map_iterator& it)
		{
			touch(&(*it));
		}
		
		inline void enforce_limit()
		{
			while (map_.size() > limit_)
			{
				evict_lru();
			}
		}
		
		map_entry			sentinel_;
		lru_map_t			map_;
		std::size_t			limit_;
		miss_handler_f		miss_handler_;
		pending_map_t		pending_replies_;
	};
	
}

#endif /* guard_utils_lru_cache_h */
