#pragma once

// the flag is only used in hardcoded queries compilation
//     see usage in plan_compiler.hpp
#ifndef HARDCODED_OUTPUT_STREAM
#include "communication/bolt/v1/encoder/result_stream.hpp"
#include "io/network/socket.hpp"
using Stream = communication::bolt::ResultStream<communication::bolt::Encoder<
    communication::bolt::ChunkedBuffer<io::network::Socket>>>;
#else
#include "../stream/print_record_stream.hpp"
using Stream = PrintRecordStream;
#endif
#include "data_structures/bitset/static_bitset.hpp"
