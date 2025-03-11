/***************************************************************************//**
 * @note
 * Copyright (c) 2019-2025 MemryX Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************/

/***************************************************************************//**
 * as a python-c extension, always put these two lines in the very beginning
 *  - ref: https://docs.python.org/3/extending/extending.html
 ******************************************************************************/
#define PY_SSIZE_T_CLEAN // make "s#" use Py_ssize_t rather than int.
#include <Python.h>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <numpy/ndarrayobject.h>
#include <numpy/ndarraytypes.h>

// wraps all constants and functions within 'memx.h' to python
#include <memx/memx.h>

// has all the gbf convert stuff
#include "convert.h"

/***************************************************************************//**
 * constant wrapper
 ******************************************************************************/
const int _wrap_memx_fmap_format_float32 = MEMX_FMAP_FORMAT_FLOAT32;
const int _wrap_memx_fmap_format_raw = MEMX_FMAP_FORMAT_RAW;
const int _wrap_memx_fmap_format_gbf80 = MEMX_FMAP_FORMAT_GBF80;

const int _wrap_memx_download_type_from_buffer = MEMX_DOWNLOAD_TYPE_FROM_BUFFER;
const int _wrap_memx_download_type_wtmem = MEMX_DOWNLOAD_TYPE_WTMEM;
const int _wrap_memx_download_type_model = MEMX_DOWNLOAD_TYPE_MODEL;
const int _wrap_memx_download_type_wtmem_and_model = MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL;
const int _wrap_memx_download_type_wtmem_and_model_buffer = MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL_BUFFER;

const int _wrap_memx_model_max_number = MEMX_MODEL_MAX_NUMBER;
const int _wrap_memx_device_group_max_number = MEMX_DEVICE_GROUP_MAX_NUMBER;
const char *_wrap_memx_device_cascade_chip_gen = (MEMX_DEVICE_CASCADE == 30) ? "3.0" : "0.0";
const char *_wrap_memx_device_cascade_plus_chip_gen = (MEMX_DEVICE_CASCADE_PLUS == 31) ? "3.1" : "0.0";

const int _wrap_memx_mpu_group_config_one_group_four_mpus = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS;
const int _wrap_memx_mpu_group_config_two_group_two_mpus = MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS;
const int _wrap_memx_mpu_group_config_one_group_one_mpu = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_ONE_MPU;
const int _wrap_memx_mpu_group_config_one_group_three_mpus = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_THREE_MPUS;
const int _wrap_memx_mpu_group_config_one_group_two_mpus = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWO_MPUS;
const int _wrap_memx_mpu_group_config_one_group_eight_mpus = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_EIGHT_MPUS;
const int _wrap_memx_mpu_group_config_one_group_twelve_mpus = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWELVE_MPUS;
const int _wrap_memx_mpu_group_config_one_group_sixteen_mpus = MEMX_MPU_GROUP_CONFIG_ONE_GROUP_SIXTEEN_MPUS;

const int _wrap_memx_power_state_ps_normal  = MEMX_PS0;
const int _wrap_memx_power_state_ps_standby = MEMX_PS1;
const int _wrap_memx_power_state_ps_idle    = MEMX_PS2;
const int _wrap_memx_power_state_ps_sleep   = MEMX_PS3;

const int _wrap_memx_boot_mode_qspi   = MXMX_BOOT_MODE_QSPI;
const int _wrap_memx_boot_mode_usb    = MXMX_BOOT_MODE_USB;
const int _wrap_memx_boot_mode_pcie   = MXMX_BOOT_MODE_PCIE;
const int _wrap_memx_boot_mode_uart   = MXMX_BOOT_MODE_UART;

const int _wrap_memx_chip_version_a0    = MXMX_CHIP_VERSION_A0;
const int _wrap_memx_chip_version_a1    = MXMX_CHIP_VERSION_A1;

/***************************************************************************//**
 * function wrapper
 ******************************************************************************/
