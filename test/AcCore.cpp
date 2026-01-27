/**
 * Copyright 2026 Bruce Ide
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * Testing core functionality to make autocereal work
 */

#include <gtest/gtest.h>
#include <fr/autocereal/autocereal.h>

// Structure to test with. We could define this inside each
// individual test, but I'm fine with having it here.

  struct Pleh {
    int foo;
    std::string bar;
  };

TEST(AcCoreTests, ListSize) {
  constexpr size_t size = fr::autocereal::member_list_size<Pleh>();
  ASSERT_EQ(size, 2);
}

// We need an array of strings with the nonstatic names of the members
// of the class

TEST(AcCoreTests, StringArray) {

  constexpr auto namesArray = fr::autocereal::class_member_names<Pleh>();
  fr::autocereal::Stringify stringifier;
  auto names = stringifier.stringify(namesArray);
  ASSERT_EQ(names.size(), 2);
  ASSERT_EQ(names[0], "foo");
  ASSERT_EQ(names[1], "bar");
  // Congratulations. We've sucessfully hoisted constexpr data into
  // into run-time!
}

// We need to be able to retrieve a member reference by index
// and use it at runtime

TEST(AcCoreTests, MemberRef) {
  Pleh pleh{1, "PLEH!"};
  constexpr auto ref_info = fr::autocereal::member_info<Pleh, 1>();
  auto& ref = fr::autocereal::member_ref<Pleh, ref_info>(pleh);
  const auto &constRef = fr::autocereal::member_ref_const<Pleh, ref_info>(pleh);
  
  ASSERT_EQ(ref, "PLEH!");
  ASSERT_EQ(constRef, "PLEH!");
  // Now we have (basically) everything we need to implement autocereal.
}

