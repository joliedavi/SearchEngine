#ifndef HW4_TEST_SUITE_H_
#define HW4_TEST_SUITE_H_

#include "gtest/gtest.h"

class HW4Environment : public ::testing::Environment {
 public:
  static void AddPoints(int points);
  static void OpenTestCase();

  // These are run once for the entire test environment:
  virtual void SetUp();
  virtual void TearDown();

 private:
  static constexpr int HW4_MAXPOINTS = 250;
  static int total_points_;
  static int curr_test_points_;
};


#endif  // HW4_TEST_SUITE_H_