static PyObject* _wrap_memx_lock(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;

  if(!PyArg_ParseTuple(args, "b", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_lock(group_id);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_trylock(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;

  if(!PyArg_ParseTuple(args, "b", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_trylock(group_id);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_unlock(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;

  if(!PyArg_ParseTuple(args, "b", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_unlock(group_id);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_open(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  uint8_t group_id;
  float chip_gen;

  if(!PyArg_ParseTuple(args, "bbf", &model_id, &group_id, &chip_gen)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_open(model_id, group_id, chip_gen);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_close(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;

  if(!PyArg_ParseTuple(args, "b", &model_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_close(model_id);
    Py_END_ALLOW_THREADS
  }

  unused(args);
  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_operation(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  int cmd_id;
PyArrayObject* data;
  uint32_t size;

  if(!PyArg_ParseTuple(args, "bizl", &model_id, &cmd_id, &PyArray_Type, &data, &size)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_INCREF(data);
    Py_BEGIN_ALLOW_THREADS
    status = memx_operation(model_id, cmd_id, (void*)PyArray_DATA(data), size);
    Py_END_ALLOW_THREADS
    Py_DECREF(data);
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_reset_device(PyObject* self, PyObject* args)
{
  memx_status status;
  uint32_t tmp = 0;
  uint8_t model_id;

  if(!PyArg_ParseTuple(args, "b", &model_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_operation(model_id, MEMX_CMD_RESET_DEVICE, &tmp, 0);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("i", status);
  } else {
    return Py_BuildValue("i", status);
  }
}

static PyObject* _wrap_memx_get_manufacturer_id(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_MANUFACTURERID, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_fw_commit(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_FW_COMMIT, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_date_code(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_DATE_CODE, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_cold_warm_reboot_count(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_COLD_WARM_REBOOT_COUNT, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_warm_reboot_count(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_WARM_REBOOT_COUNT, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_kdriver_version(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  char value[8];

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_KDRIVER_VERSION, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("s", value);
  }
}

static PyObject* _wrap_memx_get_temperature(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_TEMPERATURE, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_thermal_state(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_THERMAL_STATE, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_thermal_threshold(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_THERMAL_THRESHOLD, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_frequency(PyObject* self, PyObject* args)
{
  memx_status status;
  uint64_t value;
  uint8_t group_id;
  uint8_t chip_id;

  if(!PyArg_ParseTuple(args, "Bb", &group_id, &chip_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, chip_id, OPCODE_GET_FREQUENCY, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_voltage(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_VOLTAGE, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_throughput(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  memx_throughput_information throughput;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_THROUGHPUT, &throughput);
    Py_END_ALLOW_THREADS
  }

  PyObject *tuple = PyTuple_New(2);
  PyTuple_SetItem(tuple, 0, PyLong_FromLong(status));

  PyObject *dict = PyDict_New();
  PyDict_SetItemString(dict, "igr_from_host_us", PyLong_FromUnsignedLong(throughput.igr_from_host_us));
  PyDict_SetItemString(dict, "igr_from_host_kb", PyLong_FromUnsignedLong(throughput.igr_from_host_kb));
  PyDict_SetItemString(dict, "igr_to_mpu_us", PyLong_FromUnsignedLong(throughput.igr_to_mpu_us));
  PyDict_SetItemString(dict, "igr_to_mpu_kb", PyLong_FromUnsignedLong(throughput.igr_to_mpu_kb));
  PyDict_SetItemString(dict, "egr_from_mpu_us", PyLong_FromUnsignedLong(throughput.egr_from_mpu_us));
  PyDict_SetItemString(dict, "egr_from_mpu_kb", PyLong_FromUnsignedLong(throughput.egr_from_mpu_kb));
  PyDict_SetItemString(dict, "egr_to_host_us", PyLong_FromUnsignedLong(throughput.egr_to_host_us));
  PyDict_SetItemString(dict, "egr_to_host_kb", PyLong_FromUnsignedLong(throughput.egr_to_host_kb));
  PyDict_SetItemString(dict, "kdrv_tx_us", PyLong_FromUnsignedLong(throughput.kdrv_tx_us));
  PyDict_SetItemString(dict, "kdrv_tx_kb", PyLong_FromUnsignedLong(throughput.kdrv_tx_kb));
  PyDict_SetItemString(dict, "kdrv_rx_us", PyLong_FromUnsignedLong(throughput.kdrv_rx_us));
  PyDict_SetItemString(dict, "kdrv_rx_kb", PyLong_FromUnsignedLong(throughput.kdrv_rx_kb));
  PyDict_SetItemString(dict, "udrv_write_us", PyLong_FromUnsignedLong(throughput.udrv_write_us));
  PyDict_SetItemString(dict, "udrv_write_kb", PyLong_FromUnsignedLong(throughput.udrv_write_kb));
  PyDict_SetItemString(dict, "udrv_read_us", PyLong_FromUnsignedLong(throughput.udrv_read_us));
  PyDict_SetItemString(dict, "udrv_read_kb", PyLong_FromUnsignedLong(throughput.udrv_read_kb));
  PyTuple_SetItem(tuple, 1, dict);

  unused(self);
  return tuple;
}

static PyObject* _wrap_memx_get_power(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_POWER, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_poweralert(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_POWER_ALERT, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_get_module_info(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;
  uint64_t value;

  if(!PyArg_ParseTuple(args, "B", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_feature(group_id, 0, OPCODE_GET_MODULE_INFORMATION, &value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("K", status);
  } else {
    return Py_BuildValue("K", value);
  }
}

static PyObject* _wrap_memx_set_frequency(PyObject* self, PyObject* args)
{
  memx_status status = MEMX_STATUS_OK;
  uint16_t value = 0;
  uint8_t group_id = 0;
  uint8_t chip_id = 0;

  if(!PyArg_ParseTuple(args, "bbh", &group_id, &chip_id, &value)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_feature(group_id, chip_id, OPCODE_SET_FREQUENCY, value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("K", status);
}

static PyObject* _wrap_memx_set_voltage(PyObject* self, PyObject* args)
{
  memx_status status = MEMX_STATUS_OK;
  uint16_t value = 0;
  uint8_t group_id = 0;
  uint8_t chip_id = 0;

  if(!PyArg_ParseTuple(args, "bh", &group_id, &value)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_feature(group_id, chip_id, OPCODE_SET_VOLTAGE, value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("K", status);
}

static PyObject* _wrap_memx_set_power_threshold(PyObject* self, PyObject* args)
{
  memx_status status = MEMX_STATUS_OK;
  uint16_t value = 0;
  uint8_t group_id = 0;
  uint8_t chip_id = 0;

  if(!PyArg_ParseTuple(args, "bh", &group_id, &value)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_feature(group_id, chip_id, OPCODE_SET_POWER_THRESHOLD, value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("K", status);
}

static PyObject* _wrap_memx_set_power_alert_frequency(PyObject* self, PyObject* args)
{
  memx_status status = MEMX_STATUS_OK;
  uint16_t value = 0;
  uint8_t group_id = 0;
  uint8_t chip_id = 0;

  if(!PyArg_ParseTuple(args, "bh", &group_id, &value)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_feature(group_id, chip_id, OPCODE_SET_POWER_ALERT_FREQUENCY, value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("K", status);
}

static PyObject* _wrap_memx_set_thermal_threshold(PyObject* self, PyObject* args)
{
  memx_status status = MEMX_STATUS_OK;
  uint16_t value = 0;
  uint8_t group_id = 0;
  uint8_t chip_id = 0;

  if(!PyArg_ParseTuple(args, "bh", &group_id, &value)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_feature(group_id, chip_id, OPCODE_SET_THERMAL_THRESHOLD, value);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("K", status);
}

static PyObject* _wrap_memx_chip_count(PyObject* self, PyObject* args)
{
  memx_status status;
  uint32_t count = 0;
  uint8_t device_id;

  if(!PyArg_ParseTuple(args, "b", &device_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_total_chip_count(device_id, (uint8_t*) &count);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  if(status) {
    // non-zero error
    return Py_BuildValue("i", status);
  } else {
    return Py_BuildValue("i", count);
  }
}

static PyObject* _wrap_memx_config_mpu_group(PyObject* self, PyObject* args)
{
  memx_status status;

  uint8_t device_id = 0;
  uint8_t mpu_config = 0;

  if(!PyArg_ParseTuple(args, "bb", &device_id, &mpu_config)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_config_mpu_group(device_id, mpu_config);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_download_model_config(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  const char* file_path;
  uint8_t model_idx;

  if(!PyArg_ParseTuple(args, "bsb", &model_id, &file_path, &model_idx)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_download_model_config(model_id, file_path, model_idx);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_download_model_wtmem(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  const char* file_path;

  if(!PyArg_ParseTuple(args, "bs", &model_id, &file_path)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_download_model_wtmem(model_id, file_path);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_download_model(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  const char* file_path;
  uint8_t model_idx = 0;
  int type = 3;

  if(!PyArg_ParseTuple(args, "bs|bi", &model_id, &file_path, &model_idx, &type)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_download_model(model_id, file_path, model_idx, type);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_download_firmware(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id = 0;
  uint8_t type     = 0;
  const char* data = NULL;

  if(!PyArg_ParseTuple(args, "bsb", &group_id, &data, &type)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_download_firmware(group_id, data, type);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_set_stream_enable(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  int wait;

  if(!PyArg_ParseTuple(args, "bi", &model_id, &wait)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_stream_enable(model_id, wait);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_set_stream_disable(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  int wait;

  if(!PyArg_ParseTuple(args, "bi", &model_id, &wait)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_stream_disable(model_id, wait);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_set_ifmap_queue_size(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  int size;

  if(!PyArg_ParseTuple(args, "bi", &model_id, &size)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_ifmap_queue_size(model_id, size);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_set_ofmap_queue_size(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  int size;

  if(!PyArg_ParseTuple(args, "bi", &model_id, &size)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_ofmap_queue_size(model_id, size);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_get_ifmap_size(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  uint8_t flow_id;
  int height;
  int width;
  int z;
  int channel_number;
  int format;

  if(!PyArg_ParseTuple(args, "bb", &model_id, &flow_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_ifmap_size(model_id, flow_id, &height, &width, &z, &channel_number, &format);
    Py_END_ALLOW_THREADS
  }

  unused(status);
  unused(self);
  return Py_BuildValue("(iiii)", height, width, z, channel_number);
}

static PyObject* _wrap_memx_get_ifmap_range_convert(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  uint8_t flow_id;
  int enable;
  float shift;
  float scale;

  if(!PyArg_ParseTuple(args, "bb", &model_id, &flow_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_ifmap_range_convert(model_id, flow_id, &enable, &shift, &scale);
    Py_END_ALLOW_THREADS
  }

  unused(status);
  unused(self);
  return Py_BuildValue("(iff)", enable, shift, scale);
}

static PyObject* _wrap_memx_get_ofmap_size(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  uint8_t flow_id;
  int height;
  int width;
  int z;
  int channel_number;
  int format;

  if(!PyArg_ParseTuple(args, "bb", &model_id, &flow_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_get_ofmap_size(model_id, flow_id, &height, &width, &z, &channel_number, &format);
    Py_END_ALLOW_THREADS
  }

  unused(status);
  unused(self);
  return Py_BuildValue("(iiii)", height, width, z, channel_number);
}

static PyObject* _wrap_memx_get_ofmap_hpoc(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  uint8_t flow_id;
  int hpoc_size;
  int* _hpoc_indexes = NULL;
  PyArrayObject* hpoc_indexes = NULL;

  if(!PyArg_ParseTuple(args, "bb", &model_id, &flow_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    status = memx_get_ofmap_hpoc(model_id, flow_id, &hpoc_size, &_hpoc_indexes);
    if(memx_status_no_error(status)&&(hpoc_size > 0)) {
      npy_intp dims[] = {hpoc_size};
      hpoc_indexes = (PyArrayObject*)PyArray_SimpleNew(1, dims, NPY_INT32);
      if(hpoc_indexes == NULL)
        status = MEMX_STATUS_OTHERS;
    }
    if(memx_status_no_error(status)&&(hpoc_size > 0)) {
      for(int i=0; i<hpoc_size; ++i) {
        *((int*)PyArray_DATA(hpoc_indexes)+i) = *(_hpoc_indexes+i);
      }
    }
  }

  unused(self);
  if(memx_status_error(status))
    return Py_BuildValue("");
  return (PyObject*)hpoc_indexes;
}

static PyObject* _wrap_memx_stream_ifmap(PyObject* self, PyObject* args, PyObject *kwargs)
{
  memx_status status;
  uint8_t model_id; // mandatory
  uint8_t flow_id; // mandatory
  PyArrayObject* ifmap; // mandatory
  int timeout = 0; // optional = 0 (infinite)

  static char *kwlist[] = {"model_id","flow_id","ifmap","timeout",NULL};
  if(!PyArg_ParseTupleAndKeywords(args, kwargs, "bbO!|i", kwlist, &model_id, &flow_id, &PyArray_Type, &ifmap, &timeout)) {
    PyErr_BadArgument();
    return NULL;
  }

  uint8_t chip_gen = 0;
  status = memx_get_chip_gen(model_id, &chip_gen);
  if(status != MEMX_STATUS_OK){ return Py_BuildValue("i", status); }

  uint8_t *formatted_data = NULL;

  {
    if (chip_gen == MEMX_DEVICE_CASCADE){
        // no convert
        Py_INCREF(ifmap);
        Py_BEGIN_ALLOW_THREADS
        status = memx_stream_ifmap(model_id, flow_id, (void*)PyArray_DATA(ifmap), timeout);
        Py_END_ALLOW_THREADS
        Py_DECREF(ifmap);
    }
    else if (chip_gen == MEMX_DEVICE_CASCADE_PLUS) {


        // get the info
        int height, width, z, num_ch, format, tensor_size;
        status = memx_get_ifmap_size(model_id, flow_id, &height, &width, &z, &num_ch, &format);
        tensor_size = height*width*z*num_ch;

        if(format == MEMX_FMAP_FORMAT_BF16){
            // BF convert
            int fmt_size = tensor_size * 2;
            if(tensor_size % 2)
                fmt_size += 2;
            formatted_data = malloc(fmt_size);
            memset(formatted_data, 0, fmt_size);

            Py_INCREF(ifmap);
            Py_BEGIN_ALLOW_THREADS
            convert_bf16( (void*)PyArray_DATA(ifmap), formatted_data, tensor_size );

            // send
            status = memx_stream_ifmap(model_id, flow_id, formatted_data, timeout);

            // free
            free(formatted_data);
            formatted_data = NULL;

            Py_END_ALLOW_THREADS
            Py_DECREF(ifmap);
        } else if (format == MEMX_FMAP_FORMAT_GBF80) {
            // GBF convert
            int num_xyz_pixels = (tensor_size / num_ch);
            int num_gbf_per_pixel = (num_ch / 8) + ( ((num_ch%8)!=0) ? 1 : 0);
            int fmt_size = num_xyz_pixels * num_gbf_per_pixel * 10;
            formatted_data = malloc(fmt_size);
            memset(formatted_data, 0, fmt_size);

            Py_INCREF(ifmap);
            Py_BEGIN_ALLOW_THREADS
            convert_gbf( (void*)PyArray_DATA(ifmap), formatted_data, tensor_size, num_ch );

            // send
            status = memx_stream_ifmap(model_id, flow_id, formatted_data, timeout);

            // free
            free(formatted_data);
            formatted_data = NULL;

            Py_END_ALLOW_THREADS
            Py_DECREF(ifmap);
        } else if (format == MEMX_FMAP_FORMAT_GBF80_ROW_PAD) {
            // GBF row pad alloc
            int num_gbf_per_pixel = (num_ch / 8) + ( ((num_ch%8)!=0) ? 1 : 0);
            int fmt_size = height*((width * z * num_gbf_per_pixel * 10 + 3) &~0x3);
            formatted_data = malloc(fmt_size);
            memset(formatted_data, 0, fmt_size);

            Py_INCREF(ifmap);
            Py_BEGIN_ALLOW_THREADS
            convert_gbf_row_pad( (void*)PyArray_DATA(ifmap), formatted_data, height, width, z, num_ch);

            // send
            status = memx_stream_ifmap(model_id, flow_id, formatted_data, timeout);

            // free
            free(formatted_data);
            formatted_data = NULL;

            Py_END_ALLOW_THREADS
            Py_DECREF(ifmap);
        } else {
            // don't convert anything else
            Py_INCREF(ifmap);
            Py_BEGIN_ALLOW_THREADS
            status = memx_stream_ifmap(model_id, flow_id, (void*)PyArray_DATA(ifmap), timeout);
            Py_END_ALLOW_THREADS
            Py_DECREF(ifmap);
        }
    }
    else //unexpected chip_gen case
    {
        status = MEMX_STATUS_OTHERS;
    }
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_stream_ifmap_push(PyObject* self, PyObject* args, PyObject *kwargs)
{
  return _wrap_memx_stream_ifmap(self, args, kwargs);
}

static PyObject* _wrap_memx_stream_ofmap(PyObject* self, PyObject* args, PyObject *kwargs)
{
  memx_status status; // internal-use
  uint8_t model_id; // mandatory
  uint8_t flow_id; // mandatory
  PyArrayObject* ofmap; // mandatory
  int timeout = 0; // optional = 0 (infinite)
  int hpoc_size = 0;
  int* hpoc_indexes = NULL;

  static char *kwlist[] = {"model_id","flow_id","ofmap","timeout",NULL};
  if(!PyArg_ParseTupleAndKeywords(args, kwargs, "bbO!|i", kwlist, &model_id, &flow_id, &PyArray_Type, &ofmap, &timeout)) {
    PyErr_BadArgument();
    return NULL;
  }

  uint8_t chip_gen = 0;
  status = memx_get_chip_gen(model_id, &chip_gen);
  if(status != MEMX_STATUS_OK){ return Py_BuildValue("i", status); }

  uint8_t *formatted_data = NULL;

  {
    if (chip_gen == MEMX_DEVICE_CASCADE) {
        // no convert
        Py_INCREF(ofmap);
        Py_BEGIN_ALLOW_THREADS
        status = memx_stream_ofmap(model_id, flow_id, (void*)PyArray_DATA(ofmap), timeout);
        Py_END_ALLOW_THREADS
        Py_DECREF(ofmap);
    }
    else if (chip_gen == MEMX_DEVICE_CASCADE_PLUS) {

        // get the info
        int height, width, z, num_ch, format, tensor_size;
        status = memx_get_ofmap_size(model_id, flow_id, &height, &width, &z, &num_ch, &format);
        tensor_size = height*width*z*num_ch;

        if(format == MEMX_FMAP_FORMAT_BF16){
            // BF alloc
            int fmt_size = tensor_size * 2;
            if(tensor_size % 2)
                fmt_size += 2;
            formatted_data = malloc(fmt_size);
            memset(formatted_data, 0, fmt_size);

            // recv
            Py_INCREF(ofmap);
            Py_BEGIN_ALLOW_THREADS
            status = memx_stream_ofmap(model_id, flow_id, formatted_data, 0);

            // BF unconvert
            unconvert_bf16(formatted_data, (void*)PyArray_DATA(ofmap), tensor_size);

            // free
            free(formatted_data);
            formatted_data = NULL;

            Py_END_ALLOW_THREADS
            Py_DECREF(ofmap);

        } else if(format == MEMX_FMAP_FORMAT_GBF80){
            int num_gbf_ch = num_ch;
            status = memx_get_ofmap_hpoc(model_id, flow_id, &hpoc_size, &hpoc_indexes);
            // adjust gbf size for hpoc
            if(memx_status_no_error(status) && hpoc_size != 0) {
              num_gbf_ch += hpoc_size;
            }

            // GBF alloc
            int num_xyz_pixels = (tensor_size / num_ch);
            int num_gbf_per_pixel = (num_gbf_ch/8) + (((num_gbf_ch%8)!=0) ? 1 : 0);
            int fmt_size = num_xyz_pixels * num_gbf_per_pixel * 10;
            formatted_data = malloc(fmt_size);
            memset(formatted_data, 0, fmt_size);

            // recv
            Py_INCREF(ofmap);
            Py_BEGIN_ALLOW_THREADS
            status = memx_stream_ofmap(model_id, flow_id, formatted_data, timeout);

            // GBF unconvert
            if (hpoc_size != 0 && hpoc_indexes) {
              unconvert_gbf_hpoc(formatted_data, (void*)PyArray_DATA(ofmap), height, width, z, num_ch, hpoc_size, hpoc_indexes, 0);
            } else {
              unconvert_gbf(formatted_data, (void*)PyArray_DATA(ofmap), tensor_size, num_ch);
            }

            // free
            free(formatted_data);
            formatted_data = NULL;

            Py_END_ALLOW_THREADS
            Py_DECREF(ofmap);

        } else if(format == MEMX_FMAP_FORMAT_GBF80_ROW_PAD){
            int num_gbf_ch = num_ch;
            status = memx_get_ofmap_hpoc(model_id, flow_id, &hpoc_size, &hpoc_indexes);
            // adjust gbf size for hpoc
            if(memx_status_no_error(status) && hpoc_size != 0) {
              num_gbf_ch += hpoc_size;
            }

            // GBF row pad alloc
            int num_gbf_per_pixel = (num_gbf_ch/8) + (((num_gbf_ch%8)!=0) ? 1 : 0);
            int fmt_size = height*((width * z * num_gbf_per_pixel * 10 + 3) &~0x3);
            formatted_data = malloc(fmt_size);
            memset(formatted_data, 0, fmt_size);

            // recv
            Py_INCREF(ofmap);
            Py_BEGIN_ALLOW_THREADS
            status = memx_stream_ofmap(model_id, flow_id, formatted_data, timeout);
            // GBF unconvert
            if (hpoc_size != 0 && hpoc_indexes) {
              unconvert_gbf_hpoc(formatted_data, (void*)PyArray_DATA(ofmap), height, width, z, num_ch, hpoc_size, hpoc_indexes, 1);
            } else {
              unconvert_gbf_row_pad(formatted_data, (void*)PyArray_DATA(ofmap), height, width, z, num_ch);
            }

            // free
            free(formatted_data);
            formatted_data = NULL;

            Py_END_ALLOW_THREADS
            Py_DECREF(ofmap);

        } else {
            // don't convert anything else
            Py_INCREF(ofmap);
            Py_BEGIN_ALLOW_THREADS
            status = memx_stream_ofmap(model_id, flow_id, (void*)PyArray_DATA(ofmap), timeout);
            Py_END_ALLOW_THREADS
            Py_DECREF(ofmap);
       }
    }
    else //unexpected chip_gen case
    {
        status = MEMX_STATUS_OTHERS;
    }
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_stream_ofmap_pop(PyObject* self, PyObject* args, PyObject *kwargs)
{
  memx_status status; // internal-use
  uint8_t model_id; // mandatory
  uint8_t flow_id; // mandatory
  int timeout = 0; // optional = 0 (infinite)
  PyArrayObject* ofmap = NULL; // ndarry return on success

  static char *kwlist[] = {"model_id","flow_id","timeout", NULL};
  if(!PyArg_ParseTupleAndKeywords(args, kwargs, "bb|i", kwlist, &model_id, &flow_id, &timeout)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    // allocate memory space which will later be given to user
    int height, width, z, channel_number, format;
    status = memx_get_ofmap_size(model_id, flow_id, &height, &width, &z, &channel_number, &format);
    if(memx_status_no_error(status)) {
      npy_intp dims[4];
      if(format == MEMX_FMAP_FORMAT_FLOAT32) {
        dims[0] = height; dims[1] = width; dims[2] = z; dims[3] = channel_number;
        ofmap = (PyArrayObject*)PyArray_SimpleNew(3, dims, NPY_FLOAT32);
      }
      else if(format == MEMX_FMAP_FORMAT_GBF80) {
        dims[0] = height; dims[1] = width; dims[2] = z; dims[3] = ((channel_number + 7) >> 3) * 10;
        ofmap = (PyArrayObject*)PyArray_SimpleNew(3, dims, NPY_UINT8);
      }
      else {
        dims[0] = height; dims[1] = width; dims[2] = z; dims[3] = channel_number;
        ofmap = (PyArrayObject*)PyArray_SimpleNew(3, dims, NPY_UINT8);
      }
      if(ofmap == NULL)
        status = MEMX_STATUS_OTHERS;
    }
    // copy data from internal buffer to pyobject
    if(memx_status_no_error(status)) {
      Py_BEGIN_ALLOW_THREADS
      status = memx_stream_ofmap(model_id, flow_id, (void*)PyArray_DATA(ofmap), timeout);
      Py_END_ALLOW_THREADS
      if(memx_status_error(status))
        Py_DECREF(ofmap);
    }
  }

  unused(self);
  if(memx_status_error(status))
    return Py_BuildValue("");
  return (PyObject*)ofmap;
}

static PyObject* _wrap_memx_set_powerstate(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t model_id;
  uint8_t state;

  if(!PyArg_ParseTuple(args, "bb", &model_id, &state)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_set_powerstate(model_id, state);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_enter_device_deep_sleep(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;

  if(!PyArg_ParseTuple(args, "b", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_enter_device_deep_sleep(group_id);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

static PyObject* _wrap_memx_exit_device_deep_sleep(PyObject* self, PyObject* args)
{
  memx_status status;
  uint8_t group_id;

  if(!PyArg_ParseTuple(args, "b", &group_id)) {
    PyErr_BadArgument();
    return NULL;
  }
  {
    Py_BEGIN_ALLOW_THREADS
    status = memx_exit_device_deep_sleep(group_id);
    Py_END_ALLOW_THREADS
  }

  unused(self);
  return Py_BuildValue("i", status);
}

/***************************************************************************//**
 * module method
 ******************************************************************************/
static PyMethodDef MemxMethods[] = {
  // { name, callback, options, description }
  {"lock", (PyCFunction)_wrap_memx_lock, METH_VARARGS, NULL},
  {"trylock", (PyCFunction)_wrap_memx_trylock, METH_VARARGS, NULL},
  {"unlock", (PyCFunction)_wrap_memx_unlock, METH_VARARGS, NULL},
  {"open", (PyCFunction)_wrap_memx_open, METH_VARARGS, NULL},
  {"close", (PyCFunction)_wrap_memx_close, METH_VARARGS, NULL},
  {"operation", (PyCFunction)_wrap_memx_operation, METH_VARARGS, NULL},
  {"chip_count", (PyCFunction)_wrap_memx_chip_count, METH_VARARGS, NULL},
  {"config_mpu_group", (PyCFunction)_wrap_memx_config_mpu_group, METH_VARARGS, NULL},
  {"download_model_config", (PyCFunction)_wrap_memx_download_model_config, METH_VARARGS, NULL},
  {"download_model_wtmem", (PyCFunction)_wrap_memx_download_model_wtmem, METH_VARARGS, NULL},
  {"download", (PyCFunction)_wrap_memx_download_model, METH_VARARGS, NULL},
  {"update_firmware", (PyCFunction)_wrap_memx_download_firmware, METH_VARARGS, NULL},
  {"set_stream_enable", (PyCFunction)_wrap_memx_set_stream_enable, METH_VARARGS, NULL},
  {"set_stream_disable", (PyCFunction)_wrap_memx_set_stream_disable, METH_VARARGS, NULL},
  {"set_ifmap_queue_size", (PyCFunction)_wrap_memx_set_ifmap_queue_size, METH_VARARGS, NULL},
  {"set_ofmap_queue_size", (PyCFunction)_wrap_memx_set_ofmap_queue_size, METH_VARARGS, NULL},
  {"get_ifmap_size", (PyCFunction)_wrap_memx_get_ifmap_size, METH_VARARGS, NULL},
  {"get_ifmap_range_convert", (PyCFunction)_wrap_memx_get_ifmap_range_convert, METH_VARARGS, NULL},
  {"get_ofmap_size", (PyCFunction)_wrap_memx_get_ofmap_size, METH_VARARGS, NULL},
  {"get_ofmap_hpoc", (PyCFunction)_wrap_memx_get_ofmap_hpoc, METH_VARARGS, NULL},
  {"stream_ifmap", (PyCFunction)_wrap_memx_stream_ifmap, METH_VARARGS|METH_KEYWORDS, NULL},
  {"stream_ofmap", (PyCFunction)_wrap_memx_stream_ofmap, METH_VARARGS|METH_KEYWORDS, NULL},
  {"push", (PyCFunction)_wrap_memx_stream_ifmap_push, METH_VARARGS|METH_KEYWORDS, NULL},
  {"pop", (PyCFunction)_wrap_memx_stream_ofmap_pop, METH_VARARGS|METH_KEYWORDS, NULL},
  {"reset_device", (PyCFunction)_wrap_memx_reset_device, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_manufacturer_id", (PyCFunction)_wrap_memx_get_manufacturer_id, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_fw_commit", (PyCFunction)_wrap_memx_get_fw_commit, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_date_code", (PyCFunction)_wrap_memx_get_date_code, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_cold_warm_reboot_count", (PyCFunction)_wrap_memx_get_cold_warm_reboot_count, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_warm_reboot_count", (PyCFunction)_wrap_memx_get_warm_reboot_count, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_kdriver_version", (PyCFunction)_wrap_memx_get_kdriver_version, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_temperature", (PyCFunction)_wrap_memx_get_temperature, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_thermal_state", (PyCFunction)_wrap_memx_get_thermal_state, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_thermal_threshold", (PyCFunction)_wrap_memx_get_thermal_threshold, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_frequency", (PyCFunction)_wrap_memx_get_frequency, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_voltage", (PyCFunction)_wrap_memx_get_voltage, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_throughput", (PyCFunction)_wrap_memx_get_throughput, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_power", (PyCFunction)_wrap_memx_get_power, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_poweralert", (PyCFunction)_wrap_memx_get_poweralert, METH_VARARGS|METH_KEYWORDS, NULL},
  {"get_module_info", (PyCFunction)_wrap_memx_get_module_info, METH_VARARGS|METH_KEYWORDS, NULL},
  {"set_mpu_frequency", (PyCFunction)_wrap_memx_set_frequency, METH_VARARGS, NULL},
  {"set_mpu_voltage", (PyCFunction)_wrap_memx_set_voltage, METH_VARARGS, NULL},
  {"set_mpu_thermal_threshold", (PyCFunction)_wrap_memx_set_thermal_threshold, METH_VARARGS, NULL},
  {"set_powerstate", (PyCFunction)_wrap_memx_set_powerstate, METH_VARARGS, NULL},
  {"set_power_threshold", (PyCFunction)_wrap_memx_set_power_threshold, METH_VARARGS, NULL},
  {"set_power_alert_frequency", (PyCFunction)_wrap_memx_set_power_alert_frequency, METH_VARARGS, NULL},
  {"enter_device_deep_sleep", (PyCFunction)_wrap_memx_enter_device_deep_sleep, METH_VARARGS, NULL},
  {"exit_device_deep_sleep", (PyCFunction)_wrap_memx_exit_device_deep_sleep, METH_VARARGS, NULL},

  {NULL, NULL, 0, NULL} // Sentinel
};

/***************************************************************************//**
 * module
 ******************************************************************************/
static struct PyModuleDef MemxModule = {
  PyModuleDef_HEAD_INIT,
  "mxa", // name of module, which will be used as the name to import module from python
  NULL, // module documentation, may be NULL
  -1, // size of per-interpreter state of the module, or -1 if the module keeps state in global variables
  MemxMethods // module method declared above
};

/***************************************************************************//**
 * module init.
 *  - PyInit_name(), where 'name' is the name of the module
 *  - should be the only non-static item defined in the module file
 ******************************************************************************/
PyMODINIT_FUNC
PyInit_mxa(void)
{
  PyObject* module = PyModule_Create(&MemxModule);
  import_array(); // init. numpy array is required in the very beginning

  // wraps constant definition to module
  // renames constants here to make them short and easier to use within python
  PyModule_AddIntConstant(module, "float32", _wrap_memx_fmap_format_float32);
  PyModule_AddIntConstant(module, "uint8", _wrap_memx_fmap_format_raw);
  PyModule_AddIntConstant(module, "gbf80", _wrap_memx_fmap_format_gbf80);

  PyModule_AddIntConstant(module, "download_type_wtmem", _wrap_memx_download_type_wtmem);
  PyModule_AddIntConstant(module, "download_type_model", _wrap_memx_download_type_model);
  PyModule_AddIntConstant(module, "download_type_wtmem_and_model", _wrap_memx_download_type_wtmem_and_model);
  PyModule_AddIntConstant(module, "download_type_wtmem_and_model_buffer", _wrap_memx_download_type_wtmem_and_model_buffer);

  PyModule_AddIntConstant(module, "max_model_id", _wrap_memx_model_max_number);
  PyModule_AddIntConstant(module, "max_group_id", _wrap_memx_device_group_max_number);

  PyModule_AddStringConstant(module, "chip_gen_cascade", _wrap_memx_device_cascade_chip_gen);
  PyModule_AddStringConstant(module, "chip_gen_cascade_plus", _wrap_memx_device_cascade_plus_chip_gen);

  PyModule_AddIntConstant(module, "one_group_four_mpus", _wrap_memx_mpu_group_config_one_group_four_mpus);
  PyModule_AddIntConstant(module, "two_group_two_mpus", _wrap_memx_mpu_group_config_two_group_two_mpus);
  PyModule_AddIntConstant(module, "one_group_one_mpu",  _wrap_memx_mpu_group_config_one_group_one_mpu);
  PyModule_AddIntConstant(module, "one_group_three_mpus", _wrap_memx_mpu_group_config_one_group_three_mpus);
  PyModule_AddIntConstant(module, "one_group_two_mpus", _wrap_memx_mpu_group_config_one_group_two_mpus);
  PyModule_AddIntConstant(module, "one_group_eight_mpus", _wrap_memx_mpu_group_config_one_group_eight_mpus);
  PyModule_AddIntConstant(module, "one_group_twelve_mpus",_wrap_memx_mpu_group_config_one_group_twelve_mpus);
  PyModule_AddIntConstant(module, "one_group_sixteen_mpus", _wrap_memx_mpu_group_config_one_group_sixteen_mpus);

  PyModule_AddIntConstant(module, "memx_power_state_normal",  _wrap_memx_power_state_ps_normal);
  PyModule_AddIntConstant(module, "memx_power_state_standby", _wrap_memx_power_state_ps_standby);
  PyModule_AddIntConstant(module, "memx_power_state_idle",    _wrap_memx_power_state_ps_idle);
  PyModule_AddIntConstant(module, "memx_power_state_sleep",   _wrap_memx_power_state_ps_sleep);

  PyModule_AddIntConstant(module, "memx_boot_mode_qspi",  _wrap_memx_boot_mode_qspi);
  PyModule_AddIntConstant(module, "memx_boot_mode_usb",   _wrap_memx_boot_mode_usb);
  PyModule_AddIntConstant(module, "memx_boot_mode_pcie",  _wrap_memx_boot_mode_pcie);
  PyModule_AddIntConstant(module, "memx_boot_mode_uart",  _wrap_memx_boot_mode_uart);

  PyModule_AddIntConstant(module, "memx_chip_version_a0",   _wrap_memx_chip_version_a0);
  PyModule_AddIntConstant(module, "memx_chip_version_a1",   _wrap_memx_chip_version_a1);

  return module;
}
