// Copyright (c) 2020 Stefano Milizia
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/fuzz/fuzzer_pass_merge_function_returns.h"

#include "source/fuzz/fuzzer_util.h"
#include "source/fuzz/instruction_descriptor.h"
#include "source/fuzz/transformation_merge_function_returns.h"

namespace spvtools {
namespace fuzz {

FuzzerPassMergeFunctionReturns::FuzzerPassMergeFunctionReturns(
    opt::IRContext* ir_context, TransformationContext* transformation_context,
    FuzzerContext* fuzzer_context,
    protobufs::TransformationSequence* transformations)
    : FuzzerPass(ir_context, transformation_context, fuzzer_context,
                 transformations) {}

FuzzerPassMergeFunctionReturns::~FuzzerPassMergeFunctionReturns() = default;

void FuzzerPassMergeFunctionReturns::Apply() {
  for (auto& function : *GetIRContext()->module()) {
    // Randomly decide whether to consider this function.
    if (GetFuzzerContext()->ChoosePercentage(
            GetFuzzerContext()->GetChanceOfMergingFunctionReturns())) {
      continue;
    }

    // Only consider functions that have early returns.
    if (!function.HasEarlyReturn()) {
      continue;
    }

    // Get the return blocks.
    auto return_blocks = fuzzerutil::GetReachableReturnBlocks(
        GetIRContext(), function.result_id());

    // Only go ahead if there is more than one reachable return block.
    if (return_blocks.size() <= 1) {
      continue;
    }

    // Make sure that OpConstantTrue and OpConstantFalse are in the module.
    FindOrCreateBoolConstant(true, false);
    FindOrCreateBoolConstant(false, false);

    // Collect the ids available after the entry block of the function.
    auto ids_available_after_entry_block =
        GetTypesToIdsAvailableAfterEntryBlock(&function);

    // If the entry block does not branch unconditionally to another block,
    // split it.
    if (function.entry()->terminator()->opcode() != SpvOpBranch) {
      SplitBlockAfterOpPhiOrOpVariable(function.entry()->id());
    }

    // Collect the merge blocks of the function whose corresponding loops
    // contain return blocks.
    auto merge_blocks = GetMergeBlocksOfLoopsContainingBlocks(return_blocks);

    // Split the merge blocks, if they contain instructions different from
    // OpLabel, OpPhi and OpBranch. Collect the new ids of merge blocks.
    std::vector<uint32_t> actual_merge_blocks;
    for (uint32_t merge_block : merge_blocks) {
      opt::BasicBlock* block = GetIRContext()->get_instr_block(merge_block);

      // We don't need to split blocks that are already suitable (they only
      // contain OpLabel, OpPhi or OpBranch instructions).
      if (GetIRContext()
              ->get_instr_block(merge_block)
              ->WhileEachInst([](opt::Instruction* inst) {
                return inst->opcode() == SpvOpLabel ||
                       inst->opcode() == SpvOpPhi ||
                       inst->opcode() == SpvOpBranch;
              })) {
        actual_merge_blocks.emplace_back(merge_block);
        continue;
      }

      // If the merge block is also a loop header, we need to add a preheader,
      // which will be the new merge block.
      if (block->IsLoopHeader()) {
        actual_merge_blocks.emplace_back(
            GetOrCreateSimpleLoopPreheader(merge_block)->id());
        continue;
      }

      // If the merge block is not a loop header, we must split it after the
      // last OpPhi instruction. The merge block will be the first of the pair
      // of blocks obtained after splitting, and it keeps the original id.
      SplitBlockAfterOpPhiOrOpVariable(merge_block);
      actual_merge_blocks.emplace_back(merge_block);
    }

    // Get the ids needed by the transformation.
    uint32_t outer_header_id = GetFuzzerContext()->GetFreshId();
    uint32_t outer_return_id = GetFuzzerContext()->GetFreshId();

    bool function_is_void =
        GetIRContext()->get_type_mgr()->GetType(function.type_id())->AsVoid();

    // We only need a return value if the function is not void.
    uint32_t return_val_id =
        function_is_void ? 0 : GetFuzzerContext()->GetFreshId();

    // We only need a placeholder for the return value if the function is not
    // void and there is at least one relevant merge block.
    uint32_t returnable_val_id = 0;
    if (!function_is_void && !actual_merge_blocks.empty()) {
      // If there is an id of the suitable type, choose one at random.
      if (ids_available_after_entry_block.count(function.type_id())) {
        const auto& candidates =
            ids_available_after_entry_block[function.type_id()];
        returnable_val_id =
            candidates[GetFuzzerContext()->RandomIndex(candidates)];
      } else {
        // If there is no id, add a global OpUndef.
        uint32_t suitable_id = FindOrCreateGlobalUndef(function.type_id());
        // Add the new id to the map of available ids.
        ids_available_after_entry_block.emplace(
            function.type_id(), std::vector<uint32_t>({suitable_id}));
        returnable_val_id = suitable_id;
      }
    }

    // Collect all the ids needed for merge blocks.
    auto merge_blocks_info = GetInfoNeededForMergeBlocks(
        actual_merge_blocks, &ids_available_after_entry_block);

    // Apply the transformation if it is applicable (it could be inapplicable if
    // adding new predecessors to merge blocks breaks dominance rules).
    MaybeApplyTransformation(TransformationMergeFunctionReturns(
        function.result_id(), outer_header_id, outer_return_id, return_val_id,
        returnable_val_id, merge_blocks_info));
  }
}

std::map<uint32_t, std::vector<uint32_t>>
FuzzerPassMergeFunctionReturns::GetTypesToIdsAvailableAfterEntryBlock(
    opt::Function* function) const {
  std::map<uint32_t, std::vector<uint32_t>> result;
  // Consider all global declarations
  for (auto& global : GetIRContext()->module()->types_values()) {
    if (global.HasResultId() && global.type_id()) {
      if (!result.count(global.type_id())) {
        result.emplace(global.type_id(), std::vector<uint32_t>());
      }
      result[global.type_id()].emplace_back(global.result_id());
    }
  }

  // Consider all function parameters
  function->ForEachParam([&result](opt::Instruction* param) {
    if (param->HasResultId() && param->type_id()) {
      if (!result.count(param->type_id())) {
        result.emplace(param->type_id(), std::vector<uint32_t>());
      }

      result[param->type_id()].emplace_back(param->result_id());
    }
  });

  // Consider all the instructions in the entry block.
  for (auto& inst : *function->entry()) {
    if (inst.HasResultId() && inst.type_id()) {
      if (!result.count(inst.type_id())) {
        result.emplace(inst.type_id(), std::vector<uint32_t>());
      }
      result[inst.type_id()].emplace_back(inst.result_id());
    }
  }

  return result;
}

std::set<uint32_t>
FuzzerPassMergeFunctionReturns::GetMergeBlocksOfLoopsContainingBlocks(
    const std::set<uint32_t>& blocks) const {
  std::set<uint32_t> result;
  for (uint32_t block : blocks) {
    uint32_t merge_block =
        GetIRContext()->GetStructuredCFGAnalysis()->LoopMergeBlock(block);

    while (merge_block != 0 && !result.count(merge_block)) {
      // Add a new entry.
      result.emplace(merge_block);

      // Walk up the loop tree.
      block = merge_block;
      merge_block = GetIRContext()->GetStructuredCFGAnalysis()->LoopMergeBlock(
          merge_block);
    }
  }

  return result;
}

std::vector<protobufs::ReturnMergingInfo>
FuzzerPassMergeFunctionReturns::GetInfoNeededForMergeBlocks(
    const std::vector<uint32_t>& merge_blocks,
    std::map<uint32_t, std::vector<uint32_t>>*
        ids_available_after_entry_block) {
  std::vector<protobufs::ReturnMergingInfo> result;
  for (uint32_t merge_block : merge_blocks) {
    protobufs::ReturnMergingInfo info;
    info.set_merge_block_id(merge_block);
    info.set_is_returning_id(this->GetFuzzerContext()->GetFreshId());
    info.set_maybe_return_val_id(this->GetFuzzerContext()->GetFreshId());

    // Add all the ids needed for the OpPhi instructions.
    this->GetIRContext()
        ->get_instr_block(merge_block)
        ->ForEachPhiInst([this, &info, &ids_available_after_entry_block](
                             opt::Instruction* phi_inst) {
          protobufs::UInt32Pair entry;
          entry.set_first(phi_inst->result_id());

          // If there is an id of the suitable type, choose one at random.
          if (ids_available_after_entry_block->count(phi_inst->type_id())) {
            auto& candidates =
                ids_available_after_entry_block->at(phi_inst->type_id());
            entry.set_second(
                candidates[this->GetFuzzerContext()->RandomIndex(candidates)]);
          } else {
            // If there is no id, add a global OpUndef.
            uint32_t suitable_id =
                this->FindOrCreateGlobalUndef(phi_inst->type_id());
            // Add the new id to the map of available ids.
            ids_available_after_entry_block->emplace(
                phi_inst->type_id(), std::vector<uint32_t>({suitable_id}));
            entry.set_second(suitable_id);
          }

          // Add the entry to the list.
          *info.add_opphi_to_suitable_id() = entry;
        });

    result.emplace_back(info);
  }

  return result;
}

}  // namespace fuzz
}  // namespace spvtools
