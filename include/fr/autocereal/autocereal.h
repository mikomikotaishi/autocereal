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

#pragma once

// TODO: We probably don't need all of these, I should look into
// paring the cereal list down a bit.

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <array>
#include <meta>
#include <string.h>
#include <vector>

namespace fr::autocereal {

  /**
   * Reflection has to be consteval, so we can't hoist anything with a
   * std::vector or a std::string over to runtime. Doing so will result
   * in an error to the effect of "You tried to use something created with
   * operator new". So if we want to do this in a way we can actually use
   * it, we have to use an old-timey character array, and we have to
   * know the sizes of those arrays in advance. If you want to support
   * UTF8 identifier names or something, you'll need to use a different
   * character type.
   *
   * This does mean each autocereal class will consume
   * MAX_IDENTIFIER_LENGTH * MAX_CLASS_MEMORY bytes of memory
   * at a minimum to store the table.
   *
   */
  
  constexpr size_t MAX_IDENTIFIER_LENGTH = 256;
  constexpr size_t MAX_CLASS_MEMBERS = 256;

  /**
   * We need to be able to get the size of the member list in a
   * constexpr/consteval manner.
   */

  template <typename Class>
  consteval size_t member_list_size() {
    constexpr auto ctx = std::meta::access_context::unchecked();
    auto members = std::meta::nonstatic_data_members_of(^^Class, ctx);
    return members.size();
  }
  
  /**
   * Populate an array of character arrays with identifier names
   * from the target class.
   */
  
  template <typename Class>
  consteval auto class_member_names() {
    constexpr auto ctx = std::meta::access_context::unchecked();
    auto members = std::meta::nonstatic_data_members_of(^^Class, ctx);
    assert(members.size() < MAX_CLASS_MEMBERS);
    std::array<std::array<char, MAX_IDENTIFIER_LENGTH>, MAX_CLASS_MEMBERS> names;

    // Clear all names
    for (int i = 0; i < MAX_CLASS_MEMBERS; ++i) {
      names[i].fill('\0');
    }
    
    for (int memberIndex = 0; memberIndex < members.size(); ++memberIndex) {      
      auto svName = std::meta::identifier_of(members[memberIndex]);
      assert(svName.length() < MAX_IDENTIFIER_LENGTH);
      std::copy(svName.begin(), svName.end(), names[memberIndex].begin());
    }
    return names;
  }

  /**
   * Create a runtime vector of names from class_member_names data.
   * We could use the array directly but we'll want to spork our
   * eyeballs out before we're done.
   */

  struct Stringify {
    std::vector<std::string> stringify(std::array<std::array<char, MAX_IDENTIFIER_LENGTH>, MAX_CLASS_MEMBERS> names) {
      int idx = 0;
      std::vector<std::string> ret;
      while (names[idx][0] != '\0') {
        // I can use the character data constructor here since I zeroed out my memory.
        std::string name(names[idx].data());
        ret.push_back(name);
        idx++;
      }
      return ret;
    }
  };

  /**
   * Retrieve a memberinfo for a member, by index
   *
   * Since this is consteval, index must be a template parameter
   */
  
  template <typename Class, size_t idx>
  consteval auto member_info() {
    constexpr auto ctx = std::meta::access_context::unchecked();    
    constexpr auto member = std::meta::nonstatic_data_members_of(^^Class, ctx)[idx];
    
    return member;
  }

  /**
   * Get a ref using the return value of member_info. Somewhat surprisingly,
   * this didn't put up a fight.
   */
  
  template <typename Class, auto info>
  constexpr auto& member_ref(Class &instance) {
    return instance.[:info:];
  }

  /**
   * We also need a const version of member_ref
   */

  template <typename Class, auto info>
  constexpr const auto member_ref_const(const Class &instance) {
    return instance.[:info:];
  }

  /**
   * We need to use save_helper to recursively unwind
   * the calls to archive, since we need to send the
   * current index as a template parameter and we
   * can't just use a for loop index to do that.
   *
   * "template for" is also right out, as it wants to
   * be consteval.
   */

  template <typename Archive, typename Class, size_t memberCount, size_t index = 0>
  void saveHelper(Archive &ar, const Class& instance) {
    // TODO: Come up with a way to not create one instance of this array for every
    // member in the class.
    constexpr static auto namesArray = fr::autocereal::class_member_names<Class>();
    Stringify stringifier;
    auto names = stringifier.stringify(namesArray);

    static_assert(memberCount > 0);
    static_assert(index < memberCount);
    // Get instance ref for index
    constexpr auto ref_info = fr::autocereal::member_info<Class, index>();
    // Extract the data from the cass
    const auto& constRef = fr::autocereal::member_ref_const<Class, ref_info>(instance);
    ar(cereal::make_nvp(names[index], constRef));

    // Recursively unwind until all members are archived
    if constexpr ((index + 1)  < memberCount) {
      saveHelper<Archive, Class, memberCount, index + 1>(ar, instance);
    }    
  }

  /**
   * And a load version of that
   */

  template <typename Archive, typename Class, size_t memberCount, size_t index = 0>
  void loadHelper(Archive &ar, Class& instance) {
    static_assert(memberCount > 0);
    static_assert(index < memberCount);

    constexpr auto ref_info = fr::autocereal::member_info<Class, index>();
    auto& ref = fr::autocereal::member_ref<Class, ref_info>(instance);

    // We don't use cereal::make_nvp here because it can lead to weirdness
    // As long as the order is the same as the save function, this is fine.
    
    ar(ref);

    if constexpr ((index + 1) < memberCount) {
      loadHelper<Archive, Class, memberCount, index + 1>(ar, instance);
    }
  }
  
}

namespace cereal {

  /**
   * Implements a save function that can be used for any class
   * Note that if you have private members you want to load or save
   * you need to follow the "Non-public Serialization" instructions
   * at https://uscilab.github.io/cereal/serialization_functions.html
   * to to enable cereal to access those members.
   */

  template <typename Archive, typename Class>
  void save(Archive &ar, const Class& instance) {
    constexpr static size_t memberCount = fr::autocereal::member_list_size<Class>();
    fr::autocereal::saveHelper<Archive, Class, memberCount>(ar, instance);
  }

  /**
   * and the load version of that
   */

  template <typename Archive, typename Class>
  void load(Archive &ar, Class &instance) {
    constexpr static size_t memberCount = fr::autocereal::member_list_size<Class>();
    fr::autocereal::loadHelper<Archive, Class, memberCount>(ar, instance);
  }
  
}
