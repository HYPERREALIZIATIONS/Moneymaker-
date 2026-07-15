#include "opencl_backend.h"

#include <CL/cl.h>
#include <ethash/ethash.hpp>

#include <vector>

#include "../../algo/kawpow_dag.h"
#include "../../common/log.h"
#include "../../common/util.h"

using namespace mm;
using namespace mm::backend::opencl;

static const char* kKernelSource = R"CLC(
#define PROGPOW_LANES 16
#define PROGPOW_REGS 32
#define PROGPOW_DAG_BYTES 128
#define PROGPOW_CNT_DAG 64

uint fnv1a(uint a, uint d) { return (a ^ d) * 0x01000193u; }

// Minimal Keccak-256 (reference; validate before production).
void keccak256(__global const uchar* in, int len, uchar out[32]) {
    // NOTE: full Keccak-f[1600] implementation required here. This stub is a
    // placeholder and MUST be replaced with a correct Keccak implementation.
    for (int i = 0; i < 32; ++i) out[i] = 0;
    (void)in; (void)len;
}

__kernel void kawpow_search(
    __global const uchar* header, int header_len,
    ulong start_nonce, uint count,
    __global const uchar* dag, uint dag_items,
    __global const uchar* target,
    __global uint* result_found,
    __global ulong* result_nonce,
    __global uchar* result_hash)
{
    uint tid = get_global_id(0);
    if (tid >= count) return;
    ulong nonce = start_nonce + tid;

    uchar hbuf[128];
    for (int i = 0; i < header_len && i < 128; ++i) hbuf[i] = header[i];
    for (int i = 0; i < 8; ++i) hbuf[header_len - 8 + i] = (uchar)(nonce >> (8*i));

    uchar seed[32];
    keccak256(hbuf, header_len, seed);
    uint s = seed[0] | (seed[1] << 8) | (seed[2] << 16) | (seed[3] << 24);

    uint mix[PROGPOW_LANES * PROGPOW_REGS];
    for (uint i = 0; i < PROGPOW_LANES * PROGPOW_REGS; ++i) mix[i] = fnv1a(i, s);

    for (uint l = 0; l < PROGPOW_CNT_DAG; ++l) {
        uint p = fnv1a(s ^ l, l) % dag_items;
        __global const uchar* item = dag + (ulong)p * 2 * PROGPOW_DAG_BYTES;
        for (uint r = 0; r < PROGPOW_REGS; ++r) {
            uint boff = ((r % 32) * 4) & 255;
            uint dw = item[boff] | (item[boff+1] << 8) | (item[boff+2] << 16) | (item[boff+3] << 24);
            uint mw = mix[(r % PROGPOW_LANES) * PROGPOW_REGS + (r / PROGPOW_LANES)];
            uint math = (mw + dw); // simplified math
            mix[(r % PROGPOW_LANES) * PROGPOW_REGS + (r / PROGPOW_LANES)] = fnv1a(mw, math);
        }
    }

    uchar hash[32];
    keccak256(seed, 32, hash); // placeholder final hash

    bool ok = true;
    for (int i = 0; i < 8; ++i) {
        if (hash[i] > target[i]) { ok = false; break; }
        if (hash[i] < target[i]) break;
    }
    if (ok) {
        result_found[tid] = 1;
        result_nonce[tid] = nonce;
        for (int i = 0; i < 32; ++i) result_hash[tid*32 + i] = hash[i];
    }
}
)CLC";

OpenClBackend::OpenClBackend(int device, Algo algo) : device_(device), algo_(algo) {
    name_ = "opencl:" + std::to_string(device);
}

OpenClBackend::~OpenClBackend() { stop(); }

bool OpenClBackend::init() {
    if (algo_ != Algo::KawPow) {
        log_warn(std::string(name_) + ": RandomX/GhostRider are CPU-only; OpenCL backend idle");
        return true;
    }
    cl_uint num_platforms = 0;
    clGetPlatformIDs(0, nullptr, &num_platforms);
    if (num_platforms == 0) { log_error(name_ + ": no OpenCL platforms"); return false; }
    std::vector<cl_platform_id> platforms(num_platforms);
    clGetPlatformIDs(num_platforms, platforms.data(), nullptr);

    cl_platform_id plat = platforms[0];
    // Pick the first platform whose vendor matches the configured hint
    // ("amd", "nvidia", "intel"); otherwise use the first platform.
    for (auto p : platforms) {
        char vendor[128] = {0};
        clGetPlatformInfo(p, CL_PLATFORM_VENDOR, sizeof(vendor), vendor, nullptr);
        std::string v(vendor);
        bool match = common::iequals(v.substr(0,3), "amd") ||
                     common::iequals(v.substr(0,6), "nvidia") ||
                     common::iequals(v.substr(0,5), "intel");
        (void)match;
        plat = p; break; // first platform; refine by vendor hint in production
    }

    cl_uint num_devs = 0;
    clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devs);
    if (num_devs == 0) { log_error(name_ + ": no OpenCL GPU devices"); return false; }
    std::vector<cl_device_id> devs(num_devs);
    clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, num_devs, devs.data(), nullptr);
    cl_device_id dev = devs[device_ < (int)num_devs ? device_ : 0];

    cl_int err = 0;
    cl_ctx_ = clCreateContext(nullptr, 1, &dev, nullptr, nullptr, &err);
    cl_queue_ = clCreateCommandQueue((cl_context)cl_ctx_, dev, 0, &err);

    cl_program_ = clCreateProgramWithSource((cl_context)cl_ctx_, 1, &kKernelSource, nullptr, &err);
    clBuildProgram((cl_program)cl_program_, 1, &dev, nullptr, nullptr, nullptr);
    cl_kernel_ = clCreateKernel((cl_program)cl_program_, "kawpow_search", &err);

    log_info(std::string(name_) + ": OpenCL device initialized");
    return err == CL_SUCCESS;
}

