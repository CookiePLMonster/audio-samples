/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once

#include <cassert>
#include <vector>

#define PATTERNS_USE_HINTS 0

namespace hook
{
	extern ptrdiff_t baseAddressDifference;

	// sets the base address difference based on an obtained pointer
	inline void set_base(uintptr_t address)
	{
#ifdef _M_IX86
		uintptr_t addressDiff = (address - 0x400000);
#elif defined(_M_AMD64)
		uintptr_t addressDiff = (address - 0x140000000);
#endif

		// pointer-style cast to ensure unsigned overflow ends up copied directly into a signed value
		baseAddressDifference = *(ptrdiff_t*)&addressDiff;
	}

	// sets the base to the process main base
	void set_base();

	template<typename T>
	inline T* getRVA(uintptr_t rva)
	{
		set_base();
#ifdef _M_IX86
		return (T*)(baseAddressDifference + 0x400000 + rva);
#elif defined(_M_AMD64)
		return (T*)(0x140000000 + rva);
#endif
	}

	class pattern_match
	{
	private:
		void* m_pointer;

	public:
		inline pattern_match(void* pointer)
			: m_pointer(pointer)
		{
		}

		template<typename T>
		T* get(ptrdiff_t offset = 0) const
		{
			char* ptr = reinterpret_cast<char*>(m_pointer);
			return reinterpret_cast<T*>(ptr + offset);
		}

		template<typename T>
		T* get_null(ptrdiff_t offset = 0) const
		{
			char* ptr = reinterpret_cast<char*>(m_pointer);
			return ptr != nullptr ? reinterpret_cast<T*>(ptr + offset) : nullptr;
		}
	};

	class pattern
	{
	private:
		std::string m_bytes;
		std::string m_mask;

#if PATTERNS_USE_HINTS
		uint64_t m_hash;
#endif

		size_t m_size;

		std::vector<pattern_match> m_matches;

		bool m_matched;

	protected:
		void* m_module;

	protected:
		inline pattern(void* module)
			: m_module(module)
		{
		}

		void Initialize(const char* pattern, size_t length);

	private:
		bool ConsiderMatch(uintptr_t offset);

		void EnsureMatches(uint32_t maxCount);

	public:
		template<size_t Len>
		pattern(const char (&pattern)[Len])
		{
			m_module = getRVA<void>(0);

			Initialize(pattern, Len);
		}

		inline pattern& count(uint32_t expected)
		{
			if (!m_matched)
			{
				EnsureMatches(expected);
			}

			assert(m_matches.size() == expected);

			return *this;
		}

		inline size_t size()
		{
			if (!m_matched)
			{
				EnsureMatches(UINT32_MAX);
			}

			return m_matches.size();
		}

		inline const pattern_match& get(size_t index)
		{
			if (!m_matched)
			{
				EnsureMatches(UINT32_MAX);
			}

			return m_matches[index];
		}

		inline const pattern_match& get_null(size_t index)
		{
			if (!m_matched)
			{
				EnsureMatches(UINT32_MAX);
			}

			return !m_matches.empty() ? m_matches[index] : NULL_MATCH;
		}

		inline const pattern_match& get_one()
		{
			return count(1).get(0);
		}

		inline const pattern_match& get_one_null()
		{
			return count(1).get_null(0);
		}

		template<typename T = void>
		inline auto get_first(ptrdiff_t offset = 0)
		{
			return get_one().get<T>(offset);
		}

		template<typename T = void>
		inline auto get_first_null(ptrdiff_t offset = 0)
		{
			return get_one_null().get_null<T>(offset);
		}

	public:
#if PATTERNS_USE_HINTS
		// define a hint
		static void hint(uint64_t hash, uintptr_t address);
#endif

	private:
		static const pattern_match NULL_MATCH;
	};

	class module_pattern
		: public pattern
	{
	public:
		template<size_t Len>
		module_pattern(void* module, const char(&pattern)[Len])
			: pattern(module)
		{
			Initialize(pattern, Len);
		}
	};


	template<typename T = void, size_t Len>
	auto get_pattern(const char(&pattern_string)[Len], ptrdiff_t offset = 0)
	{
		return pattern(pattern_string).get_first<T>(offset);
	}

	template<typename T = void, size_t Len>
	auto get_pattern_null(const char(&pattern_string)[Len], ptrdiff_t offset = 0)
	{
		return pattern(pattern_string).get_first_null<T>(offset);
	}
}