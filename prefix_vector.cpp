#include "prefix_vector.hpp"

#include <cstring>

namespace bigendian {
	bool operator==(bitstring const& a, bitstring const& b) {
		if (a.length() != b.length()) return false;
		size_t const full_bytes = a.length() / 8;
		if (0 != full_bytes && 0 != std::memcmp(a.byte_data(), b.byte_data(), full_bytes)) return false;
		return a.fraction_byte() == b.fraction_byte();
	}

	bool operator!=(bitstring const& a, bitstring const& b) {
		return !(a == b);
	}

	bool is_lexicographic_less(bitstring const& a, bitstring const& b) {
		size_t const min_len = std::min(a.length(), b.length());
		bitstring const a_trunc = a.truncate(min_len);
		bitstring const b_trunc = b.truncate(min_len);

		size_t const full_bytes = min_len / 8;
		if (0 != full_bytes) {
			int cmp = std::memcmp(a_trunc.byte_data(), b_trunc.byte_data(), full_bytes);
			if (0 != cmp) return cmp < 0;
		}
		if (a_trunc.fraction_byte() != b_trunc.fraction_byte()) {
			return a_trunc.fraction_byte() < b_trunc.fraction_byte();
		}
		// the one with shorter length is prefix of the other
		return a.length() < b.length();
	}

	bool is_tree_less(bitstring const& a, bitstring const& b) {
		size_t const min_len = std::min(a.length(), b.length());
		bitstring const a_trunc = a.truncate(min_len);
		bitstring const b_trunc = b.truncate(min_len);

		size_t const full_bytes = min_len / 8;
		if (0 != full_bytes) {
			int cmp = std::memcmp(a_trunc.byte_data(), b_trunc.byte_data(), full_bytes);
			if (0 != cmp) return cmp < 0;
		}
		if (a_trunc.fraction_byte() != b_trunc.fraction_byte()) {
			return a_trunc.fraction_byte() < b_trunc.fraction_byte();
		}
		// the one with shorter length is prefix of the other
		if (a.length() < b.length()) { // a is prefix of b
			// if 0 == b.get_bit(min_len + 1) the b hangs is on the left of ancestor a
			return 0 == b.get_bit(min_len + 1);
		} else if (a.length() > b.length()) { // b is prefix of a
			return 0 == a.get_bit(min_len + 1);
		} else {
			return false; // equal
		}
	}

	bool is_prefix(bitstring const& prefix, bitstring const& str) {
		if (str.length() < prefix.length()) return false;
		bitstring const str_trunc = str.truncate(prefix.length());
		return prefix == str_trunc;
	}

	bitstring longest_common_prefix(bitstring const& a, bitstring const& b) {
		size_t const min_len = std::min(a.length(), b.length());
		size_t common = 0;
		for (size_t i = 0; i < min_len / 8; ++i, common += 8) {
			if (a.get_byte(i) != b.get_byte(i)) break;
		}
		while (common < min_len && a.get_bit(common) == b.get_bit(common)) ++common;
		return a.truncate(common);
	}
}