void OpenClBackend::set_job(const core::Job& job) {
    if (algo_ != Algo::KawPow) return;
    job_ = job;
    have_job_ = true;

    uint64_t epoch = ethash::get_epoch_number(static_cast<int>(job.height));
    if (epoch != current_epoch_) {
        std::vector<uint8_t> dag;
        uint32_t items = 0;
        algo::build_kawpow_dag(epoch, dag, items);
        if (d_dag_) clReleaseMemObject((cl_mem)d_dag_);
        cl_int err = 0;
        d_dag_ = clCreateBuffer((cl_context)cl_ctx_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                dag.size(), dag.data(), &err);
        dag_items_ = items;
        current_epoch_ = epoch;
        log_info(std::string(name_) + ": built KawPow DAG (" + std::to_string(items) + " items)");
    }
}

void OpenClBackend::start() {
    if (algo_ != Algo::KawPow || running_) return;
    running_ = true;
    thread_ = std::thread([this]() { worker(); });
}

void OpenClBackend::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void OpenClBackend::worker() {
    const uint32_t batch = 1u << 20;
    cl_context ctx = (cl_context)cl_ctx_;
    cl_command_queue q = (cl_command_queue)cl_queue_;
    cl_kernel k = (cl_kernel)cl_kernel_;

    cl_mem d_header = clCreateBuffer(ctx, CL_MEM_READ_ONLY, 128, nullptr, nullptr);
    cl_mem d_target = clCreateBuffer(ctx, CL_MEM_READ_ONLY, 32, nullptr, nullptr);
    cl_mem d_found = clCreateBuffer(ctx, CL_MEM_READ_WRITE, batch * sizeof(cl_uint), nullptr, nullptr);
    cl_mem d_nonce = clCreateBuffer(ctx, CL_MEM_READ_WRITE, batch * sizeof(cl_ulong), nullptr, nullptr);
    cl_mem d_hash = clCreateBuffer(ctx, CL_MEM_READ_WRITE, batch * 32, nullptr, nullptr);

    std::vector<cl_uint> h_found(batch);
    std::vector<cl_ulong> h_nonce(batch);
    std::vector<uint8_t> h_hash(batch * 32);

    while (running_ && have_job_) {
        std::vector<uint8_t> hdr = job_.blob;
        clEnqueueWriteBuffer(q, d_header, CL_TRUE, 0, hdr.size(), hdr.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(q, d_target, CL_TRUE, 0, 32, job_.target.data(), 0, nullptr, nullptr);

        cl_ulong start = nonce_base_;
        nonce_base_ += batch;

        clSetKernelArg(k, 0, sizeof(cl_mem), &d_header);
        clSetKernelArg(k, 1, sizeof(int), &hdr.size());
        clSetKernelArg(k, 2, sizeof(cl_ulong), &start);
        cl_uint cnt = batch; clSetKernelArg(k, 3, sizeof(cl_uint), &cnt);
        clSetKernelArg(k, 4, sizeof(cl_mem), &d_dag_);
        clSetKernelArg(k, 5, sizeof(cl_uint), &dag_items_);
        clSetKernelArg(k, 6, sizeof(cl_mem), &d_target);
        clSetKernelArg(k, 7, sizeof(cl_mem), &d_found);
        clSetKernelArg(k, 8, sizeof(cl_mem), &d_nonce);
        clSetKernelArg(k, 9, sizeof(cl_mem), &d_hash);

        size_t global = batch;
        clEnqueueNDRangeKernel(q, k, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);

        clEnqueueReadBuffer(q, d_found, CL_TRUE, 0, batch * sizeof(cl_uint), h_found.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(q, d_nonce, CL_TRUE, 0, batch * sizeof(cl_ulong), h_nonce.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(q, d_hash, CL_TRUE, 0, batch * 32, h_hash.data(), 0, nullptr, nullptr);

        for (uint32_t i = 0; i < batch; ++i) {
            if (h_found[i]) {
                core::Share s;
                s.job_id = job_.job_id;
                s.nonce = h_nonce[i];
                s.hash.assign(h_hash.begin() + i * 32, h_hash.begin() + i * 32 + 32);
                emit_share(s);
            }
        }
        tracker_.add(batch);
    }

    clReleaseMemObject(d_header); clReleaseMemObject(d_target);
    clReleaseMemObject(d_found); clReleaseMemObject(d_nonce); clReleaseMemObject(d_hash);
}
