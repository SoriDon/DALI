// Copyright (c) 2017-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef DALI_PIPELINE_OPERATOR_OP_SPEC_H_
#define DALI_PIPELINE_OPERATOR_OP_SPEC_H_

#include <map>
#include <utility>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <set>
#include <type_traits>

#include "dali/core/common.h"
#include "dali/core/copy_vector_helper.h"
#include "dali/core/error_handling.h"
#include "dali/core/tensor_shape_print.h"
#include "dali/pipeline/operator/argument.h"
#include "dali/pipeline/data/tensor.h"
#include "dali/pipeline/operator/op_schema.h"
#include "dali/pipeline/workspace/workspace.h"

namespace dali {



/**
 * @brief Defines all parameters needed to construct an Operator,
 * DataReader, Parser, or Allocator including the object name,
 * any additional input and output tensors it may need, and any
 * number of additional arguments.
 */
class DLL_PUBLIC OpSpec {
 public:
  struct InOutDeviceDesc {
    std::string name;
    std::string device;
    bool operator<(const InOutDeviceDesc &other) const {
      return std::make_pair(name, device) < std::make_pair(other.name, other.device);
    }
  };

  DLL_PUBLIC inline OpSpec() = default;

  /**
   * @brief Returns a full tensor name given its name and device
   */
  DLL_PUBLIC static std::string TensorName(const std::string &name, const std::string &device) {
    return name + "_" + device;
  }

  /**
   * @brief Constructs a specification for an op with the given schema name.
   */
  DLL_PUBLIC explicit inline OpSpec(const string &schema_name) {
    SetSchema(schema_name);
  }

  /**
   * @brief Getter for the schema name of the Operator.
   */
  DLL_PUBLIC inline const string& SchemaName() const { return schema_name_; }

  /**
   * @brief Sets the schema of the Operator.
   */
  DLL_PUBLIC inline void SetSchema(const string &schema_name) {
    schema_name_ = schema_name;
    schema_ = schema_name_.empty() ? nullptr : SchemaRegistry::TryGetSchema(schema_name_);
  }

  DLL_PUBLIC inline const OpSchema &GetSchema() const {
    DALI_ENFORCE(schema_ != nullptr, "No schema found for operator \"" + SchemaName() + "\"");
    return *schema_;
  }

  /**
   * @brief Add an argument with the given name and value.
   */
  template <typename T>
  DLL_PUBLIC inline OpSpec& AddArg(const string &name, const T &val) {
    EnforceNoAliasWithDeprecated(name);
    DALI_ENFORCE(argument_idxs_.find(name) == argument_idxs_.end(),
        "AddArg failed. Argument with name \"" + name +
        "\" already exists. ");
    return SetArg(name, val);
  }

  /**
   * @brief Add an argument with the given name and value if it doesn't exist already.
   */
  template <typename T>
  DLL_PUBLIC inline OpSpec& AddArgIfNotExisting(const string &name, const T &val) {
    if (argument_idxs_.find(name) != argument_idxs_.end()) {
      return *this;
    }
    return SetArg(name, val);
  }

  /**
   * @brief Sets or adds an argument with the given name and value.
   */
  template <typename T>
  DLL_PUBLIC inline OpSpec& SetArg(const string &name, const T &val) {
    using S = argument_storage_t<T>;
    return SetInitializedArg(name, Argument::Store<S>(name, static_cast<S>(val)));
  }

  /**
   * @brief Sets or adds an argument with the given name and value.
   */
  template <typename T>
  DLL_PUBLIC inline OpSpec& SetArg(const string &name, const std::vector<T> &val) {
    using S = argument_storage_t<T>;
    using V = std::vector<S>;
    return SetInitializedArg(name, Argument::Store<V>(name, detail::convert_vector<S>(val)));
  }


