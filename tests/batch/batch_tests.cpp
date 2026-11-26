#include <catch2/catch_test_macros.hpp>
#include "batch.h"

TEST_CASE("batch stub links", "[batch]") { REQUIRE(lumora::batch::batch_status_text(lumora::batch::BatchStatus::Ok) != nullptr); }
