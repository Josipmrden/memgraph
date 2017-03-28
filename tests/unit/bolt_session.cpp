#include "bolt_common.hpp"

#include "communication/bolt/v1/encoder/result_stream.hpp"
#include "communication/bolt/v1/session.hpp"
#include "query/engine.hpp"

using ResultStreamT =
    communication::bolt::ResultStream<communication::bolt::Encoder<
        communication::bolt::ChunkedBuffer<TestSocket>>>;
using SessionT = communication::bolt::Session<TestSocket>;

/**
 * TODO (mferencevic): document
 */

const uint8_t handshake_req[] =
    "\x60\x60\xb0\x17\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00";
const uint8_t handshake_resp[] = "\x00\x00\x00\x01";
const uint8_t init_req[] =
    "\x00\x3f\xb2\x01\xd0\x15\x6c\x69\x62\x6e\x65\x6f\x34\x6a\x2d\x63\x6c\x69"
    "\x65\x6e\x74\x2f\x31\x2e\x32\x2e\x31\xa3\x86\x73\x63\x68\x65\x6d\x65\x85"
    "\x62\x61\x73\x69\x63\x89\x70\x72\x69\x6e\x63\x69\x70\x61\x6c\x80\x8b\x63"
    "\x72\x65\x64\x65\x6e\x74\x69\x61\x6c\x73\x80\x00\x00";
const uint8_t init_resp[] = "\x00\x03\xb1\x70\xa0\x00\x00";
const uint8_t run_req[] =
    "\x00\x26\xb2\x10\xd0\x21\x43\x52\x45\x41\x54\x45\x20\x28\x6e\x20\x7b\x6e"
    "\x61\x6d\x65\x3a\x20\x32\x39\x33\x38\x33\x7d\x29\x20\x52\x45\x54\x55\x52"
    "\x4e\x20\x6e\xa0\x00\x00";

TEST(Bolt, Session) {
  Dbms dbms;
  TestSocket socket(10);
  QueryEngine<ResultStreamT> query_engine;
  SessionT session(std::move(socket), dbms, query_engine);
  std::vector<uint8_t> &output = session.socket_.output;

  // execute handshake
  session.Execute(handshake_req, 20);
  ASSERT_EQ(session.state_, communication::bolt::INIT);
  PrintOutput(output);
  CheckOutput(output, handshake_resp, 4);

  // execute init
  session.Execute(init_req, 67);
  ASSERT_EQ(session.state_, communication::bolt::EXECUTOR);
  PrintOutput(output);
  CheckOutput(output, init_resp, 7);

  // execute run
  session.Execute(run_req, 42);

  // TODO (mferencevic): query engine doesn't currently work,
  // we should test the query output here and the next state
  // ASSERT_EQ(session.state, bolt::EXECUTOR);
  // PrintOutput(output);
  // CheckOutput(output, run_resp, len);

  // TODO (mferencevic): add more tests

  session.Close();
}

int main(int argc, char **argv) {
  logging::init_sync();
  logging::log->pipe(std::make_unique<Stdout>());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
