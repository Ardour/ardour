/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libpbd_int62_h__
#define __libpbd_int62_h__

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <exception>
#include <limits>

/* int62_t is a class the functions as a 62 bit signed integer complete with a flag that can be used to indicate a boolean property of
 * the object. The flag is stored inside the 64 bit integer used by the object (as a single bit), and all operations on the object that
 * change either the flag or the value are atomic.
 *
 * this was written to function as a base class for a timeline positional/distance type which needs to indicate whether it represents
 * audio time or musical time.
 */

class alignas(16) int62_t {
  protected:
	/* std::atomic<> takes care of memory barriers for us; the actual load and stores
	   are atomic on architectures that we're likely to care about.
	*/
	std::atomic<int64_t> v;

	/* this defines the bit used to indicate "flag" or not */
	static const int64_t flagbit_mask = (1LL<<62);

  protected:
	/* the "flagbit" follows 2's complement logic. It is "set" if the value is positive and the bit is 1; it is also set if the
	 * value is negative and bit is 0.
	*/
	static int64_t int62 (int64_t v) { if (v >= 0) { return v & ~flagbit_mask; } return (v | flagbit_mask); }
	static bool    flagged (int64_t v) { if (v >= 0) { return v & flagbit_mask; } return ((v & flagbit_mask) == 0); }

  public:
	/* this is really a private method but is useful to construct the int64_t value when building tests. It is static anyway, so
	   providing public access doesn't hurt.
	*/
	static int64_t build (bool flag, int64_t v) { if (v >= 0) { return (flag ? flagbit_mask : 0) | v; } return flag ? (v & ~flagbit_mask) : v; }

	int62_t () : v (0) {}
	int62_t  (bool bc, int64_t vc) : v (build (bc, vc)) {}
	int62_t (int62_t const & other) { v.store (other.v.load()); }

	static const int64_t max = 4611686018427387903; /* 2^62 - 1 */
	static const int64_t min = -2305843009213693952;

	bool    flagged() const { return flagged (v); }
	int64_t val() const { return int62(v); }

	int62_t& operator= (int64_t n) { v.store (build (flagged (v.load()), n)); return *this; }
	int62_t& operator= (int62_t const & other) { v.store (other.v.load()); return *this; }

	/* there's a pattern to many of these operators:

	   1) atomically load the current in64_t into "tmp". This value has
	      both the flag bit and the values bits of this int62_t.

	   2) constructor a new int62_t from
	        (a) is the flag bit set (using ::flagged (tmp))
	        (b) the result of applying the operator (plus arg) to the value
	            bits (obtained using ::int62 (tmp))

	   Note that we need to ensure that we're atomically determining both
	   the flag bit and values bit, hence the initial load into "tmp"
	   rather than two separate loads for each "part".
	*/

	int62_t operator- () const      { int64_t tmp = v; return int62_t (flagged (tmp), -int62(tmp)); }

	int62_t operator+ (int64_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) + n); }
	int62_t operator- (int64_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) - n); }
	int62_t operator* (int64_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) * n); }
	int62_t operator/ (int64_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) / n); }
	int62_t operator% (int64_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) % n); }

	int62_t operator+ (int62_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) + n.val()); }
	int62_t operator- (int62_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) - n.val()); }
	int62_t operator* (int62_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) * n.val()); }
	int62_t operator/ (int62_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) / n.val()); }
	int62_t operator% (int62_t n) const { int64_t tmp = v; return int62_t (flagged (tmp), int62 (tmp) % n.val()); }

	/* comparison operators .. will throw if the two objects have different
	 * flag settings (which is assumed to indicate that they differ in some
	 * important respect, and thus should not have their values compared)
	 */

	struct flag_mismatch : public std::exception {
		flag_mismatch () {}
		const char* what () const throw () { return "mismatched flags in int62_t"; }
	};

	bool operator< (int62_t const & other) const { if (flagged() != other.flagged()) throw flag_mismatch(); return val() < other.val(); }
	bool operator<= (int62_t const & other) const { if (flagged() != other.flagged()) throw flag_mismatch(); return val() <= other.val(); }
	bool operator> (int62_t const & other) const { if (flagged() != other.flagged()) throw flag_mismatch(); return val() > other.val(); }
	bool operator>= (int62_t const & other) const { if (flagged() != other.flagged()) throw flag_mismatch(); return val() >= other.val(); }

	/* don't throw flag_mismatch for explicit equality checks, since
	 * the semantics are well defined and the computation cost is trivial
	 */

	bool operator!= (int62_t const & other) const { if (flagged() != other.flagged()) return false; return val() != other.val(); }
	bool operator== (int62_t const & other) const { if (flagged() != other.flagged()) return true; return val() == other.val(); }

	explicit operator int64_t() const { return int62(v); }

	bool operator< (int64_t n) const { return val() < n; }
	bool operator<= (int64_t n) const { return val() <= n; }
	bool operator> (int64_t n) const { return val() > n; }
	bool operator>= (int64_t n) const { return val() >= n; }
	bool operator!= (int64_t n) const { return val() != n; }
	bool operator== (int64_t n) const { return val() == n; }

	int62_t abs() const { int64_t tmp = v; return int62_t (flagged(tmp), ::abs(int62(tmp))); }

	int62_t& operator+= (int64_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) + n);
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator-= (int64_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) - n);
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator*= (int64_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) * n);
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator/= (int64_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) / n);
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator%= (int64_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) % n);
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}

	int62_t& operator+= (int62_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) + n.val());
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator-= (int62_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) - n.val());
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator*= (int62_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) * n.val());
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator/= (int62_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t newval = build (flagged (oldval), int62 (oldval) / n.val());
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}
	int62_t& operator%= (int62_t n) {
		while (1) {
			int64_t oldval = v.load (std::memory_order_relaxed);
			int64_t  newval = build (flagged (oldval), int62 (oldval) % n.val());
			if (v.compare_exchange_weak (oldval, newval)) {
				break;
			}
		}
		return *this;
	}

};

namespace std {
	template<>
	struct numeric_limits<int62_t> {
		static int62_t min() { return int62_t (false, -2305843009213693952); }
		static int62_t max() { return int62_t (false, 4611686018427387904); }
	};
}

#endif /* __libpbd_int62_h__ */
