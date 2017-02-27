/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "Patterns.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sstream>
#include <algorithm>

#if PATTERNS_USE_HINTS
#include <map>
#endif


#if PATTERNS_USE_HINTS

// from boost someplace
template <std::uint64_t FnvPrime, std::uint64_t OffsetBasis>
struct basic_fnv_1
{
	std::uint64_t operator()(std::string const& text) const
	{
		std::uint64_t hash = OffsetBasis;
		for (std::string::const_iterator it = text.begin(), end = text.end();
			 it != end; ++it)
		{
			hash *= FnvPrime;
			hash ^= *it;
		}

		return hash;
	}
};

const std::uint64_t fnv_prime = 1099511628211u;
const std::uint64_t fnv_offset_basis = 14695981039346656037u;

typedef basic_fnv_1<fnv_prime, fnv_offset_basis> fnv_1;

#endif

namespace hook
{
	ptrdiff_t baseAddressDifference;

	// sets the base to the process main base
	void set_base()
	{
		set_base((uintptr_t)GetModuleHandle(nullptr));
	}


#if PATTERNS_USE_HINTS
static std::multimap<uint64_t, uintptr_t> g_hints;
#endif

static void TransformPattern(const std::string& pattern, std::string& data, std::string& mask)
{
	std::ostringstream dataStr;
	std::ostringstream maskStr;

	uint8_t tempDigit = 0;
	bool tempFlag = false;

	for (auto& ch : pattern)
	{
		if (ch == ' ')
		{
			continue;
		}
		else if (ch == '?')
		{
			dataStr << '\x00';
			maskStr << '?';
		}
		else if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'))
		{
			char str[] = { ch, 0 };
			int thisDigit = strtol(str, nullptr, 16);

			if (!tempFlag)
			{
				tempDigit = uint8_t(thisDigit << 4);
				tempFlag = true;
			}
			else
			{
				tempDigit |= thisDigit;
				tempFlag = false;

				dataStr << tempDigit;
				maskStr << 'x';
			}
		}
	}

	data = dataStr.str();
	mask = maskStr.str();
}

class executable_meta
{
private:
	uintptr_t m_begin;
	uintptr_t m_end;

public:
	template<typename TReturn, typename TOffset>
	TReturn* getRVA(TOffset rva)
	{
		return (TReturn*)(m_begin + rva);
	}

	executable_meta(void* module)
		: m_begin((uintptr_t)module)
	{
		PIMAGE_DOS_HEADER dosHeader = getRVA<IMAGE_DOS_HEADER>(0);
		PIMAGE_NT_HEADERS ntHeader = getRVA<IMAGE_NT_HEADERS>(dosHeader->e_lfanew);

		m_end = m_begin + ntHeader->OptionalHeader.SizeOfCode;
	}

	inline uintptr_t begin() const { return m_begin; }
	inline uintptr_t end() const   { return m_end; }
};

void pattern::Initialize(const char* pattern, size_t length)
{
	// get the hash for the base pattern
	std::string baseString(pattern, length);
#if PATTERNS_USE_HINTS
	m_hash = fnv_1()(baseString);
#endif

	m_matched = false;

	// transform the base pattern from IDA format to canonical format
	TransformPattern(baseString, m_bytes, m_mask);

	m_size = m_mask.size();

#if PATTERNS_USE_HINTS
	// if there's hints, try those first
	if (m_module == GetModuleHandle(nullptr))
	{
		auto range = g_hints.equal_range(m_hash);

		if (range.first != range.second)
		{
			std::for_each(range.first, range.second, [&] (const std::pair<uint64_t, uintptr_t>& hint)
			{
				ConsiderMatch(hint.second);
			});

			// if the hints succeeded, we don't need to do anything more
			if (!m_matches.empty())
			{
				m_matched = true;
				return;
			}
		}
	}
#endif
}

void pattern::EnsureMatches(uint32_t maxCount)
{
	if (m_matched)
	{
		return;
	}

	// scan the executable for code
	executable_meta executable(m_module);

	// check if SSE 4.2 is supported
	bool sse42 = false;

	if (m_mask.size() <= 16)
	{
		int cpuid[4];
		__cpuid(cpuid, 0);

		if (cpuid[0] >= 1)
		{
			__cpuidex(cpuid, 1, 0);

			sse42 = (cpuid[2] & (1 << 20)) != 0;
		}
	}

	auto matchSuccess = [&] (uintptr_t address)
	{
#if PATTERNS_USE_HINTS
		g_hints.insert(std::make_pair(m_hash, address));
#else
		(void)address;
#endif

		return (m_matches.size() == maxCount);
	};

	if (!sse42)
	{
		for (uintptr_t i = executable.begin(); i != executable.end(); i++)
		{
			if (ConsiderMatch(i))
			{
				if (matchSuccess(i))
				{
					break;
				}
			}
		}
	}
	else
	{
		__declspec(align(16)) char desiredMask[16] = { 0 };

		for (size_t i = 0; i < m_mask.size(); i++)
		{
			desiredMask[i >> 3] |= ((m_mask[i] == '?') ? 0 : 1) << (i & 7);
		}

		__m128i mask = _mm_load_si128(reinterpret_cast<const __m128i*>(desiredMask));
		__m128i comparand = _mm_loadu_si128(reinterpret_cast<const __m128i*>(m_bytes.c_str()));

		for (uintptr_t i = executable.begin(); i != executable.end(); i++)
		{
			__m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(i));
			__m128i result = _mm_cmpestrm(value, 16, comparand, m_bytes.size(), _SIDD_CMP_EQUAL_EACH);

			// as the result can match more bits than the mask contains
			__m128i matches = _mm_and_si128(mask, result);
			__m128i equivalence = _mm_xor_si128(mask, matches);

			if (_mm_test_all_zeros(equivalence, equivalence))
			{
				m_matches.emplace_back((void*)i);

				if (matchSuccess(i))
				{
					break;
				}
			}
		}
	}

	m_matched = true;
}

bool pattern::ConsiderMatch(uintptr_t offset)
{
	const char* pattern = m_bytes.c_str();
	const char* mask = m_mask.c_str();

	char* ptr = reinterpret_cast<char*>(offset);

	for (size_t i = 0; i < m_size; i++)
	{
		if (mask[i] == '?')
		{
			continue;
		}

		if (pattern[i] != ptr[i])
		{
			return false;
		}
	}

	m_matches.emplace_back(ptr);

	return true;
}

#if PATTERNS_USE_HINTS
void pattern::hint(uint64_t hash, uintptr_t address)
{
	auto range = g_hints.equal_range(hash);

	for (auto it = range.first; it != range.second; it++)
	{
		if (it->second == address)
		{
			return;
		}
	}

	g_hints.insert(std::make_pair(hash, address));
}
#endif
}