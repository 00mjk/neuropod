//
// Uber, Inc. (c) 2018
//

#include "python_bridge.hh"

#include "neuropod/bindings/python_bindings.hh"

#include <exception>
#include <sstream>
#include <vector>

#include <dlfcn.h>
#include <stdlib.h>

namespace neuropod
{

namespace
{

void set_python_path(const std::vector<std::string> &paths_to_add)
{
    std::stringstream python_path;

    for (const auto &dir : paths_to_add)
    {
        python_path << dir << ":";
    }

    if (const char *existing = std::getenv("PYTHONPATH"))
    {
        python_path << existing;
    }

    // Overwrite the existing PYTHONPATH with the new one
    setenv("PYTHONPATH", python_path.str().c_str(), 1);
}

// Initialize python if necessary and make sure we don't lock the GIL
std::unique_ptr<py::gil_scoped_release> maybe_initialize()
{
    if (Py_IsInitialized())
    {
        return nullptr;
    }

#ifndef __APPLE__
// This binary is already linked against `libpython`; the dlopen just
// promotes it to RTLD_GLOBAL.
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define PYTHON_LIB_NAME "libpython" STR(PYTHON_VERSION) ".so"
    void *libpython = dlopen(PYTHON_LIB_NAME, RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);

    if (libpython == nullptr)
    {
        NEUROPOD_ERROR("Failed to promote libpython to RTLD_GLOBAL. Error from dlopen: " << dlerror());
    }
#endif

    // If we have a virtualenv, use it
    if (auto venv_path = std::getenv("VIRTUAL_ENV"))
    {
        setenv("PYTHONHOME", venv_path, true);
    }

    // Start the interpreter
    py::initialize_interpreter();

    // TODO: shutdown the interpreter once we know that there are no more python objects left
    // atexit(py::finalize_interpreter);
    return stdx::make_unique<py::gil_scoped_release>();
}

// Handle interpreter startup and shutdown
// If we initialized the interpreter, make sure we don't have a lock on the GIL by storing a
// py::gil_scoped_release
static auto gil_release = maybe_initialize();

} // namespace

PythonBridge::PythonBridge(const std::string &             neuropod_path,
                           const RuntimeOptions &          options,
                           const std::vector<std::string> &python_path_additions)
    : NeuropodBackendWithDefaultAllocator<TestNeuropodTensor>(neuropod_path)
{
    // Modify PYTHONPATH
    set_python_path(python_path_additions);

    // Acquire the GIL
    py::gil_scoped_acquire gil;

    // Get the neuropod loader
    py::object load_neuropod = py::module::import("neuropod.loader").attr("load_neuropod");

    // Converts from unicode to ascii for python 3 string arrays
    maybe_convert_bindings_types_ = stdx::make_unique<py::object>(
        py::module::import("neuropod.utils.dtype_utils").attr("maybe_convert_bindings_types"));

    // Make sure that the model is local
    // Note: we could also delegate this to the python implementation
    const auto local_path = loader_->ensure_local();

    // Load the neuropod and save a reference to it
    neuropod_ = stdx::make_unique<py::object>(load_neuropod(local_path));
}

PythonBridge::~PythonBridge()
{
    // Acquire the GIL
    py::gil_scoped_acquire gil;

    // Delete the stored objects
    maybe_convert_bindings_types_.reset();
    neuropod_.reset();
}

struct SealedPythonValueMap : public SealedValueMap
{
    std::unique_ptr<py::dict> out;
    SealedPythonValueMap() : out(stdx::make_unique<py::dict>()) {}
    ~SealedPythonValueMap()
    {
        // Acquire the GIL
        py::gil_scoped_acquire gil;

        out.reset();
    }

    void seal(const std::string &name, const std::shared_ptr<NeuropodValue> &item)
    {
        // Acquire the GIL
        py::gil_scoped_acquire gil;

        // Convert to numpy
        (*out)[name.c_str()] = tensor_to_numpy(std::dynamic_pointer_cast<NeuropodTensor>(item));
    }
};

std::unique_ptr<SealedValueMap> PythonBridge::get_sealed_map()
{
    return stdx::make_unique<SealedPythonValueMap>();
}

// Run inference
std::unique_ptr<NeuropodValueMap> PythonBridge::infer_internal(const SealedValueMap &inputs_orig)
{
    // Cast to the right type
    auto &inputs = static_cast<const SealedPythonValueMap &>(inputs_orig);

    // Acquire the GIL
    py::gil_scoped_acquire gil;

    // Convert to a py::dict
    auto &model_inputs = *inputs.out;

    // Run inference
    py::dict model_outputs_raw = neuropod_->attr("infer")(model_inputs).cast<py::dict>();

    // Postprocess for python 3
    py::dict model_outputs = (*maybe_convert_bindings_types_)(model_outputs_raw).cast<py::dict>();

    // Get the outputs
    auto outputs = from_numpy_dict(*get_tensor_allocator(), model_outputs);

    // We need a unique pointer
    return stdx::make_unique<NeuropodValueMap>(std::move(outputs));
}

REGISTER_NEUROPOD_BACKEND(PythonBridge, "python", "pytorch")

} // namespace neuropod
