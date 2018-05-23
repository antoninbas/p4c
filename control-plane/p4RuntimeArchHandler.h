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

#include <set>

#include <boost/optional.hpp>

#include "p4/config/p4info.pb.h"

#include "frontends/p4/externInstance.h"
#include "frontends/p4/methodInstance.h"
#include "ir/ir.h"
#include "lib/ordered_set.h"

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
    virtual void postCollect(const P4RuntimeSymbolTableIface& symbols) = 0;
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

struct P4RuntimeArchHandlerBuilder {
    virtual ~P4RuntimeArchHandlerBuilder() { }

    virtual P4RuntimeArchHandlerIface* operator()(
        ReferenceMap* refMap,
        TypeMap* typeMap,
        const IR::ToplevelBlock* evaluatedProgram) const = 0;
};

namespace Helpers {

/// @return an extern instance defined or referenced by the value of @table's
/// @propertyName property, or boost::none if no extern was referenced.
boost::optional<ExternInstance>
getExternInstanceFromProperty(const IR::P4Table* table,
                              const cstring& propertyName,
                              ReferenceMap* refMap,
                              TypeMap* typeMap,
                              bool *isConstructedInPlace = nullptr);

bool isExternPropertyConstructedInPlace(const IR::P4Table* table,
                                        const cstring& propertyName);

/// Visit evaluated blocks under the provided top-level block. Guarantees that
/// each block is visited only once, even if multiple paths to reach it exist.
template <typename Func>
void forAllEvaluatedBlocks(const IR::ToplevelBlock* aToplevelBlock, Func function) {
    std::set<const IR::Block*> visited;
    ordered_set<const IR::Block*> frontier{aToplevelBlock};

    while (!frontier.empty()) {
        // Pop a block off the frontier of blocks we haven't yet visited.
        auto evaluatedBlock = *frontier.begin();
        frontier.erase(frontier.begin());
        visited.insert(evaluatedBlock);

        function(evaluatedBlock);

        // Add child blocks to the frontier if we haven't already visited them.
        for (auto evaluatedChild : evaluatedBlock->constantValue) {
            if (!evaluatedChild.second->is<IR::Block>()) continue;
            auto evaluatedChildBlock = evaluatedChild.second->to<IR::Block>();
            if (visited.find(evaluatedChildBlock) != visited.end()) continue;
            frontier.insert(evaluatedChildBlock);
        }
    }
}

/// Serialize @annotated's P4 annotations and attach them to a P4Info message
/// with an 'annotations' field. '@name' and '@id' are ignored, as well as
/// annotations whose name satisfies predicate @p.
template <typename Message, typename UnaryPredicate>
void addAnnotations(Message* message, const IR::IAnnotated* annotated, UnaryPredicate p) {
    CHECK_NULL(message);

    // Synthesized resources may have no annotations.
    if (annotated == nullptr) return;

    for (const IR::Annotation* annotation : annotated->getAnnotations()->annotations) {
        // Don't output the @name or @id annotations; they're represented
        // elsewhere in P4Info messages.
        if (annotation->name == IR::Annotation::nameAnnotation) continue;
        if (annotation->name == "id") continue;
        if (p(annotation->name)) continue;

        // Serialize the annotation.
        // XXX(seth): Might be nice to do something better than rely on toString().
        std::string serializedAnnotation = "@" + annotation->name + "(";
        auto expressions = annotation->expr;
        for (unsigned i = 0; i < expressions.size(); ++i) {
            serializedAnnotation.append(expressions[i]->toString());
            if (i + 1 < expressions.size()) serializedAnnotation.append(", ");
        }
        serializedAnnotation.append(")");

        message->add_annotations(serializedAnnotation);
    }
}

/// calls addAnnotations with a unconditionally false predicate.
template <typename Message>
void addAnnotations(Message* message, const IR::IAnnotated* annotated) {
    addAnnotations(message, annotated, [](cstring){ return false; });
}

/// @return @table's size property if available, falling back to the
/// architecture's default size.
int64_t getTableSize(const IR::P4Table* table);

/// A traits class describing the properties of "counterlike" things.
template <typename Kind> struct CounterlikeTraits;

/**
 * The information about a counter or meter instance which is necessary to
 * serialize it. @Kind must be a class with a CounterlikeTraits<>
 * specialization.
 */
template <typename Kind>
struct Counterlike {
    /// The name of the instance.
    const cstring name;
    /// If non-null, the instance's annotations.
    const IR::IAnnotated* annotations;
    /// The units parameter to the instance; valid values vary depending on @Kind.
    const cstring unit;
    /// The size parameter to the instance.
    const int64_t size;
    /// If not none, the instance is a direct resource associated with @table.
    const boost::optional<cstring> table;

