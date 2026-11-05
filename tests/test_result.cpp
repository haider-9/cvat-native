#include "cvatnative/core/Result.h"

#include <gtest/gtest.h>

using namespace cvat;

TEST(Result, HoldsValue)
{
    Result<int> result(42);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(*result, 42);
    EXPECT_EQ(result.value(), 42);
}

TEST(Result, HoldsError)
{
    Result<int> result(ApiError(ErrorKind::NotFound, QStringLiteral("missing"), 404));
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().kind, ErrorKind::NotFound);
    EXPECT_EQ(result.error().status, 404);
    EXPECT_TRUE(result.error().toString().contains(QStringLiteral("404")));
}

TEST(Result, RetryableClassification)
{
    EXPECT_TRUE(ApiError(ErrorKind::Network, QStringLiteral("network")).isRetryable());
    EXPECT_TRUE(ApiError(ErrorKind::Timeout, QStringLiteral("timeout")).isRetryable());
    EXPECT_TRUE(ApiError(ErrorKind::Http, QStringLiteral("busy"), 503).isRetryable());
    EXPECT_FALSE(ApiError(ErrorKind::Unauthorized, QStringLiteral("auth"), 401).isRetryable());
    EXPECT_FALSE(ApiError(ErrorKind::Parse, QStringLiteral("parse")).isRetryable());
}
