/*
Copyright 2013-present Barefoot Networks, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef CONTROL_PLANE_P4RUNTIMEARCHHANDLER_H_
#define CONTROL_PLANE_P4RUNTIMEARCHHANDLER_H_

#include <boost/optional.hpp>

#include "p4/config/p4info.pb.h"

#include "frontends/p4/methodInstance.h"
#include "ir/ir.h"

namespace P4 {

namespace ControlPlaneAPI {

using p4rt_id_t = uint32_t;

class P4RuntimeSymbolType {
 public:
    virtual ~P4RuntimeSymbolType() { }

    explicit operator p4rt_id_t() const { return id; }

    static P4RuntimeSymbolType ACTION() {
        return P4RuntimeSymbolType(p4::config::P4Ids::ACTION);
    }
    static P4RuntimeSymbolType TABLE() {
        return P4RuntimeSymbolType(p4::config::P4Ids::TABLE);
    }
    static P4RuntimeSymbolType VALUE_SET() {
        return P4RuntimeSymbolType(p4::config::P4Ids::VALUE_SET);
    }
    static P4RuntimeSymbolType CONTROLLER_HEADER() {
        return P4RuntimeSymbolType(p4::config::P4Ids::CONTROLLER_HEADER);
    }

    bool operator==(const P4RuntimeSymbolType& other) const {
        return id == other.id;
    }

    bool operator!=(const P4RuntimeSymbolType& other) const {
        return !(*this == other);
    }

    bool operator<(const P4RuntimeSymbolType& other) const {
        return id < other.id;
    }

 protected:
    static P4RuntimeSymbolType make(p4rt_id_t id) {
        return P4RuntimeSymbolType(id);
    }

 private:
    // even if the constructor is protected, the static functions in the derived
    // classes cannot access it, which is why we use the make factory function
    constexpr P4RuntimeSymbolType(p4rt_id_t id) noexcept
        : id(id) { }

    p4rt_id_t id;
};

/// A table which tracks the symbols which are visible to P4Runtime and their ids.
class P4RuntimeSymbolTableIface {
 public:
    virtual ~P4RuntimeSymbolTableIface() { }
    /// Add a @type symbol, extracting the name and id from @declaration.
    virtual void add(P4RuntimeSymbolType type, const IR::IDeclaration* declaration) = 0;
    /// Add a @type symbol with @name and possibly an explicit P4 '@id'.
    virtual void add(P4RuntimeSymbolType type, cstring name,
                     boost::optional<p4rt_id_t> id = boost::none) = 0;
    /// @return the P4Runtime id for the symbol of @type corresponding to @declaration.
    virtual p4rt_id_t getId(P4RuntimeSymbolType type,
                            const IR::IDeclaration* declaration) const = 0;
    /// @return the P4Runtime id for the symbol of @type with name @name.
    virtual p4rt_id_t getId(P4RuntimeSymbolType type, cstring name) const = 0;
    /// @return the alias for the given fully qualified external name. P4Runtime
    /// defines an alias for each object to make referring to objects easier.
    /// By default, the alias is the shortest unique suffix of path components in
    /// the name.
    virtual cstring getAlias(cstring name) const = 0;
};

class P4RuntimeArchHandlerIface {
 public:
    virtual ~P4RuntimeArchHandlerIface() { }
    virtual void collectTableProperties(P4RuntimeSymbolTableIface* symbols,
                                        const IR::TableBlock* tableBlock) = 0;
    virtual void collectExternInstance(P4RuntimeSymbolTableIface* symbols,
                                       const IR::ExternBlock* externBlock) = 0;
    virtual void collectExternFunction(P4RuntimeSymbolTableIface* symbols,
                                       const P4::ExternFunction* externFunction) = 0;
    virtual void addTableProperties(const P4RuntimeSymbolTableIface& symbols,
                                    p4::config::P4Info* p4info,
                                    p4::config::Table* table,
                                    const IR::TableBlock* tableBlock) = 0;
    virtual void addExternInstance(const P4RuntimeSymbolTableIface& symbols,
                                   p4::config::P4Info* p4info,
                                   const IR::ExternBlock* externBlock) = 0;
    virtual void addExternFunction(const P4RuntimeSymbolTableIface& symbols,
                                   p4::config::P4Info* p4info,
                                   const P4::ExternFunction* externFunction) = 0;
};

class P4RuntimeArchHandlerBuilder {
 public:
    virtual ~P4RuntimeArchHandlerBuilder() { }

    virtual P4RuntimeArchHandlerIface* operator()(
        ReferenceMap* refMap,
        TypeMap* typeMap) const = 0;
};

/// @return an extern instance defined or referenced by the value of @table's
/// @propertyName property, or boost::none if no extern was referenced.
static boost::optional<ExternInstance>
getExternInstanceFromProperty(const IR::P4Table* table,
                              const cstring& propertyName,
                              ReferenceMap* refMap,
                              TypeMap* typeMap,
                              bool *isConstructedInPlace = nullptr);

}  // namespace ControlPlaneAPI

}  // namespace P4

#endif  /* CONTROL_PLANE_P4RUNTIMEARCHHANDLER_H_ */
