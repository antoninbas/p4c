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

#ifndef BACKENDS_BMV2_DPDK_SWITCH_DPDKSWITCH_H_
#define BACKENDS_BMV2_DPDK_SWITCH_DPDKSWITCH_H_

#include <algorithm>
#include <cstring>
#include "frontends/common/constantFolding.h"
#include "frontends/p4/evaluator/evaluator.h"
#include "frontends/p4/fromv1.0/v1model.h"
#include "frontends/p4/simplify.h"
#include "frontends/p4/unusedDeclarations.h"
#include "midend/convertEnums.h"
#include "backends/bmv2/common/action.h"
#include "backends/bmv2/common/backend.h"
#include "backends/bmv2/common/control.h"
#include "backends/bmv2/common/deparser.h"
#include "backends/bmv2/common/extern.h"
#include "backends/bmv2/common/globals.h"
#include "backends/bmv2/common/header.h"
#include "backends/bmv2/common/parser.h"
#include "backends/bmv2/common/programStructure.h"
#include "backends/bmv2/common/sharedActionSelectorCheck.h"

namespace BMV2 {

class DpdkProgramStructure : public ProgramStructure {
 public:
    std::set<cstring>                pipeline_controls;
    std::set<cstring>                non_pipeline_controls;

    ordered_map<cstring, const IR::P4Parser*> parsers;
    ordered_map<cstring, const IR::P4Control*> pipes;
    ordered_map<cstring, const IR::P4Control*> deparsers;

    // architecture related information
    // ordered_map<const IR::Node*, std::pair<cstring, block_t> > block_type;

    DpdkProgramStructure() { }
};

class DpdkSwitchExpressionConverter : public ExpressionConverter {
    DpdkProgramStructure* structure;

 public:
    DpdkSwitchExpressionConverter(P4::ReferenceMap* refMap, P4::TypeMap* typeMap,
        DpdkProgramStructure* structure, cstring scalarsName) :
        ExpressionConverter(refMap, typeMap, structure, scalarsName), structure(structure) { }

    bool isStandardMetadataParameter(const IR::Parameter* param) {
        auto st = dynamic_cast<DpdkProgramStructure*>(structure);
        for (const auto& parser : st->parsers) {
            auto params = parser.second->getApplyParameters();
            if (params->parameters.at(3) == param)
                return true;
        }
        for (const auto& pipe : st->pipes) {
            auto params = pipe.second->getApplyParameters();
            if (params->parameters.at(2) == param)
                return true;
        }
        for (const auto& deparser : st->deparsers) {
            auto params = deparser.second->getApplyParameters();
            if (params->parameters.at(3) == param)
                return true;
        }
        return false;
    }

    Util::IJson* convertParam(const IR::Parameter* param, cstring fieldName) override {
        if (isStandardMetadataParameter(param)) {
            auto result = new Util::JsonObject();
            result->emplace("type", "field");
            auto e = BMV2::mkArrayField(result, "value");
            e->append("standard_metadata");
            e->append(fieldName);
            return result;
        }
        return nullptr;
    }
};

class ParseDpdkArchitecture : public Inspector {
    DpdkProgramStructure* structure;
    P4V1::V1Model&      v1model;

 public:
    explicit ParseDpdkArchitecture(DpdkProgramStructure* structure) :
        structure(structure), v1model(P4V1::V1Model::instance) { }
    void modelError(const char* format, const IR::Node* node);
    bool preorder(const IR::PackageBlock* block) override;
};

class DpdkSwitchBackend : public Backend {
    BMV2Options&        options;
    P4V1::V1Model&      v1model;
    DpdkProgramStructure* structure;

 protected:
    cstring createCalculation(cstring algo, const IR::Expression* fields,
                              Util::JsonArray* calculations, bool usePayload, const IR::Node* node);

 public:
    void modelError(const char* format, const IR::Node* place) const;
    void convertChecksum(const IR::BlockStatement* body, Util::JsonArray* checksums,
                         Util::JsonArray* calculations, bool verify);
    void createActions(ConversionContext* ctxt, DpdkProgramStructure* structure);

    void convert(const IR::ToplevelBlock* tlb) override;
    DpdkSwitchBackend(BMV2Options& options, P4::ReferenceMap* refMap, P4::TypeMap* typeMap,
                        P4::ConvertEnums::EnumMapping* enumMap) :
        Backend(options, refMap, typeMap, enumMap), options(options),
        v1model(P4V1::V1Model::instance) { }
};

EXTERN_CONVERTER_W_FUNCTION(clone)
EXTERN_CONVERTER_W_FUNCTION_AND_MODEL(clone3, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_FUNCTION_AND_MODEL(hash, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_FUNCTION(digest)
EXTERN_CONVERTER_W_FUNCTION(resubmit)
EXTERN_CONVERTER_W_FUNCTION(recirculate)
EXTERN_CONVERTER_W_FUNCTION(mark_to_drop)
EXTERN_CONVERTER_W_FUNCTION_AND_MODEL(random, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_FUNCTION_AND_MODEL(truncate, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE_AND_MODEL(register, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE_AND_MODEL(counter, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE_AND_MODEL(meter, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE(direct_counter)
EXTERN_CONVERTER_W_OBJECT_AND_INSTANCE_AND_MODEL(direct_meter, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_INSTANCE_AND_MODEL(action_profile, P4V1::V1Model, v1model)
EXTERN_CONVERTER_W_INSTANCE_AND_MODEL(action_selector, P4V1::V1Model, v1model)

}  // namespace BMV2

#endif /* BACKENDS_BMV2_DPDK_SWITCH_DPDKSWITCH_H_ */
