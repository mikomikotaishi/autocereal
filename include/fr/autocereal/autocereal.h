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
   * Define a singleton for any given class, which contains
   * an array of character arrays to the methods for the class
   * and the total number of elements in the class
   *
   * Appropriate singleton usage check:
   *
   * * Will there only ever be one of this type of object? Yes,
   *   classes can have only one definition, therefore there
   *   will never be need to be two of this type of singleton
   *   for a class.
   * * Is stateless: True. It contains information accumulated
   *   at compile time that will never change.
   * * Does not hide dependencies: True.
   *
   * I actually really like a singleton here. It guarantees I
   * won't run any of my reflection functions multiple times
   * on a class, and it seems great for isolating a lot of
   * the consteval/constexpr stuff that needs to happen in
   * the private sections of the code.
   *
   * This may be the first time I've ever actually liked
   * the singleton pattern for something.
   */

  template <typename Class>
  class ClassSingleton {
    constexpr static auto _ctx = std::meta::access_context::unchecked();
    static constexpr size_t _memberCount = std::meta::nonstatic_data_members_of(^^Class, _ctx).size();
    // Number of parents this class has
    static constexpr size_t _baseCount = std::meta::bases_of(^^Class, _ctx).size();
    static constexpr auto _bases = std::define_static_array(std::meta::bases_of(^^Class, _ctx));
    
    std::vector<std::string> _memberNamesStrings;

    // Sets up and returns the membernames array at compile time.
    // We can return this array and move it into _memberNames at runtime.
    consteval auto classMemberNames() {
      std::array<std::array<char, MAX_IDENTIFIER_LENGTH>, MAX_CLASS_MEMBERS> names;

      auto members = std::meta::nonstatic_data_members_of(^^Class, _ctx);
      // We do MAX_CLASS_MEMBERS - 1 so we know we'll always hit a null
      // terminator in this array
      assert(members.size() < MAX_CLASS_MEMBERS - 1);

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
    
  public:
    using ReflectionType = Class;

    template <int index>
    struct Parent {
      using Type = [:std::meta::type_of(_bases[index]):];
    };
    
    static const ClassSingleton& instance() {
      static ClassSingleton<Class> instance;
      return instance;
    }

    const std::string& memberAtIndex(int index) const {
      return _memberNamesStrings[index];
    }
    
    constexpr size_t memberCount() const {
      return _memberCount;
    }

    constexpr size_t baseCount() const {
      return _baseCount;
    }

    std::vector<std::string> getMemberNames() const {
      return _memberNamesStrings;
    }

  private:
    ClassSingleton() {
      auto memberNames = classMemberNames();
      
      size_t idx = 0;
      // Restringify array at runtime
      while(memberNames[idx][0] != '\0') {
        std::string name(memberNames[idx].data());
        _memberNamesStrings.push_back(name);
        idx++;
      }
    }
    ~ClassSingleton() {}
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
   * This will save any parents that need to be saved.
   * This should only be called if there ARE any parents.
   * (And not by you! You don't need to worry about this!)
   */

  template <typename Archive, typename Class, size_t memberCount, size_t index = 0>
  void saveHelper(Archive &ar, const Class &instance);
  
  template <typename Archive, typename Class, size_t index = 0>
  void saveParents(Archive &ar, const Class& instance) {
    const fr::autocereal::ClassSingleton<Class>& classInstance = fr::autocereal::ClassSingleton<Class>::instance();    
    using Parent = typename fr::autocereal::ClassSingleton<Class>::Parent<index>::Type;
    const fr::autocereal::ClassSingleton<Parent>& parentInstance = fr::autocereal::ClassSingleton<Parent>::instance();    
    const auto& baseInstance = static_cast<const Parent&>(instance);
    saveHelper<Archive, Parent, parentInstance.memberCount()>(ar, baseInstance);
    if constexpr ((index + 1)  < classInstance.baseCount()) {
      saveParents<Archive, Class, index + 1>(ar, instance);
    }
  }

  /**
   * This will load any parents that need to be loaded.
   * This should only be called if there ARE any parents.
   * (And not by you! You don't need to worry about this!)
   */

  template <typename Archive, typename Class, size_t memberCount, size_t index = 0>
  void loadHelper(Archive &ar, Class& instance);

  template <typename Archive, typename Class, size_t index = 0>
  void loadParents(Archive &ar, Class& instance) {
    const fr::autocereal::ClassSingleton<Class>& classInstance = fr::autocereal::ClassSingleton<Class>::instance();
    using Parent = typename fr::autocereal::ClassSingleton<Class>::Parent<index>::Type;
    const fr::autocereal::ClassSingleton<Parent>& parentInstance = fr::autocereal::ClassSingleton<Parent>::instance();    
    auto& baseInstance = static_cast<Parent&>(instance);
    loadHelper<Archive, Parent, parentInstance.memberCount()>(ar, baseInstance);
    if constexpr ((index + 1)  < classInstance.baseCount()) {
      loadParents<Archive, Class, index + 1>(ar, instance);
    }      
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

  template <typename Archive, typename Class, size_t memberCount, size_t index>
  void saveHelper(Archive &ar, const Class& instance) {
    const auto& classInstance = ClassSingleton<Class>::instance();
    static_assert(classInstance.memberCount() > 0);
    static_assert(index < classInstance.memberCount());

    if constexpr(index == 0) {
      if constexpr (classInstance.baseCount() > 0) {
        fr::autocereal::saveParents<Archive, Class>(ar, instance);
      }
    }
    
    // Get instance ref for index
    constexpr auto ref_info = fr::autocereal::member_info<Class, index>();
    // Extract the data from the cass
    const auto& constRef = fr::autocereal::member_ref_const<Class, ref_info>(instance);
    ar(cereal::make_nvp(classInstance.memberAtIndex(index), constRef));

    // Recursively unwind until all members are archived
    if constexpr ((index + 1)  < classInstance.memberCount()) {
      saveHelper<Archive, Class, classInstance.memberCount(), index + 1>(ar, instance);
    }    
  }

  /**
   * And a load version of that
   */

  template <typename Archive, typename Class, size_t memberCount, size_t index>
  void loadHelper(Archive &ar, Class& instance) {
    const auto& classInstance = ClassSingleton<Class>::instance();
    static_assert(classInstance.memberCount() > 0);
    static_assert(index < classInstance.memberCount());

    if constexpr(index == 0) {
      if constexpr (classInstance.baseCount() > 0) {
        fr::autocereal::loadParents<Archive, Class>(ar, instance);
      }
    }

    constexpr auto ref_info = fr::autocereal::member_info<Class, index>();
    auto& ref = fr::autocereal::member_ref<Class, ref_info>(instance);

    // We don't use cereal::make_nvp here because it can lead to weirdness
    // As long as the order is the same as the save function, this is fine.
    
    ar(ref);

    if constexpr ((index + 1) < classInstance.memberCount()) {
      loadHelper<Archive, Class, classInstance.memberCount(), index + 1>(ar, instance);
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
    const auto& classInstance = fr::autocereal::ClassSingleton<Class>::instance();
    fr::autocereal::saveHelper<Archive, Class, classInstance.memberCount()>(ar, instance);
  }

  /**
   * and the load version of that
   */

  template <typename Archive, typename Class>
  void load(Archive &ar, Class &instance) {
    const auto& classInstance = fr::autocereal::ClassSingleton<Class>::instance();
    fr::autocereal::loadHelper<Archive, Class, classInstance.memberCount()>(ar, instance);
  }
  
}
