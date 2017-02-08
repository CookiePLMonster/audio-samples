/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once

#include "Patterns.h"

#include <cassert>
#include <vector>

#define PATTERNS_USE_HINTS 0

namespace hook
{
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
		T* get(ptrdiff_t offset) const
		{
			char* ptr = reinterpret_cast<char*>(m_pointer);
			return reinterpret_cast<T*>(ptr + offset);
		}

		template<typename T>
		T* get() const
		{
			return get<T>(0);
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

		inline const pattern_match& get_one()
		{
			return count(1).get(0);
		}

		template<typename T = void>
		inline auto get_first()
		{
			return get_one().get<T>();
		}

		template<typename T = void>
		inline auto get_first(ptrdiff_t offset)
		{
			return get_one().get<T>(offset);
		}

	public:
#if PATTERNS_USE_HINTS
		// define a hint
		static void hint(uint64_t hash, uintptr_t address);
#endif
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
	auto get_pattern(const char(&pattern_string)[Len])
	{
		return pattern(pattern_string).get_first<T>();
	}

	template<typename T = void, size_t Len>
	auto get_pattern(const char(&pattern_string)[Len], ptrdiff_t offset)
	{
		return pattern(pattern_string).get_first<T>(offset);
	}
}