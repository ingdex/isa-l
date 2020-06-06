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


#include "utils.hpp"
#include "options.h"
#include "size.h"
#include <iostream>
#include <isa-l.h>

std::string utils::duration_to_string(std::chrono::nanoseconds duration)
{
    using namespace std::chrono_literals;

    if (duration > 100000us)
        return std::to_string(
                   std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()) +
               " ms";
    else if (duration > 100000ns)
        return std::to_string(
                   std::chrono::duration_cast<std::chrono::microseconds>(duration).count()) +
               " us";
    else
        return std::to_string(duration.count()) + " ns";
}

std::string utils::format_size(uint64_t size)
{
    std::string out = size::to_string(size);
    if (size >= 1000)
        out += " (" + size::to_string(size, false) + ")";
    return out;
}

void utils::display_info(const options& options)
{
    std::cout << "[Info   ] Using isa-l                  " << ISAL_MAJOR_VERSION << "."
              << ISAL_MINOR_VERSION << "." << ISAL_PATCH_VERSION << "\n";
    std::cout << "[Info   ] Dataset size:                " << format_size(options.dataset_size)
              << "\n";
    std::cout << "[Info   ] Number of buffers:           " << options.buffer_count << "\n";
    std::cout << "[Info   ] Number of lost buffers:      " << options.lost_buffers << "\n";
    int data_per_buffer = options.dataset_size / (options.buffer_count - options.lost_buffers);
    std::cout << "[Info   ] Error correction codes size: "
              << format_size(data_per_buffer * options.lost_buffers) << "\n";
    std::cout << "[Info   ] Buffer size:                 " << format_size(data_per_buffer) << "\n";
}
