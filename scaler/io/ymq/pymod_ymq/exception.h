#pragma once

#include <Python.h>
#include <structmember.h>

#include "abstract.h"
#include "descrobject.h"
#include "object.h"
#include "pyerrors.h"
#include "scaler/io/ymq/pymod_ymq/ymq.h"
#include "tupleobject.h"

typedef struct {
    PyException_HEAD;
    PyObject* code;
    PyObject* message;
} YMQException;

extern "C" {

static int YMQException_init(YMQException* self, PyObject* args, PyObject* kwds) {
    // check the args
    PyObject *code = nullptr, *message = nullptr;
    static const char* kwlist[] = {"code", "message", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", (char**)kwlist, &code, &message))
        return -1;

    // replace with PyType_GetModuleByDef(Py_TYPE(self), &ymq_module) in a newer Python version
    // https://docs.python.org/3/c-api/type.html#c.PyType_GetModuleByDef
    PyObject* module = PyType_GetModule(Py_TYPE(self));
    if (!module) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get module for Message type");
        return -1;
    }

    auto state = (YMQState*)PyModule_GetState(module);
    if (!state) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get module state");
        return -1;
    }

    if (!PyObject_IsInstance(code, state->PyErrorCodeType)) {
        PyErr_SetString(PyExc_TypeError, "expected code to be of type ErrorCode");
        return -1;
    }

    if (!PyUnicode_Check(message)) {
        PyErr_SetString(PyExc_TypeError, "expected message to be a string");
        return -1;
    }

    // delegate to the base class init
    return self->ob_base.ob_type->tp_base->tp_init((PyObject*)self, args, kwds);
}

static void YMQException_dealloc(YMQException* self) {
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* YMQException_code_getter(YMQException* self, void* Py_UNUSED(closure)) {
    return PyTuple_GetItem(self->args, 0);  // code is the first item in args
}

static PyObject* YMQException_message_getter(YMQException* self, void* Py_UNUSED(closure)) {
    return PyTuple_GetItem(self->args, 1);  // message is the second item in args
}
}

static PyGetSetDef YMQException_getset[] = {
    {"code", (getter)YMQException_code_getter, nullptr, PyDoc_STR("error code"), nullptr},
    {"message", (getter)YMQException_message_getter, nullptr, PyDoc_STR("error message"), nullptr},
    {nullptr}  // Sentinel
};

static PyType_Slot YMQException_slots[] = {
    {Py_tp_init, (void*)YMQException_init},
    {Py_tp_dealloc, (void*)YMQException_dealloc},
    {Py_tp_getset, (void*)YMQException_getset},
    {0, 0}};

static PyType_Spec YMQException_spec = {
    "ymq.YMQException", sizeof(YMQException), 0, Py_TPFLAGS_DEFAULT, YMQException_slots};

YMQException* YMQException_fromCoreException(YMQState* state, Error* error) {
    PyObject* code = PyLong_FromLong(error->_errorCode);

    if (!code)
        return nullptr;

    PyObject* message = PyUnicode_FromString(error->what());

    if (!message) {
        Py_DECREF(code);
        return nullptr;
    }

    PyObject* tuple = PyTuple_Pack(2, code, message);

    if (!tuple) {
        Py_DECREF(code);
        Py_DECREF(message);
        return nullptr;
    }

    auto exc = (YMQException*)PyObject_CallObject(state->PyExceptionType, tuple);

    Py_DECREF(code);
    Py_DECREF(message);
    Py_DECREF(tuple);

    return exc;
}
