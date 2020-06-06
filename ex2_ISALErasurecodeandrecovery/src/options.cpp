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


#include "options.h"
#include "size.h"
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <iostream>

options options::parse(int argc, char* argv[])
{
    using namespace boost::program_options;
    using std::vector;
    using std::string;
    using std::cout;

    options_description desc(
        "Usage: ./ex2 [--help] [--data-size <size>] [--buffer-count <n>] [--lost-buffers <n>]");

    desc.add_options()("help", "display this message");
    desc.add_options()(
        "data-size",
        value<string>()->value_name("size")->default_value("256MiB"),
        "the amount of data to be distributed among the buffers (size <= 256MiB)");
    desc.add_options()(
        "buffer-count",
        value<int>()->value_name("n")->default_value(24),
        "the number of buffers to distribute the data across (n <= 24)");
    desc.add_options()(
        "lost-buffers",
        value<int>()->value_name("n")->default_value(2),
        "the number of buffer that will get inaccessible (n <= 2)");

    variables_map vm;

    try
    {
        store(parse_command_line(argc, argv, desc), vm);
    }
    catch (unknown_option&)
    {
    }

    notify(vm);

    if (vm.count("help"))
    {
        cout << desc << "\n";
        exit(0);
    }

    uint64_t dataset_size = vm.count("data-size")
                                ? std::max(size::from_string(vm["data-size"].as<string>()), 22ul)
                                : 268435456ul;
    int lost_buffers = vm.count("lost-buffers") ? std::max(vm["lost-buffers"].as<int>(), 1) : 2;
    int buffer_count =
        vm.count("buffer-count") ? std::max(vm["buffer-count"].as<int>(), lost_buffers + 1) : 24;
    uint64_t data_per_buffer = dataset_size / (buffer_count - lost_buffers);

    if (buffer_count > 24 || lost_buffers > 2 || data_per_buffer < 1)
    {
        cout << desc << "\n";
        exit(-1);
    }

    return options{(buffer_count - lost_buffers) * data_per_buffer, buffer_count, lost_buffers};
}
