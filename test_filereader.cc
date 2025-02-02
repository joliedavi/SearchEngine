#include "./FileReader.h"

#include "gtest/gtest.h"
#include "./test_suite.h"

using std::string;

namespace hw4 {

TEST(Test_FileReader, TestFileReaderBasic) {
  HW4Environment::OpenTestCase();

  // See if we can read a file successfully.
  FileReader f(".", "test_files/hextext.txt");
  string contents;
  ASSERT_TRUE(f.ReadFile(&contents));
  ASSERT_EQ(4800U, contents.size());
  HW4Environment::AddPoints(10);

  // See if we can read a non text file
  // that contains the '\0' byte.
  f = FileReader(".", "test_files/transparent.gif");
  ASSERT_TRUE(f.ReadFile(&contents));
  ASSERT_EQ(43U, contents.size());
  HW4Environment::AddPoints(10);

  // Make sure reading a non-existent file fails.
  f = FileReader(".", "non-existent");
  ASSERT_FALSE(f.ReadFile(&contents));
  HW4Environment::AddPoints(5);

  // Another non-existant file, but
  // with a more complicated path
  f = FileReader("./libhw2", "./libhw2/../cpplint.py");
  ASSERT_FALSE(f.ReadFile(&contents));
  HW4Environment::AddPoints(5);
}

}  // namespace hw4
