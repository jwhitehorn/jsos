#include <value.h>
#include <vm.h>
#include <gc.h>
#include <exception.h>
#include "lib.h"
#include "console.h"

static VAL VM;
static VAL VM_prototype;

static js_vm_t* get_vm(js_vm_t* vm, VAL this)
{
    if(js_value_get_type(this) != JS_T_OBJECT || js_value_get_pointer(js_value_get_pointer(this)->object.class) != js_value_get_pointer(VM)) {
        js_throw_error(vm->lib.TypeError, "Can't call VM instance methods with non-VM as object");
    }
    return (js_vm_t*)js_value_get_pointer(this)->object.state;
}

static VAL wrap_vm(js_vm_t* vm)
{
    VAL obj = js_value_make_object(VM_prototype, VM);
    js_value_get_pointer(obj)->object.state = vm;
    return obj;
}

static VAL VM_construct(js_vm_t* vm, void* state, VAL this, uint32_t argc, VAL* argv)
{
    if(argc == 0 || (js_value_get_type(argv[0]) != JS_T_STRING && js_value_get_type(argv[0]) != JS_T_FUNCTION)) {
        js_throw_error(vm->lib.TypeError, "VM constructor expects either a function or image data");
    }
    js_vm_t* new_vm = js_vm_new();
    // @TODO make this go through a system call:
    console_init(new_vm);
    lib_vm_init(new_vm);
    js_image_t* image;
    uint32_t section;
    if(js_value_get_type(argv[0]) == JS_T_STRING) {
        // load new image from string
        js_string_t* str = js_to_js_string_t(argv[0]);
        image = js_image_parse(str->buff, str->length);
        if(!image) {
            js_throw_message(vm, "Couldn't parse image");
        }
        section = 0;
    } else {
        // run section in existing image
        js_function_t* fn = (js_function_t*)js_value_get_pointer(argv[0]);
        if(fn->is_native) {
            js_throw_error(vm->lib.TypeError, "Can't create VM from native function");
        }
        image = fn->js.image;
        section = fn->js.section;
    }
    js_vm_exec(new_vm, image, section, new_vm->global_scope, js_value_null(), 0, NULL);
    return wrap_vm(new_vm);
}

static VAL VM_self(js_vm_t* vm, void* state, VAL this, uint32_t argc, VAL* argv)
{
    return wrap_vm(vm);
}

static VAL VM_prototype_id(js_vm_t* vm, void* state, VAL this, uint32_t argc, VAL* argv)
{
    return js_value_make_double((uint32_t)get_vm(vm, this));
}

void lib_vm_init(js_vm_t* vm)
{
    VM = js_value_make_native_function(vm, NULL, js_cstring("VM"), NULL, VM_construct);
    js_gc_register_global(&VM, sizeof(VM));
    VM_prototype = js_object_get(VM, js_cstring("prototype"));
    js_gc_register_global(&VM_prototype, sizeof(VM_prototype));
    js_object_put(vm->global_scope->global_object, js_cstring("VM"), VM);
    
    // class methods:
    js_object_put(VM, js_cstring("self"), js_value_make_native_function(vm, NULL, js_cstring("self"), VM_self, NULL));
    
    // instance methods:
    js_object_put(VM_prototype, js_cstring("id"), js_value_make_native_function(vm, NULL, js_cstring("id"), VM_prototype_id, NULL));
}