    /// @return the information required to serialize an explicit @instance of
    /// @Kind, which is defined inside a control block.
    static boost::optional<Counterlike<Kind>>
    from(const IR::ExternBlock* instance) {
        CHECK_NULL(instance);
        auto declaration = instance->node->to<IR::IDeclaration>();

        // Counter and meter externs refer to their unit as a "type"; this is
        // (confusingly) unrelated to the "type" field of a counter or meter in
        // P4Info.
        auto unit = instance->getParameterValue("type");
        if (!unit->is<IR::Declaration_ID>()) {
            ::error("%1% '%2%' has a unit type which is not an enum constant: %3%",
                    CounterlikeTraits<Kind>::name(), declaration, unit);
            return boost::none;
        }

        auto size = instance->getParameterValue("size")->to<IR::Constant>();
        if (!size->is<IR::Constant>()) {
            ::error("%1% '%2%' has a non-constant size: %3%",
                    CounterlikeTraits<Kind>::name(), declaration, size);
            return boost::none;
        }

        return Counterlike<Kind>{declaration->controlPlaneName(),
                                 declaration->to<IR::IAnnotated>(),
                                 unit->to<IR::Declaration_ID>()->name,
                                 size->to<IR::Constant>()->value.get_si(),
                                 boost::none};
    }

    /// @return the information required to serialize an @instance of @Kind which
    /// is either defined in or referenced by a property value of @table. (This
    /// implies that @instance is a direct resource of @table.)
    static boost::optional<Counterlike<Kind>>
    fromDirect(const ExternInstance& instance, const IR::P4Table* table) {
        CHECK_NULL(table);
        BUG_CHECK(instance.name != boost::none,
                  "Caller should've ensured we have a name");

        if (instance.type->name != CounterlikeTraits<Kind>::directTypeName()) {
            ::error("Expected a direct %1%: %2%", CounterlikeTraits<Kind>::name(),
                    instance.expression);
            return boost::none;
        }

        auto unitArgument = instance.arguments->at(0)->expression;
        if (unitArgument == nullptr) {
            ::error("Direct %1% instance %2% should take a constructor argument",
                    CounterlikeTraits<Kind>::name(), instance.expression);
            return boost::none;
        }
        if (!unitArgument->is<IR::Member>()) {
            ::error("Direct %1% instance %2% has an unexpected constructor argument",
                    CounterlikeTraits<Kind>::name(), instance.expression);
            return boost::none;
        }

        auto unit = unitArgument->to<IR::Member>()->member.name;
        return Counterlike<Kind>{*instance.name, instance.annotations,
                                 unit, Helpers::getTableSize(table),
                                 table->controlPlaneName()};
    }
};

/// @return the direct counter associated with @table, if it has one, or
/// boost::none otherwise.
template <typename Kind>
boost::optional<Counterlike<Kind>>
getDirectCounterlike(const IR::P4Table* table, ReferenceMap* refMap, TypeMap* typeMap) {
    auto propertyName = CounterlikeTraits<Kind>::directPropertyName();
    auto instance =
      getExternInstanceFromProperty(table, propertyName, refMap, typeMap);
    if (!instance) return boost::none;
    return Counterlike<Kind>::fromDirect(*instance, table);
}

}  // namespace Helpers

namespace Standard {

struct V1ModelArchHandlerBuilder : public P4RuntimeArchHandlerBuilder {
    P4RuntimeArchHandlerIface* operator()(
        ReferenceMap* refMap,
        TypeMap* typeMap,
        const IR::ToplevelBlock* evaluatedProgram) const override;
};

}  // namespace Standard

}  // namespace ControlPlaneAPI

}  // namespace P4

#endif  /* CONTROL_PLANE_P4RUNTIMEARCHHANDLER_H_ */