  /**
   * @brief Add an instantiated argument with given name
   */
  DLL_PUBLIC inline OpSpec& AddInitializedArg(const string& name, std::shared_ptr<Argument> arg) {
    EnforceNoAliasWithDeprecated(name);
    DALI_ENFORCE(argument_idxs_.find(name) == argument_idxs_.end(),
        "AddArg failed. Argument with name \"" + name +
        "\" already exists. ");
    return SetInitializedArg(name, arg);
  }

  /**
   * @brief Sets or adds an argument with given name
   *
   * @remarks Deprecated arguments are renamed (or dropped, if no longer used).
   */
  DLL_PUBLIC OpSpec& SetInitializedArg(const string& arg_name, std::shared_ptr<Argument> arg);

  /**
   * @brief Check if the `arg_name` was already set through a deprecated argument
   */
  DLL_PUBLIC void EnforceNoAliasWithDeprecated(const string& arg_name);

  // Forward to string implementation
  template <unsigned N>
  DLL_PUBLIC inline OpSpec& SetArg(const string &name, const char (&c_str)[N]) {
    return this->SetArg<std::string>(name, c_str);
  }

  /**
   * @brief Specifies the name and device (cpu or gpu) of an
   * input to the op. Intermediate data all have unique names,
   * so a tensor with name "cropped" will refer to the same
   * tensor regardless of whether device is "cpu" or "gpu".
   * The ordering of inputs is also strict. The order in
   * which inputs are added to the OpSpec is the order in
   * which the Operator will receive them.
   */
  DLL_PUBLIC OpSpec& AddInput(const string &name, const string &device, bool regular_input = true);

  /**
   * @brief Specifies the argument input to the op.
   * Argument inputs are named inputs that are treated as
   * per-iteration arguments. The input may be added only if
   * corresponding argument exists in the schema.
   */
  DLL_PUBLIC OpSpec& AddArgumentInput(const string &arg_name, const string &inp_name);

  /**
   * @brief Specifies the name and device (cpu or gpu) of an
   * output to the op. Intermediate data all have unique names,
   * so a tensor with name "cropped" will refer to the same
   * tensor regardless of whether device is "cpu" or "gpu".
   * The ordering of outputs is also strict. The order in
   * which outputs are added to the OpSpec is the order in
   * which the Operator will receive them.
   */
  DLL_PUBLIC OpSpec& AddOutput(const string &name, const string &device);

  DLL_PUBLIC inline int NumInput() const { return inputs_.size(); }

  DLL_PUBLIC inline int NumArgumentInput() const {
    return argument_inputs_.size();
  }

  DLL_PUBLIC inline int NumRegularInput() const {
    return NumInput() - NumArgumentInput();
  }

  DLL_PUBLIC inline int NumOutput() const { return outputs_.size(); }

  DLL_PUBLIC inline string Input(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumInput());
    return TensorName(inputs_[idx].name, inputs_[idx].device);
  }

