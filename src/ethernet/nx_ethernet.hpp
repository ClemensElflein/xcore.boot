//
// Created by clemens on 5/7/24.
//

#ifndef NX_ETHERNET_HPP
#define NX_ETHERNET_HPP

#include <nx_api.h>

#include <cstddef>
#ifdef __cplusplus
extern "C" {
#endif
// 1500 MTU + 14 for the ethernet header + 4 for CRC
constexpr size_t max_payload_size = 1518;
constexpr size_t pool_size = max_payload_size * 50;
extern NX_IP ip;

void nx_ethernet_init();

void ETH_IRQHandler();

#ifdef __cplusplus
}
#endif

#endif  // NX_ETHERNET_HPP
