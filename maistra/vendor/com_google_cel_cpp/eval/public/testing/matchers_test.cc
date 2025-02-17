#include "eval/public/testing/matchers.h"

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace test {
namespace {

using testing::DoubleEq;
using testing::DoubleNear;
using testing::Gt;
using testing::Lt;
using testing::Not;
using testutil::EqualsProto;

TEST(IsCelValue, EqualitySmoketest) {
  EXPECT_THAT(CelValue::CreateBool(true),
              EqualsCelValue(CelValue::CreateBool(true)));
  EXPECT_THAT(CelValue::CreateInt64(-1),
              EqualsCelValue(CelValue::CreateInt64(-1)));
  EXPECT_THAT(CelValue::CreateUint64(2),
              EqualsCelValue(CelValue::CreateUint64(2)));
  EXPECT_THAT(CelValue::CreateDouble(1.25),
              EqualsCelValue(CelValue::CreateDouble(1.25)));
  EXPECT_THAT(CelValue::CreateStringView("abc"),
              EqualsCelValue(CelValue::CreateStringView("abc")));
  EXPECT_THAT(CelValue::CreateBytesView("def"),
              EqualsCelValue(CelValue::CreateBytesView("def")));
  EXPECT_THAT(CelValue::CreateDuration(absl::Seconds(2)),
              EqualsCelValue(CelValue::CreateDuration(absl::Seconds(2))));
  EXPECT_THAT(
      CelValue::CreateTimestamp(absl::FromUnixSeconds(1)),
      EqualsCelValue(CelValue::CreateTimestamp(absl::FromUnixSeconds(1))));

  EXPECT_THAT(CelValue::CreateInt64(-1),
              Not(EqualsCelValue(CelValue::CreateBool(true))));
  EXPECT_THAT(CelValue::CreateUint64(2),
              Not(EqualsCelValue(CelValue::CreateInt64(-1))));
  EXPECT_THAT(CelValue::CreateDouble(1.25),
              Not(EqualsCelValue(CelValue::CreateUint64(2))));
  EXPECT_THAT(CelValue::CreateStringView("abc"),
              Not(EqualsCelValue(CelValue::CreateDouble(1.25))));
  EXPECT_THAT(CelValue::CreateBytesView("def"),
              Not(EqualsCelValue(CelValue::CreateStringView("abc"))));
  EXPECT_THAT(CelValue::CreateDuration(absl::Seconds(2)),
              Not(EqualsCelValue(CelValue::CreateBytesView("def"))));
  EXPECT_THAT(CelValue::CreateTimestamp(absl::FromUnixSeconds(1)),
              Not(EqualsCelValue(CelValue::CreateDuration(absl::Seconds(2)))));
  EXPECT_THAT(
      CelValue::CreateBool(true),
      Not(EqualsCelValue(CelValue::CreateTimestamp(absl::FromUnixSeconds(1)))));
}

TEST(PrimitiveMatchers, Smoketest) {
  EXPECT_THAT(CelValue::CreateBool(true), IsCelBool(true));
  EXPECT_THAT(CelValue::CreateBool(false), IsCelBool(Not(true)));

  EXPECT_THAT(CelValue::CreateInt64(1), IsCelInt64(1));
  EXPECT_THAT(CelValue::CreateInt64(-1), IsCelInt64(Not(Gt(0))));

  EXPECT_THAT(CelValue::CreateUint64(1), IsCelUint64(1));
  EXPECT_THAT(CelValue::CreateUint64(2), IsCelUint64(Not(Lt(2))));

  EXPECT_THAT(CelValue::CreateDouble(1.5), IsCelDouble(DoubleEq(1.5)));
  EXPECT_THAT(CelValue::CreateDouble(1.0 + 0.8),
              IsCelDouble(DoubleNear(1.8, 1e-5)));

  EXPECT_THAT(CelValue::CreateStringView("abc"), IsCelString("abc"));
  EXPECT_THAT(CelValue::CreateStringView("abcdef"),
              IsCelString(testing::HasSubstr("def")));

  EXPECT_THAT(CelValue::CreateBytesView("abc"), IsCelBytes("abc"));
  EXPECT_THAT(CelValue::CreateBytesView("abcdef"),
              IsCelBytes(testing::HasSubstr("def")));

  EXPECT_THAT(CelValue::CreateDuration(absl::Seconds(2)),
              IsCelDuration(Lt(absl::Minutes(1))));

  EXPECT_THAT(CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
              IsCelTimestamp(Lt(absl::FromUnixSeconds(30))));
}

TEST(PrimitiveMatchers, WrongType) {
  EXPECT_THAT(CelValue::CreateBool(true), Not(IsCelInt64(1)));
  EXPECT_THAT(CelValue::CreateInt64(1), Not(IsCelUint64(1)));
  EXPECT_THAT(CelValue::CreateUint64(1), Not(IsCelDouble(1.0)));
  EXPECT_THAT(CelValue::CreateDouble(1.5), Not(IsCelString("abc")));
  EXPECT_THAT(CelValue::CreateStringView("abc"), Not(IsCelBytes("abc")));
  EXPECT_THAT(CelValue::CreateBytesView("abc"),
              Not(IsCelDuration(Lt(absl::Minutes(1)))));
  EXPECT_THAT(CelValue::CreateDuration(absl::Seconds(2)),
              Not(IsCelTimestamp(Lt(absl::FromUnixSeconds(30)))));
  EXPECT_THAT(CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
              Not(IsCelBool(true)));
}

TEST(SpecialMatchers, SmokeTest) {
  auto status = absl::InternalError("error");
  CelValue error = CelValue::CreateError(&status);
  EXPECT_THAT(error, IsCelError(testing::Eq(
                         absl::Status(absl::StatusCode::kInternal, "error"))));

  TestMessage proto_message;
  proto_message.add_bool_list(true);
  proto_message.add_bool_list(false);
  proto_message.add_int64_list(1);
  proto_message.add_int64_list(-1);
  CelValue message = CelProtoWrapper::CreateMessage(&proto_message, nullptr);
  EXPECT_THAT(message, IsCelMessage(EqualsProto(proto_message)));
}

}  // namespace
}  // namespace test
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
