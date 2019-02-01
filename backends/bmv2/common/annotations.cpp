/*
Copyright 2019-present Barefoot Networks, Inc.

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

#include "annotations.h"

#include <sstream>  // for std::ostringstream
#include <string>

#include "frontends/p4/toP4/toP4.h"

namespace BMV2 {

static std::string serializeOneAnnotation(const IR::Annotation* annotation) {
    std::ostringstream oss;
    P4::ToP4 top4(&oss, false);
    annotation->apply(top4);
    auto serializedAnnnotation = oss.str();
    // remove the whitespace added by ToP4.
    serializedAnnnotation.pop_back();
    return serializedAnnnotation;
}

void addAnnotations(Util::JsonObject* jObject, const IR::IAnnotated* annotated) {
    CHECK_NULL(jObject);

    if (annotated == nullptr) return;

    auto* jAnnotations = new Util::JsonArray();
    jObject->emplace("annotations", jAnnotations);

    for (const IR::Annotation* annotation : annotated->getAnnotations()->annotations) {
        if (annotation->name == IR::Annotation::nameAnnotation) continue;

        if (annotation->name == "brief" || annotation->name == "description")
          continue;

        jAnnotations->append(serializeOneAnnotation(annotation));
    }
}

}  // namespace BMV2
