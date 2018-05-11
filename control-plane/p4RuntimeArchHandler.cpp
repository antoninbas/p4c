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

#include <boost/optional.hpp>

#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/externInstance.h"
#include "frontends/p4/typeMap.h"
#include "ir/ir.h"

#include "p4RuntimeArchHandler.h"

namespace P4 {

namespace ControlPlaneAPI {

static boost::optional<ExternInstance>
getExternInstanceFromProperty(const IR::P4Table* table,
                              const cstring& propertyName,
                              ReferenceMap* refMap,
                              TypeMap* typeMap,
                              bool *isConstructedInPlace) {
    auto property = table->properties->getProperty(propertyName);
    if (property == nullptr) return boost::none;
    if (!property->value->is<IR::ExpressionValue>()) {
        ::error("Expected %1% property value for table %2% to be an expression: %3%",
                propertyName, table->controlPlaneName(), property);
        return boost::none;
    }

    auto expr = property->value->to<IR::ExpressionValue>()->expression;
    if (isConstructedInPlace) *isConstructedInPlace = expr->is<IR::ConstructorCallExpression>();
    if (expr->is<IR::ConstructorCallExpression>()
        && property->getAnnotation(IR::Annotation::nameAnnotation) == nullptr) {
        ::error("Table '%1%' has an anonymous table property '%2%' with no name annotation, "
                "which is not supported by P4Runtime", table->controlPlaneName(), propertyName);
        return boost::none;
    }
    auto name = property->controlPlaneName();
    auto externInstance = ExternInstance::resolve(expr, refMap, typeMap, name);
    if (!externInstance) {
        ::error("Expected %1% property value for table %2% to resolve to an "
                "extern instance: %3%", propertyName, table->controlPlaneName(),
                property);
        return boost::none;
    }

    return externInstance;
}

}  // namespace ControlPlaneAPI

}  // namespace P4
