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
 */

#include <gtest/gtest.h>
#include <fr/autocereal/autocereal.h>
#include <sstream>
#include <string>

  struct ToFromTestStruct {
    std::string message;
  };

TEST(ToFromFunctions, GenericInputOutput) {

  ToFromTestStruct hello{"Hello, World!"};
  ToFromTestStruct copy;
  std::string outputStr;
  std::stringstream stream;
  {
    // As usual, it's best to do this in its own scope
    // so ar can clean up with its destructor.
    cereal::JSONOutputArchive ar(stream);
    fr::autocereal::to_output_archive(cereal::make_nvp("hello", hello), ar);
  }

  outputStr = stream.str();
  
  /**
   * outputStr will look like:
   *
   * {
   *     "hello": {
   *      "message": "Hello, World!"
   *  }
   * }
   *
   * You DO have to use cereal::make_nvp if you want to name your
   * variable like that, otherwise you get a generic "value0"
   * instead. I'm OK with that, which is good, since reflection
   * sensibly doesn't provide a way to get the original name of a
   * variable you pass into a function.
   *
   */
  
  ASSERT_NE(outputStr.find(hello.message), std::string::npos);

  {
    cereal::JSONInputArchive ar(stream);
    fr::autocereal::from_input_archive(copy, ar);
  }

  ASSERT_EQ(hello.message, copy.message);
  
}

TEST(ToFromFunctions, JsonInputOutput) {
  ToFromTestStruct hello{"Hello World!"};
  ToFromTestStruct copy;
  std::stringstream stream;
  fr::autocereal::to_json(hello, stream);
  fr::autocereal::from_json(copy, stream);
  ASSERT_EQ(hello.message, copy.message);
}

TEST(ToFromFunctions, XmlInputOutput) {
  ToFromTestStruct hello{"Hello World!"};
  ToFromTestStruct copy;
  std::stringstream stream;
  fr::autocereal::to_xml(hello, stream);
  fr::autocereal::from_xml(copy, stream);
  ASSERT_EQ(hello.message, copy.message);
}

TEST(ToFromFunctions, StringJson) {
  ToFromTestStruct hello{"Hello World!"};
  ToFromTestStruct copy;

  auto json = fr::autocereal::to_json(cereal::make_nvp("hello", hello));
  std::cout << json << std::endl;
  fr::autocereal::from_json(copy, json);
  ASSERT_EQ(hello.message, copy.message);
}

TEST(ToFromFunctions, StringXml) {
  ToFromTestStruct hello{"Hello world!"};
  ToFromTestStruct copy;

  auto xml = fr::autocereal::to_xml(cereal::make_nvp("hello", hello));
  std::cout << xml << std::endl;

  fr::autocereal::from_xml(copy, xml);
  ASSERT_EQ(hello.message, copy.message);
}

// You can absolutely hand code these for config files and stuff

TEST(ToFromFunctions, HandCoded) {
  ToFromTestStruct hello;

  // The outer variable is pretty much ignored. I'm calling it "config" this time.
  fr::autocereal::from_json(hello, "{\"config\":{\"message\":\"JelloWorld!\"}}");
  ASSERT_EQ(hello.message, "JelloWorld!");
}


// Just print them out for you
TEST(ToFromFunctions, Output) {
  ToFromTestStruct hello{"Hello, World!"};

  fr::autocereal::to_json(cereal::make_nvp("JsonVersion", hello), std::cout);
  std::cout << std::endl;

  fr::autocereal::to_xml(cereal::make_nvp("XmlVersion", hello), std::cout);
  std::cout << std::endl; 
}

