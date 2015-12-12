#include "prefix_vector.hpp"

#include <iostream>

#include <netinet/ip.h>
#include <netinet/ip6.h>

struct cidr_ipv4 {
	uint32_t addr;
	uint8_t prefix;
};

struct cidr_ipv4_get_bit_string {
	typedef bigendian::bitstring bitstring;

	bigendian::bitstring operator()(cidr_ipv4 const& value) {
		return bigendian::bitstring(&value.addr, size_t{value.prefix});
	}
};

template class prefix_vector<cidr_ipv4, uint32_t, cidr_ipv4_get_bit_string>;

int main() {
	prefix_vector<cidr_ipv4, uint32_t, cidr_ipv4_get_bit_string> routing_table;
	cidr_ipv4 any{0, 0};
	cidr_ipv4 loopback_net{ htonl(INADDR_LOOPBACK), 8 };
	cidr_ipv4 loopback{ htonl(INADDR_LOOPBACK), 32 };
	cidr_ipv4 null{0, 32};

	routing_table.insert_or_assign(any, 20);
	routing_table.insert_or_assign(loopback_net, 10);

	std::cout << routing_table.find(any)->value() << "\n";
	std::cout << routing_table.find(loopback_net)->value() << "\n";
	std::cout << routing_table.find(loopback)->value() << "\n";
	std::cout << routing_table.find(null)->value() << "\n";

	for (auto const& elem: routing_table.subkeys(loopback_net)) {
		std::cout << "subkey: " << elem.value() << "\n";
	}

	decltype(routing_table) const& const_routing_table{routing_table};

	std::cout << const_routing_table.find(any)->value() << "\n";
	std::cout << const_routing_table.find(loopback_net)->value() << "\n";
	std::cout << const_routing_table.find(loopback)->value() << "\n";
	std::cout << const_routing_table.find(null)->value() << "\n";

	for (auto const& elem: const_routing_table.subkeys(loopback_net)) {
		std::cout << "subkey: " << elem.value() << "\n";
	}

	using std::swap;
	decltype(routing_table) other_routing_table;
	swap(routing_table, other_routing_table);

	return 0;
}
