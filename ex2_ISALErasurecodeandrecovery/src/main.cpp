/*
Copyright (c) <2017>, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "measurements.hpp"
#include "options.h"
#include "prealloc.h"
#include "random_number_generator.h"
#include "utils.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <isa-l.h>

// Create the source data buffers with random data, plus extra room for the error correction codes.
// We create a total of 'm' buffers of lenght 'len'.
// The 'k' first buffers contain the data.
// The 'm-k' remaining buffer are left uninitialized, and will store the error correction codes.
uint8_t** create_source_data(int m, int k, int len)
{
    uint8_t** sources = (uint8_t**)malloc(m * sizeof(uint8_t*));

    random_number_generator<uint8_t> data_generator;

    for (int i = 0; i < m; ++i)
    {
        sources[i] = (uint8_t*)malloc(len * sizeof(uint8_t));
        if (i < k)
        {
            int j = 0;
            for (; j < len / 64 * 64; j += 64)
            {
                memset(&sources[i][j], data_generator.get(), 64);
            }
            for (; j < len; ++j)
            {
                sources[i][j] = (uint8_t)data_generator.get();
            }
        }
    }
    return sources;
}

// Create the error correction codes, and store them alongside the data.
std::vector<uint8_t> encode_data(int m, int k, uint8_t** sources, int len, prealloc_encode prealloc)
{
    // Generate encode matrix
    gf_gen_cauchy1_matrix(prealloc.encode_matrix.data(), m, k);

    // Generates the expanded tables needed for fast encoding
    ec_init_tables(k, m - k, &prealloc.encode_matrix[k * k], prealloc.table.data());

    // Actually generated the error correction codes
    ec_encode_data(
        len, k, m - k, prealloc.table.data(), (uint8_t**)sources, (uint8_t**)&sources[k]);

    return prealloc.encode_matrix;
}

// We randomly choose up to 2 buffers to "lose", and return the indexes of those buffers.
// Note that we can lose both part of the data or part of the error correction codes indifferently.
std::vector<int> generate_errors(int m, int errors_count)
{
    random_number_generator<int> idx_generator(0, m - 1);
    std::vector<int>             errors{idx_generator.get(), 0};
    if (errors_count == 2)
    {
        do
        {
            errors[1] = idx_generator.get();
        } while (errors[1] == errors[0]);
        std::sort(errors.begin(), errors.end());
    }
    return errors;
}

// We arrange a new array of buffers that skip the ones we "lost"
uint8_t** create_erroneous_data(int k, uint8_t** source_data, std::vector<int> errors)
{
    uint8_t** erroneous_data;
    erroneous_data = (uint8_t**)malloc(k * sizeof(uint8_t*));

    for (int i = 0, r = 0; i < k; ++i, ++r)
    {
        while (std::find(errors.cbegin(), errors.cend(), r) != errors.cend())
            ++r;
        for (int j = 0; j < k; j++)
        {
            erroneous_data[i] = source_data[r];
        }
    }
    return erroneous_data;
}

// Recover the contents of the "lost" buffers
// - m              : the total number of buffer, containint both the source data and the error
//                    correction codes
// - k              : the number of buffer that contain the source data
// - erroneous_data : the original buffers without the ones we "lost"
// - errors         : the indexes of the buffers we "lost"
// - encode_matrix  : the matrix used to generate the error correction codes
// - len            : the length (in bytes) of each buffer
// Return the recovered "lost" buffers
uint8_t** recover_data(
    int                         m,
    int                         k,
    uint8_t**                   erroneous_data,
    const std::vector<int>&     errors,
    const std::vector<uint8_t>& encode_matrix,
    int                         len,
    prealloc_recover            prealloc)
{
    for (int i = 0, r = 0; i < k; ++i, ++r)
    {
        while (std::find(errors.cbegin(), errors.cend(), r) != errors.cend())
            ++r;
        for (int j = 0; j < k; j++)
        {
            prealloc.errors_matrix[k * i + j] = encode_matrix[k * r + j];
        }
    }

    gf_invert_matrix(prealloc.errors_matrix.data(), prealloc.invert_matrix.data(), k);

    for (int e = 0; e < errors.size(); ++e)
    {
        int idx = errors[e];
        if (idx < k) // We lost one of the buffers containing the data
        {
            for (int j = 0; j < k; j++)
            {
                prealloc.decode_matrix[k * e + j] = prealloc.invert_matrix[k * idx + j];
            }
        }
        else // We lost one of the buffer containing the error correction codes
        {
            for (int i = 0; i < k; i++)
            {
                uint8_t s = 0;
                for (int j = 0; j < k; j++)
                    s ^= gf_mul(prealloc.invert_matrix[j * k + i], encode_matrix[k * idx + j]);
                prealloc.decode_matrix[k * e + i] = s;
            }
        }
    }

    ec_init_tables(k, m - k, prealloc.decode_matrix.data(), prealloc.table.data());
    ec_encode_data(len, k, (m - k), prealloc.table.data(), erroneous_data, prealloc.decoding);

    return prealloc.decoding;
}

// Performs 1 storage/recovery cycle, and returns the storage and recovery time.
// - m            : the total number of buffer, that will contain both the source data and the
//                  error correction codes
// - k            : the number of buffer that will contain the source data
// - len          : the length (in bytes) of each buffer
// - errors_count : the number of buffer to lose (must be equal to m-k)
measurements iteration(int m, int k, int len, int errors_count)
{
    uint8_t** source_data = create_source_data(m, k, len);

    prealloc_encode      prealloc_encode(m, k);
    auto                 start_storage = std::chrono::steady_clock::now();
    std::vector<uint8_t> encode_matrix =
        encode_data(m, k, source_data, len, std::move(prealloc_encode));
    auto end_storage = std::chrono::steady_clock::now();

    std::vector<int> errors         = generate_errors(m, errors_count);
    uint8_t**        erroneous_data = create_erroneous_data(k, source_data, errors);

    prealloc_recover prealloc_recover(m, k, errors.size(), len);
    auto             start_recovery = std::chrono::steady_clock::now();
    uint8_t**        decoding =
        recover_data(m, k, erroneous_data, errors, encode_matrix, len, std::move(prealloc_recover));
    auto end_recovery = std::chrono::steady_clock::now();

    bool success = false;
    for (int i = 0; i < errors.size(); ++i)
    {
        int ret = memcmp(source_data[errors[i]], decoding[i], len);
        success = (ret == 0);
    }

    free(erroneous_data);

    for (int i = 0; i < m; ++i)
    {
        free(source_data[i]);
    }
    free(source_data);

    for (int i = 0; i < errors_count; ++i)
    {
        free(decoding[i]);
    }
    free(decoding);

    if (!success)
        return {std::chrono::nanoseconds{0}, std::chrono::nanoseconds{0}};
    return {end_storage - start_storage, end_recovery - start_recovery};
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;

    options options = options::parse(argc, argv);

    utils::display_info(options);

    int m            = options.buffer_count;
    int k            = options.buffer_count - options.lost_buffers;
    int len          = options.dataset_size / (options.buffer_count - options.lost_buffers);
    int errors_count = options.lost_buffers;

    std::cout << "[Info   ] Perfoming benchmark...\n";
    std::cout << "[Info   ] 0 % done" << std::flush;

    measurements total_measurements;
    int          iterations = 0;
    auto         start_time = std::chrono::steady_clock::now();

    do
    {
        measurements new_measurement = iteration(m, k, len, errors_count);
        if (new_measurement.storage > 0s && new_measurement.recovery > 0s)
        {
            ++iterations;
            total_measurements += new_measurement;

            if (std::chrono::steady_clock::now() - start_time > 1s)
            {
                auto estimated_iterations =
                    std::min(10000l, (1s / (total_measurements.recovery / iterations)));
                auto estimated_runtime =
                    estimated_iterations *
                    ((std::chrono::steady_clock::now() - start_time) / iterations);
                if (estimated_runtime > 5s && iterations % (estimated_iterations / 10) == 0)
                    std::cout << "\r[Info   ] "
                              << (int)(((double)iterations / estimated_iterations) * 100)
                              << " % done" << std::flush;
            }
        }
    } while (iterations < 10000 &&
             (total_measurements.storage < 1s || total_measurements.recovery < 1s));

    std::cout << "\r[Info   ] 100 % done\n";
    if (iterations > 1)
        std::cout << "[Info   ] Average results over " << iterations << " iterations:\n";
    std::cout << "[Info   ] Storage time:                "
              << utils::duration_to_string(total_measurements.storage / iterations) << "\n";
    std::cout << "[Info   ] Recovery time:               "
              << utils::duration_to_string(total_measurements.recovery / iterations) << "\n";
}
