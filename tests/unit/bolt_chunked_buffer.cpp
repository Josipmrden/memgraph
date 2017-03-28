#include "bolt_common.hpp"
#include "communication/bolt/v1/encoder/chunked_buffer.hpp"

// aliases
using SocketT = TestSocket;
using BufferT = communication::bolt::ChunkedBuffer<SocketT>;

// "alias" constants
static constexpr auto CHS = communication::bolt::CHUNK_HEADER_SIZE;
static constexpr auto CEMS = communication::bolt::CHUNK_END_MARKER_SIZE;
static constexpr auto MCS = communication::bolt::MAX_CHUNK_SIZE;
static constexpr auto WCS = communication::bolt::WHOLE_CHUNK_SIZE;

/**
 * Verifies a single chunk. The chunk should be constructed from header
 * (chunk size), data and end marker. The header is two bytes long number
 * written in big endian format. Data is array of elements which max size is
 * 0xFFFF. The end marker is always two bytes long array of two zeros.
 *
 * @param data pointer on data array (array of bytes)
 * @param size of data array
 * @param element expected element in all positions of chunk data array
 *        (all data bytes in tested chunk should be equal to element)
 */
void VerifyChunkOfOnes(uint8_t *data, int size, uint8_t element) {
  // first two bytes are size (big endian)
  uint8_t lower_byte = size & 0xFF;
  uint8_t higher_byte = (size & 0xFF00) >> 8;
  ASSERT_EQ(*data, higher_byte);
  ASSERT_EQ(*(data + 1), lower_byte);

  // in the data array should be size number of ones
  // the header is skipped
  for (auto i = CHS; i < size + CHS; ++i) {
    ASSERT_EQ(*(data + i), element);
  }

  // last two bytes should be zeros
  // next to header and data
  ASSERT_EQ(*(data + CHS + size), 0x00);
  ASSERT_EQ(*(data + CHS + size + 1), 0x00);
}

TEST(BoltChunkedBuffer, OneSmallChunk) {
  // initialize array of 100 ones (small chunk)
  int size = 100;
  uint8_t element = '1';
  std::vector<uint8_t> data(100, element);

  // initialize tested buffer
  SocketT socket(10);
  BufferT buffer(socket);

  // write into buffer
  buffer.Write(data.data(), size);
  buffer.Flush();

  // check the output array
  // the array should look like: [0, 100, 1, 1, ... , 1, 0, 0]
  VerifyChunkOfOnes(socket.output.data(), size, element);
}

TEST(BoltChunkedBuffer, TwoSmallChunks) {
  // initialize the small arrays
  int size1 = 100;
  uint8_t element1 = '1';
  std::vector<uint8_t> data1(size1, element1);
  int size2 = 200;
  uint8_t element2 = '2';
  std::vector<uint8_t> data2(size2, element2);

  // initialize tested buffer
  SocketT socket(10);
  BufferT buffer(socket);

  // write into buffer
  buffer.Write(data1.data(), size1);
  buffer.Chunk();
  buffer.Write(data2.data(), size2);
  buffer.Flush();

  // check the output array
  // the output array should look like this: [0, 100, 1, 1, ... , 1, 0, 0] +
  //                                         [0, 100, 2, 2, ...... , 2, 0, 0]
  auto data = socket.output.data();
  VerifyChunkOfOnes(data, size1, element1);
  VerifyChunkOfOnes(data + CHS + size1 + CEMS, size2, element2);
}

TEST(BoltChunkedBuffer, OneAndAHalfOfMaxChunk) {
  // initialize a big chunk
  int size = 100000;
  uint8_t element = '1';
  std::vector<uint8_t> data(size, element);

  // initialize tested buffer
  SocketT socket(10);
  BufferT buffer(socket);

  // write into buffer
  buffer.Write(data.data(), size);
  buffer.Flush();

  // check the output array
  // the output array should look like this:
  // [0xFF, 0xFF, 1, 1, ... , 1, 0, 0, 0x86, 0xA1, 1, 1, ... , 1, 0, 0]
  auto output = socket.output.data();
  VerifyChunkOfOnes(output, MCS, element);
  VerifyChunkOfOnes(output + WCS, size - MCS, element);
}

int main(int argc, char **argv) {
  logging::init_sync();
  logging::log->pipe(std::make_unique<Stdout>());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
