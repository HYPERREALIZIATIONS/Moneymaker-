#pragma once

#include <cstdint>
#include <vector>

namespace mm::algo {

// Build the KawPow/Ethash DAG (array of 128-byte items) for an epoch using
// the accessible Ethash light cache. Used by the CUDA and OpenCL backends to
// upload the dataset to the device. (Reference implementation - validate
// against Ethash/KawPow test vectors.)
void build_kawpow_dag(uint64_t epoch, std::vector<uint8_t>& out_dag, uint32_t& out_items);

} // namespace mm::algo
