
// =================================================================================================
// This file is part of the CLBlast project. Author(s):
//   Cedric Nugteren <www.cedricnugteren.nl>
//
// This file implements the Xcopy class (see the header for information about the class).
//
// =================================================================================================

#include "routines/level1/xcopy.hpp"

#include <string>
#include <vector>

namespace clblast {
// =================================================================================================

// Constructor: forwards to base class constructor
template <typename T>
Xcopy<T>::Xcopy(Queue& queue, EventPointer event, StatusCode& status, const std::string& name)
    : Routine(queue, event, name, {"Xaxpy"}, PrecisionValue<T>(), {},
              {
#include "../../kernels/level1/level1.opencl"
// (comment to prevent auto-re-ordering)
#include "../../kernels/level1/xcopy.opencl"
              },
              status) {
}

// =================================================================================================

// The main routine
template <typename T>
StatusCode Xcopy<T>::DoCopy(const size_t n, const Buffer<T>& x_buffer, const size_t x_offset, const size_t x_inc,
                            const Buffer<T>& y_buffer, const size_t y_offset, const size_t y_inc) {
  // Makes sure all dimensions are larger than zero
  if (n == 0) {
    return StatusCode::kInvalidDimension;
  }

  // Tests the vectors for validity
  auto status = TestVectorX(n, x_buffer, x_offset, x_inc);
  if (status != StatusCode::kSuccess) {
    return status;
  }

  status = TestVectorY(n, y_buffer, y_offset, y_inc);
  if (status != StatusCode::kSuccess) {
    return status;
  }

  // Determines whether or not the fast-version can be used
  bool use_fast_kernel = (x_offset == 0) && (x_inc == 1) && (y_offset == 0) && (y_inc == 1) &&
                         IsMultiple(n, db_["WGS"] * db_["WPT"] * db_["VW"]);

  // If possible, run the fast-version of the kernel
  auto kernel_name = (use_fast_kernel) ? "XcopyFast" : "Xcopy";

  // Retrieves the Xcopy kernel from the compiled binary
  auto kernel = Kernel(program_, kernel_name);

  // Sets the kernel arguments
  if (use_fast_kernel) {
    kernel.SetArgument(0, static_cast<int>(n));
    kernel.SetArgument(1, x_buffer());
    kernel.SetArgument(2, y_buffer());
  } else {
    kernel.SetArgument(0, static_cast<int>(n));
    kernel.SetArgument(1, x_buffer());
    kernel.SetArgument(2, static_cast<int>(x_offset));
    kernel.SetArgument(3, static_cast<int>(x_inc));
    kernel.SetArgument(4, y_buffer());
    kernel.SetArgument(5, static_cast<int>(y_offset));
    kernel.SetArgument(6, static_cast<int>(y_inc));
  }

  // Launches the kernel
  if (use_fast_kernel) {
    auto global = std::vector<size_t>{CeilDiv(n, db_["WPT"] * db_["VW"])};
    auto local = std::vector<size_t>{db_["WGS"]};
    RunKernel(kernel, queue_, device_, global, local, event_);
  } else {
    auto n_ceiled = Ceil(n, db_["WGS"] * db_["WPT"]);
    auto global = std::vector<size_t>{n_ceiled / db_["WPT"]};
    auto local = std::vector<size_t>{db_["WGS"]};
    RunKernel(kernel, queue_, device_, global, local, event_);
  }

  return StatusCode::kSuccess;
}

// =================================================================================================

// Compiles the templated class
template class Xcopy<half>;
template class Xcopy<float>;
template class Xcopy<double>;
template class Xcopy<float2>;
template class Xcopy<double2>;

// =================================================================================================
}  // namespace clblast
