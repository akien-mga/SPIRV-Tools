// Copyright (c) 2017 Google Inc.
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

#include "markv_model_shader_default.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using spvutils::HuffmanCodec;

namespace spvtools {

namespace {

// Signals that the value is not in the coding scheme and a fallback method
// needs to be used.
const uint64_t kMarkvNoneOfTheAbove = MarkvModel::GetMarkvNoneOfTheAbove();

inline uint32_t CombineOpcodeAndNumOperands(uint32_t opcode,
                                            uint32_t num_operands) {
  return opcode | (num_operands << 16);
}

// The following file contains autogenerated statistical coding rules.
// Generated by running spirv-stats on representative corpus of shaders with
// flags:
// --codegen_opcode_and_num_operands_hist
// --codegen_opcode_and_num_operands_markov_huffman_codecs
// --codegen_literal_string_huffman_codecs
// --codegen_non_id_word_huffman_codecs
// --codegen_id_descriptor_huffman_codecs
//
// Example:
// find <SHADER_CORPUS_DIR> -type f -print0 | xargs -0 -s 2000000
// ~/SPIRV-Tools/build/tools/spirv-stats -v
// --codegen_opcode_and_num_operands_hist
// --codegen_opcode_and_num_operands_markov_huffman_codecs
// --codegen_literal_string_huffman_codecs --codegen_non_id_word_huffman_codecs
// --codegen_id_descriptor_huffman_codecs -o
// ~/SPIRV-Tools/source/comp/markv_autogen.inc
#include "markv_model_shader_default_autogen.inc"

}  // namespace

MarkvModelShaderDefault::MarkvModelShaderDefault() {
  const uint16_t kVersionNumber = 0;
  SetModelVersion(kVersionNumber);

  opcode_and_num_operands_huffman_codec_.reset(
      new HuffmanCodec<uint64_t>(GetOpcodeAndNumOperandsHist()));
  opcode_and_num_operands_markov_huffman_codecs_ =
      GetOpcodeAndNumOperandsMarkovHuffmanCodecs();
  non_id_word_huffman_codecs_ = GetNonIdWordHuffmanCodecs();
  id_descriptor_huffman_codecs_ = GetIdDescriptorHuffmanCodecs();
  descriptors_with_coding_scheme_ = GetDescriptorsWithCodingScheme();
  literal_string_huffman_codecs_ = GetLiteralStringHuffmanCodecs();

  operand_chunk_lengths_[SPV_OPERAND_TYPE_TYPE_ID] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_RESULT_ID] = 8;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_ID] = 8;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_SCOPE_ID] = 8;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_MEMORY_SEMANTICS_ID] = 8;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_LITERAL_INTEGER] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_OPTIONAL_LITERAL_INTEGER] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_CAPABILITY] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_SOURCE_LANGUAGE] = 3;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_EXECUTION_MODEL] = 3;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_ADDRESSING_MODEL] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_MEMORY_MODEL] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_EXECUTION_MODE] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_STORAGE_CLASS] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_DIMENSIONALITY] = 3;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_SAMPLER_ADDRESSING_MODE] = 3;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_SAMPLER_FILTER_MODE] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_SAMPLER_IMAGE_FORMAT] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_FP_ROUNDING_MODE] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_LINKAGE_TYPE] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_ACCESS_QUALIFIER] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_OPTIONAL_ACCESS_QUALIFIER] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_FUNCTION_PARAMETER_ATTRIBUTE] = 3;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_DECORATION] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_BUILT_IN] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_GROUP_OPERATION] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_KERNEL_ENQ_FLAGS] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_KERNEL_PROFILING_INFO] = 2;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_FP_FAST_MATH_MODE] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_FUNCTION_CONTROL] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_LOOP_CONTROL] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_IMAGE] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_OPTIONAL_IMAGE] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_OPTIONAL_MEMORY_ACCESS] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_SELECTION_CONTROL] = 4;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_EXTENSION_INSTRUCTION_NUMBER] = 6;
  operand_chunk_lengths_[SPV_OPERAND_TYPE_TYPED_LITERAL_NUMBER] = 6;
}

}  // namespace spvtools