  DLL_PUBLIC inline string InputName(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumInput());
    return inputs_[idx].name;
  }

  DLL_PUBLIC inline string InputDevice(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumInput());
    return inputs_[idx].device;
  }

  DLL_PUBLIC inline bool IsArgumentInput(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumInput());
    return idx >= NumRegularInput();
  }

  DLL_PUBLIC inline std::string ArgumentInputName(int idx) const {
    DALI_ENFORCE(IsArgumentInput(idx),
        "Index " + to_string(idx) + " does not correspond to valid argument input.");
    return argument_inputs_[idx - NumRegularInput()].first;
  }

  DLL_PUBLIC int ArgumentInputIdx(const std::string &name) const {
    auto it = argument_input_idxs_.find(name);
    DALI_ENFORCE(it != argument_input_idxs_.end(),
                 make_string("No such argument input: \"", name, "\""));
    return it->second;
  }

  DLL_PUBLIC inline string Output(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumOutput());
    return TensorName(outputs_[idx].name, outputs_[idx].device);
  }

  DLL_PUBLIC inline string OutputName(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumOutput());
    return outputs_[idx].name;
  }

  DLL_PUBLIC inline string OutputDevice(int idx) const {
    DALI_ENFORCE_VALID_INDEX(idx, NumOutput());
    return outputs_[idx].device;
  }

  DLL_PUBLIC inline auto &ArgumentInputs() const {
    return argument_inputs_;
  }

  DLL_PUBLIC inline const auto &Arguments() const {
    return arguments_;
  }

  DLL_PUBLIC inline int OutputIdxForName(const string &name, const string &device) {
    auto it = output_name_idx_.find({name, device});
    DALI_ENFORCE(it != output_name_idx_.end(), "Output with name '" +
        name + "' and device '" + device + "' does not exist.");
    return it->second;
  }

  /**
   * @brief Checks the spec to see if an argument has been specified
   *
   * @remark If user does not explicitly specify value for OptionalArgument,
   *         this will return false.
   */
  DLL_PUBLIC bool HasArgument(const string &name) const {
    return argument_idxs_.count(name);
  }

  /**
   * @brief Checks the spec to see if a tensor argument has been specified
   */
  DLL_PUBLIC bool HasTensorArgument(const std::string &name) const {
    return argument_input_idxs_.count(name);
  }

  /**
   * @brief Checks the spec to see if an argument has been specified by one of two possible ways
   */
  DLL_PUBLIC bool ArgumentDefined(const std::string &name) const {
    return HasArgument(name) || HasTensorArgument(name);
  }

  /**
   * @brief Lists all arguments specified in this spec.
   */
  DLL_PUBLIC std::vector<std::string> ListArguments() const {
    std::vector<std::string> ret;
    for (auto &a : arguments_) {
      ret.push_back(a->get_name());
    }
    for (auto &a : argument_inputs_) {
      ret.push_back(a.first);
    }
    return ret;
  }

  /**
   * @brief Checks the Spec for an argument with the given name/type.
   * Returns the default if an argument with the given name/type does
   * not exist.
   */
  template <typename T>
  DLL_PUBLIC inline T GetArgument(const string &name,
                       const ArgumentWorkspace *ws = nullptr,
                       Index idx = 0) const {
    using S = argument_storage_t<T>;
    return GetArgumentImpl<T, S>(name, ws, idx);
  }

  template <typename T>
  DLL_PUBLIC inline bool TryGetArgument(T &result,
                                        const string &name,
                                        const ArgumentWorkspace *ws = nullptr,
                                        Index idx = 0) const {
    using S = argument_storage_t<T>;
    return TryGetArgumentImpl<T, S>(result, name, ws, idx);
  }

  /**
   * @brief Checks the Spec for a repeated argument of the given name/type.
   * Returns the default if an argument with the given name does not exist.
   *
   * @remark On Python level the arguments marked with *_VEC type, convert a single value
   * of element type to a list of element types, so GetRepeatedArgument can be used.
   * When the argument is set through C++ there is no such conversion and GetSingleOrRepeatedArg()
   * should be used instead.
   */
  template <typename T>
  DLL_PUBLIC inline std::vector<T> GetRepeatedArgument(const string &name) const {
    using S = argument_storage_t<T>;
    return GetRepeatedArgumentImpl<T, S>(name);
  }

  /**
   * @brief Checks the Spec for a repeated argument of the given name/type.
   * Returns the default if an argument with the given name does not exist.
   */
  template <typename Collection>
  DLL_PUBLIC bool TryGetRepeatedArgument(Collection &result, const string &name) const {
    using T = typename Collection::value_type;
    using S = argument_storage_t<T>;
    return TryGetRepeatedArgumentImpl<S>(result, name);
  }

  DLL_PUBLIC inline InOutDeviceDesc& MutableInput(int idx) {
    DALI_ENFORCE_VALID_INDEX(idx, NumInput());
    return inputs_[idx];
  }

  DLL_PUBLIC inline InOutDeviceDesc& MutableOutput(int idx) {
    DALI_ENFORCE_VALID_INDEX(idx, NumOutput());
    return outputs_[idx];
  }

  DLL_PUBLIC string ToString() const {
    string ret;
    ret += "OpSpec for " + SchemaName() + ":\n  Inputs:\n";
    for (size_t i = 0; i < inputs_.size(); ++i) {
      ret += "    " + Input(i) + "\n";
    }
    ret += "  Outputs:\n";
    for (size_t i = 0; i < outputs_.size(); ++i) {
      ret += "    " + Output(i) + "\n";
    }
    ret += "  Arguments:\n";
    for (auto& a : arguments_) {
      ret += "    ";
      ret += a->ToString();
      ret += "\n";
    }
    return ret;
  }

 private:
  template <typename T, typename S>
  inline T GetArgumentImpl(const string &name, const ArgumentWorkspace *ws, Index idx) const;

  /**
   * @brief Check if the ArgumentInput of given shape can be used with GetArgument(),
   *        representing a batch of scalars
   *
   * @argument should_throw whether this function should throw an error if the shape doesn't match
   * @return true iff the shape is allowed to be used as Argument
   */
  bool CheckScalarArgumentShape(const TensorListShape<> &shape, int batch_size,
                          const std::string &name, bool should_throw = false) const {
    DALI_ENFORCE(is_uniform(shape),
                 "Arguments should be passed as uniform TensorLists. Argument \"" + name +
                     "\" is not uniform. To access non-uniform argument inputs use "
                     "ArgumentWorkspace::ArgumentInput method directly.");

    bool valid_shape = true;
    for (int i = 0; i < shape.num_samples() && valid_shape; i++) {
      valid_shape = volume(shape[i]) == 1 || shape[i].empty();
    }

    if (should_throw) {
      DALI_ENFORCE(
          valid_shape,
          make_string(
              "Unexpected shape of argument \"", name, "\". Expected batch of ", batch_size,
              " scalars or a batch of tensors containing one element per sample. Got:\n", shape));
    }
    return valid_shape;
  }

  template <typename T, typename S>
  inline bool TryGetArgumentImpl(T &result,
                                 const string &name,
                                 const ArgumentWorkspace *ws,
                                 Index idx) const;

  template <typename T, typename S>
  inline std::vector<T> GetRepeatedArgumentImpl(const string &name) const;

  template <typename S, typename C>
  inline bool TryGetRepeatedArgumentImpl(C &result, const string &name) const;

  string schema_name_;
  const OpSchema *schema_ = nullptr;

  // the list of arguments, in addition order
  std::vector<std::shared_ptr<Argument>> arguments_;
  // maps names to argument indices
  std::unordered_map<string, int> argument_idxs_;

  // argument input names and indices in addition order
  std::vector<std::pair<string, int>> argument_inputs_;
  // maps argument names to input indices
  std::unordered_map<string, int> argument_input_idxs_;

  // Regular arguments that were already set through renamed deprecated arguments
  // Maps regular_argument -> deprecated_argument
  std::map<std::string, std::string> set_through_deprecated_arguments_;

  std::map<InOutDeviceDesc, int> output_name_idx_;
  vector<InOutDeviceDesc> inputs_, outputs_;
};


