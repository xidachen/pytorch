#include <torch/csrc/lazy/core/tensor.h>

#include <torch/csrc/lazy/core/config.h>
#include <torch/csrc/lazy/core/helpers.h>
#include <torch/csrc/lazy/core/ir_dump_util.h>
#include <torch/csrc/lazy/core/lazy_graph_executor.h>
#include <torch/csrc/lazy/core/metrics.h>
#include <torch/csrc/lazy/core/tensor_util.h>
#include <torch/csrc/lazy/ts_backend/ops/cast.h>
#include <torch/csrc/lazy/ts_backend/ops/device_data.h>
#include <torch/csrc/lazy/ts_backend/ops/scalar.h>

namespace torch {
namespace lazy {

LazyTensor::Data::~Data() {
  LazyGraphExecutor::Get()->UnregisterTensor(this);
}

LazyTensor LazyTensor::Create(
    const at::Tensor& tensor,
    const BackendDevice& device) {
  CHECK_NE(tensor.device().type(), at::kLazy);
  LazyTensor xtensor(tensor, device);
  LazyGraphExecutor::Get()->RegisterTensor(xtensor.data_ptr());
  return xtensor;
}

LazyTensor LazyTensor::Create(Value ir_value, const BackendDevice& device) {
  LazyTensor xtensor(std::move(ir_value), device);
  LazyGraphExecutor::Get()->RegisterTensor(xtensor.data_ptr());
  return xtensor;
}

LazyTensor LazyTensor::Create(
    std::shared_ptr<LazyView> view,
    const BackendDevice& device) {
  LazyTensor xtensor(std::move(view), device);
  LazyGraphExecutor::Get()->RegisterTensor(xtensor.data_ptr());
  return xtensor;
}

LazyTensor LazyTensor::Create(BackendDataPtr handle) {
  LazyTensor xtensor(std::move(handle));
  LazyGraphExecutor::Get()->RegisterTensor(xtensor.data_ptr());
  return xtensor;
}

LazyTensor LazyTensor::Create(std::shared_ptr<Data> data) {
  return LazyTensor(std::move(data));
}

LazyTensor::LazyTensor(const at::Tensor& tensor, const BackendDevice& device)
    : data_(std::make_shared<Data>(tensor, device)) {}

LazyTensor::LazyTensor(BackendDataPtr handle)
    : data_(std::make_shared<Data>(handle, handle->device())) {}

LazyTensor::LazyTensor(Value ir_value, const BackendDevice& device)
    : data_(std::make_shared<Data>(std::move(ir_value), device)) {
  TryLimitGraphSize();
}

LazyTensor::LazyTensor(
    std::shared_ptr<LazyView> view,
    const BackendDevice& device)
    : data_(std::make_shared<Data>(std::move(view), device)) {}

LazyTensor::LazyTensor(std::shared_ptr<Data> data) : data_(std::move(data)) {}

LazyTensor::Data* LazyTensor::data() const {
  CHECK(data_ != nullptr) << "Trying to access a null cursor";
  return data_.get();
}

int64_t LazyTensor::size(int64_t dim) const {
  auto tensor_shape = shape();
  int rank = tensor_shape.Get().dim();
  int dim_index = GetCanonicalDimensionIndex(dim, rank);
  return tensor_shape.Get().size(dim_index);
}

at::ScalarType LazyTensor::dtype() const {
  return shape().Get().scalar_type();
}

MaybeRef<Shape> LazyTensor::shape() const {
  if (data()->view != nullptr) {
    return data()->view->shape();
  }
  if (data()->handle != nullptr) {
    return Shape(data()->handle->shape());
  }
  if (data()->ir_value) {
    // TODO(whc) remove shape from LazyTensor API too!
    return GetShapeFromTsValue(data()->ir_value);
  }
  CHECK(data()->tensor_data);
  return Shape(
      data()->tensor_data->scalar_type(),
      ToI64Vector(data()->tensor_data->sizes()));
}

const BackendDevice& LazyTensor::GetDevice() const {
  return data()->device;
}

int64_t LazyTensor::GetUniqueId() const {
  return data()->unique_id;
}

std::ptrdiff_t LazyTensor::GetViewAliasId() const {
  return data()->view != nullptr
      ? reinterpret_cast<std::ptrdiff_t>(data()->view->alias().get())
      : 0;
}

BackendDataPtr LazyTensor::GetDataHandle() {
  // Data can coexist with a view, but we need to check that the view did
  // not receive any updates before calling the current IR valid.
  bool up_to_date = true;
  Value ir_value;
  if (data()->view != nullptr) {
    bool updated = false;
    std::tie(ir_value, updated) = GetViewUpdate(data()->view);
    up_to_date = !updated;
  }
  if (up_to_date) {
    BackendDataPtr handle = CurrentDataHandle();
    if (handle != nullptr) {
      CHECK(handle->HasValue())
          << "Trying to access data while an async operation is in flight: "
          << Shape(handle->shape());
      return handle;
    }
  }
  if (ir_value) {
    // The view gave us an updated IR value. We usually do not have a valid IR
    // value field together with a view, but to allow code reuse in
    // ApplyPendingGraph() we temporarily set it here. The following call to
    // ApplyPendingGraph() will clear it.
    AssignIrValue(std::move(ir_value));
  }
  if (data()->ir_value) {
    ApplyPendingGraph();
  } else {
    CHECK(data()->tensor_data);
    data()->handle = TensorToDataHandle(*data()->tensor_data, GetDevice());
  }
  return data()->handle;
}

BackendDataPtr LazyTensor::CurrentDataHandle() const {
  return data()->handle;
}

void LazyTensor::SetDataHandle(BackendDataPtr handle) {
  SetDataHandle(std::move(handle), /*sync=*/true);
}

void LazyTensor::SetDataHandle(BackendDataPtr handle, bool sync) {
  data()->handle = std::move(handle);
  // Assigning a device data should always clear the IR node, to allow graph
  // trimming. A view cannot be reset though, unless we are at a step-end sync.
  AssignIrValue(Value());
  if (sync) {
    data()->view = nullptr;
    data()->tensor_data = c10::nullopt;
  }
}

void LazyTensor::SetIrValue(Value ir_value) {
  data()->handle = nullptr;
  data()->tensor_data = c10::nullopt;
  if (data()->view != nullptr) {
    // If we have an active view, and a SetIrValue() happens, it means we are
    // within an in-place execution context, and we need to update the view's
    // alias as well.
    data()->view = UpdateView(data()->view, std::move(ir_value));
    data()->generation += 1;
  } else {
    AssignIrValue(std::move(ir_value));
    TryLimitGraphSize();
  }
}

void LazyTensor::SetInPlaceIrValue(Value ir_value) {
  auto tensor_shape = shape();
  if (tensor_shape.Get().scalar_type() !=
      GetShapeFromTsValue(ir_value).scalar_type()) {
    ir_value = MakeNode<Cast>(ir_value, tensor_shape.Get().scalar_type());
  }
  SetIrValue(std::move(ir_value));
}

void LazyTensor::AssignIrValue(Value ir_value) const {
  data()->ir_value = std::move(ir_value);
  data()->generation += 1;
}

void LazyTensor::TryLimitGraphSize() {
  if (data()->ir_value &&
      LazyGraphExecutor::Get()->IncTrimCounter() %
              FLAGS_torch_lazy_trim_graph_check_frequency ==
          0) {
    size_t graph_size = Util::GetGraphSize({data()->ir_value.node.get()});
    if (graph_size > FLAGS_torch_lazy_trim_graph_size) {
      TORCH_LAZY_COUNTER("TrimIrGraph", 1);
      ApplyPendingGraph();
    }
  }
}

Value LazyTensor::GetIrValue() const {
  Value ir_value = CurrentIrValue();
  if (ir_value) {
    return ir_value;
  }
  BackendDataPtr handle = CurrentDataHandle();
  if (handle != nullptr) {
    // In case of tensor node, we do not clear the data when we set the IR
    // node. This because we want further calls to GetIrValue() to fetch the
    // same IR node, and not create new ones (even though the lowering context
    // will still collapse them all into a single parameter op). So the call
    // which wants the data will still find it, w/out having to fetch it via
    // a computation client from-server call.
    AssignIrValue(CreateTensorNode(handle, /*read_only=*/false));
    return data()->ir_value;
  }
  c10::optional<at::Tensor> tensor_data = CurrentTensorData();
  CHECK(tensor_data);
  AssignIrValue(GetIrValueForTensor(*tensor_data, GetDevice()));
  return data()->ir_value;
}

Value LazyTensor::CurrentIrValue() const {
  if (data()->view != nullptr) {
    return std::get<0>(GetViewUpdate(data()->view));
  }
  return data()->ir_value;
}

void LazyTensor::SetTensorData(at::Tensor tensor_data) {
  data()->tensor_data = std::move(tensor_data);
}

c10::optional<at::Tensor> LazyTensor::CurrentTensorData() const {
  if (data()->view != nullptr && !data()->view->IsUpToDate()) {
    return c10::nullopt;
  }
  return data()->tensor_data;
}

Value LazyTensor::GetIrValueForTensor(
    const at::Tensor& tensor,
    const BackendDevice& device) const {
  BackendDataPtr data;
  bool read_only = false;
  if (tensor.dim() == 0 && tensor.numel() == 1) {
    at::Scalar value = tensor.item();
    if (IsSpecialScalar(value)) {
      return MakeNode<Scalar>(std::move(value), tensor.scalar_type());
    }
    data = LazyGraphExecutor::Get()->GetDeviceData(tensor.cpu(), device);
    read_only = true;
  } else {
    TORCH_LAZY_TIMED("IrValueTensorToDataHandle");
    data = TensorToDataHandle(tensor, device);
  }
  return CreateTensorNode(std::move(data), read_only);
}

std::tuple<Value, bool> LazyTensor::GetViewUpdate(
    const std::shared_ptr<LazyView>& view) const {
  auto value_with_update = view->GetViewIrNode();
  if (std::get<1>(value_with_update)) {
    data()->handle = nullptr;
    data()->tensor_data = c10::nullopt;
  }
  return value_with_update;
}

std::shared_ptr<LazyView> LazyTensor::UpdateView(
    std::shared_ptr<LazyView> view,
    Value ir_value) const {
  if (GetShapeFromTsValue(ir_value).sizes() != view->shape().sizes()) {
    CHECK_EQ(GetShapeFromTsValue(ir_value).numel(), view->shape().numel());

    ViewInfo view_info(
        ViewInfo::Type::kReshape, GetShapeFromTsValue(ir_value), view->shape());
    view = view->CreateSubView(view_info.shape, view_info);
  }
  view->Update(std::move(ir_value));
  return view;
}

void LazyTensor::SetSubView(ViewInfo view_info) const {
  data()->view = data()->view->CreateSubView(view_info.shape, view_info);
  data()->generation += 1;
}

void LazyTensor::ModifyCurrentView(ViewInfo view_info) const {
  if (data()->view != nullptr) {
    SetSubView(view_info);
    return;
  }
  // This node is not a view. Since this function is meant to modify a view
  // in place, we need to turn this existing tensor into a view.
  Value ir_value = GetIrValue();
  std::shared_ptr<Alias> alias = std::make_shared<Alias>(ir_value);
  data()->view =
      std::make_shared<LazyView>(view_info.shape, alias, std::move(view_info));
  AssignIrValue(Value());
}

std::shared_ptr<LazyView> LazyTensor::CreateView(ViewInfo view_info) const {
  if (data()->view != nullptr) {
    return data()->view->CreateSubView(view_info.shape, view_info);
  }
  // This node is not a view, and creating a view forks the current node into
  // becoming one itself. This means creating an alias with the current IR
  // Node, and using the same alias for the created IR Node.
  Value ir_value = GetIrValue();
  std::shared_ptr<Alias> alias = std::make_shared<Alias>(ir_value);
  ViewInfo this_view_info(
      ViewInfo::Type::kNoOp,
      GetShapeFromTsValue(ir_value),
      GetShapeFromTsValue(ir_value));
  data()->view = std::make_shared<LazyView>(
      GetShapeFromTsValue(ir_value), alias, std::move(this_view_info));
  AssignIrValue(Value());
  return std::make_shared<LazyView>(view_info.shape, alias, view_info);
}

LazyTensor LazyTensor::CreateViewTensor(ViewInfo view_info) const {
  return Create(CreateView(std::move(view_info)), GetDevice());
}

at::Tensor LazyTensor::ToTensor(bool detached) {
  at::Tensor tensor;
  c10::optional<at::Tensor> tensor_data = CurrentTensorData();
  if (!tensor_data) {
    LazyGraphExecutor::Get()->DeviceBarrier(GetDevice());
    // The GetDataHandle() call will trigger an ApplyPendingGraph() if an IR
    // Node is available on the tensor.
    std::vector<at::Tensor> tensors =
        DataHandlesToTensors({GetDataHandle()}, dtype());
    tensor = std::move(tensors.front());
    if (!detached) {
      SetTensorData(tensor);
    }
  } else {
    tensor = *tensor_data;
    if (detached) {
      if (data()->ir_value || data()->handle != nullptr ||
          data()->view != nullptr) {
        // If we have other authoritive sources, just drop our reference and
        // transfer it to the caller.
        data()->tensor_data = c10::nullopt;
      } else {
        // Otherwise we need to make a copy to prevent the caller changing our
        // version.
        tensor = CopyTensor(tensor);
      }
    }
  }
  return tensor;
}

void LazyTensor::ShallowCopyTo(LazyTensor* dest) const {
  dest->SetIrValue(GetIrValue());
}

void LazyTensor::SetTensor(at::Tensor tensor) {
  SetTensorData(tensor);
  data()->view = nullptr;
  data()->handle = nullptr;
  AssignIrValue(Value());
}

void LazyTensor::UpdateFromTensor(at::Tensor tensor, bool sync) {
  if (sync) {
    at::Tensor typed_tensor = CopyTensor(tensor, dtype(), /*copy=*/false);
    SetIrValue(GetIrValueForTensor(typed_tensor, GetDevice()));
  } else {
    SetTensorData(tensor);
    data()->handle = nullptr;
    AssignIrValue(Value());
    if (data()->view != nullptr) {
      Value ir_value = GetIrValueForTensor(tensor, GetDevice());
      data()->view = UpdateView(data()->view, std::move(ir_value));
    }
  }
}

void LazyTensor::UpdateFromTensorOut(at::Tensor tensor) {
  if (data()->view != nullptr && shape().Get().numel() != tensor.numel()) {
    data()->view = nullptr;
  }
  UpdateFromTensor(std::move(tensor), /*sync=*/false);
}

void LazyTensor::UpdateFromTensorOut(const LazyTensor& tensor) {
  if (data()->view != nullptr &&
      shape().Get().numel() != tensor.shape().Get().numel()) {
    data()->view = nullptr;
  }
  SetIrValue(tensor.GetIrValue());
}

Value LazyTensor::CreateTensorNode(BackendDataPtr data, bool read_only) const {
  data->SetInfo(std::make_shared<LazyGraphExecutor::DeviceDataInfo>(
      GetUniqueId(), read_only));
  return MakeNode<DeviceData>(std::move(data));
}

std::vector<LazyTensor> LazyTensor::MakeOutputTensors(NodePtr node) const {
  std::vector<LazyTensor> tensors;
  tensors.reserve(node->num_outputs());
  for (size_t i = 0; i < node->num_outputs(); ++i) {
    tensors.push_back(Create(Value(node, i), GetDevice()));
  }
  return tensors;
}

LazyTensor LazyTensor::CopyTensorToDevice(const BackendDevice& device) {
  // TODO: This can be optimized.
  return Create(ToTensor(/*detached=*/true), device);
}

void LazyTensor::ApplyPendingGraph() {
  LazyGraphExecutor::Get()->DeviceBarrier(GetDevice());
  // This method is called to ensure that the tensor data is available on
  // device, so that a call to CurrentDataHandle() returns a valid pointer.
  if (CurrentDataHandle() == nullptr) {
    std::vector<LazyTensor> tensors({*this});
    LazyGraphExecutor::Get()->SyncTensorsGraph(
        &tensors,
        {},
        /*wait=*/true,
        /*sync_ltc_data=*/false);
  }
}

int64_t LazyTensor::GetNextTensorId() {
  static std::atomic<int64_t>* id_generator = new std::atomic<int64_t>(1);
  return id_generator->fetch_add(1);
}

} // namespace lazy
} // namespace torch