template <typename T, typename S>
inline T OpSpec::GetArgumentImpl(
      const string &name,
      const ArgumentWorkspace *ws,
      Index idx) const {
  // Search for the argument in tensor arguments first
  if (this->HasTensorArgument(name)) {
    DALI_ENFORCE(ws != nullptr, "Tensor value is unexpected for argument \"" + name + "\".");
    const auto &value = ws->ArgumentInput(name);
    CheckScalarArgumentShape(value.shape(), GetArgument<int>("max_batch_size"), name, true);
    DALI_ENFORCE(IsType<T>(value.type()), make_string(
        "Unexpected type of argument \"", name, "\". Expected ",
        TypeTable::GetTypeName<T>(), " and got ", value.type()));
    return static_cast<T>(value.tensor<T>(idx)[0]);
  }
  // Search for the argument locally
  auto arg_it = argument_idxs_.find(name);
  if (arg_it != argument_idxs_.end()) {
    // Found locally - return
    Argument &arg = *arguments_[arg_it->second];
    return static_cast<T>(arg.Get<S>());
  } else {
    // Argument wasn't present locally, get the default from the associated schema
    const OpSchema& schema = GetSchema();
    return static_cast<T>(schema.GetDefaultValueForArgument<S>(name));
  }
}

template <typename T, typename S>
inline bool OpSpec::TryGetArgumentImpl(
      T &result,
      const string &name,
      const ArgumentWorkspace *ws,
      Index idx) const {
  // Search for the argument in tensor arguments first
  if (this->HasTensorArgument(name)) {
    if (ws == nullptr)
      return false;
    const auto& value = ws->ArgumentInput(name);
    if (!CheckScalarArgumentShape(value.shape(), GetArgument<int>("max_batch_size"), name, false)) {
      return false;
    }
    if (!IsType<T>(value.type()))
      return false;
    result = value.tensor<T>(idx)[0];
    return true;
  }
  // Search for the argument locally
  auto arg_it = argument_idxs_.find(name);
  const OpSchema& schema = GetSchema();
  if (arg_it != argument_idxs_.end()) {
    // Found locally - return
    Argument &arg = *arguments_[arg_it->second];
    if (arg.IsType<S>()) {
      result = static_cast<T>(arg.Get<S>());
      return true;
    }
  } else if (schema.HasArgument(name, true) && schema.HasArgumentDefaultValue(name)) {
    // Argument wasn't present locally, get the default from the associated schema if any
    auto schema_val = schema.FindDefaultValue(name);
    using VT = const ValueInst<S>;
    if (VT *vt = dynamic_cast<VT *>(schema_val.second)) {
      result = static_cast<T>(vt->Get());
      return true;
    }
  }
  return false;
}


template <typename T, typename S>
inline std::vector<T> OpSpec::GetRepeatedArgumentImpl(const string &name) const {
  using V = std::vector<S>;
  // Search for the argument locally
  auto arg_it = argument_idxs_.find(name);
  if (arg_it != argument_idxs_.end()) {
    // Found locally - return
    Argument &arg = *arguments_[arg_it->second];
    return detail::convert_vector<T>(arg.Get<V>());
  } else {
    // Argument wasn't present locally, get the default from the associated schema
    const OpSchema& schema = GetSchema();
    return detail::convert_vector<T>(schema.GetDefaultValueForArgument<V>(name));
  }
}

template <typename S, typename C>
inline bool OpSpec::TryGetRepeatedArgumentImpl(C &result, const string &name) const {
  using V = std::vector<S>;
  // Search for the argument locally
  auto arg_it = argument_idxs_.find(name);
  const OpSchema& schema = GetSchema();
  if (arg_it != argument_idxs_.end()) {
    // Found locally - return
    Argument &arg = *arguments_[arg_it->second];
    if (arg.IsType<V>()) {
      detail::copy_vector(result, arg.Get<V>());
      return true;
    }
  } else if (schema.HasArgument(name, true) && schema.HasArgumentDefaultValue(name)) {
    // Argument wasn't present locally, get the default from the associated schema if any
    auto schema_val = schema.FindDefaultValue(name);
    using VT = const ValueInst<V>;
    if (VT *vt = dynamic_cast<VT *>(schema_val.second)) {
      detail::copy_vector(result, vt->Get());
      return true;
    }
  }
  return false;
}

}  // namespace dali

#endif  // DALI_PIPELINE_OPERATOR_OP_SPEC_H_
