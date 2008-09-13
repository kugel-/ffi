#include <stdbool.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "MemoryPointer.h"

typedef struct MemoryPointer {
    AbstractMemory memory;
    VALUE parent;
    bool autorelease;
    bool allocated;
} MemoryPointer;

static VALUE memptr_allocate(VALUE self, VALUE size, VALUE count, VALUE clear);
static void memptr_free(MemoryPointer* ptr);
static void memptr_mark(MemoryPointer* ptr);

VALUE rb_FFI_MemoryPointer_class;
static VALUE classMemoryPointer = Qnil;

VALUE
rb_FFI_MemoryPointer_new(caddr_t addr)
{
    MemoryPointer* p;
    
    p = ALLOC(MemoryPointer);
    memset(p, 0, sizeof(*p));
    p->memory.address = addr;
    p->memory.size = (size_t) ~0L;
    p->parent = Qnil;
    return Data_Wrap_Struct(classMemoryPointer, memptr_mark, memptr_free, p);
}

static VALUE
memptr_allocate(VALUE self, VALUE size, VALUE count, VALUE clear)
{
    MemoryPointer* p;
    
    p = ALLOC(MemoryPointer);
    memset(p, 0, sizeof(*p));
    p->autorelease = true;
    p->memory.size = NUM2ULONG(size) * NUM2ULONG(count);
    p->memory.address = malloc(p->memory.size);
    p->parent = Qnil;
    p->allocated = true;

    if (p->memory.address == NULL) {
        int size = p->memory.size;
        xfree(p);
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%u bytes", size);
    }
    if (TYPE(clear) == T_TRUE) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return Data_Wrap_Struct(classMemoryPointer, memptr_mark, memptr_free, p);
}

static VALUE
memptr_plus(VALUE self, VALUE offset)
{
    MemoryPointer* ptr = (MemoryPointer *) DATA_PTR(self);
    MemoryPointer* p;
    long off = NUM2LONG(offset);

    checkBounds(&ptr->memory, off, 1);
    p = ALLOC(MemoryPointer);
    memset(p, 0, sizeof(*p));
    p->memory.address = ptr->memory.address + off;;
    p->memory.size = ptr->memory.size - off;;
    p->parent = self;
    p->allocated = false;
    p->autorelease = true;
    return Data_Wrap_Struct(classMemoryPointer, memptr_mark, memptr_free, p);
}

static VALUE
memptr_inspect(VALUE self)
{
    MemoryPointer* ptr = (MemoryPointer *) DATA_PTR(self);
    char tmp[100];
    snprintf(tmp, sizeof(tmp), "Pointer: [address=%p]", ptr->memory.address);
    return rb_str_new2(tmp);
}


static void
memptr_free(MemoryPointer* ptr)
{
    if (ptr->autorelease && ptr->allocated) {
        free(ptr->memory.address);
    }
    xfree(ptr);

}
static void
memptr_mark(MemoryPointer* ptr)
{
    if (ptr->parent != Qnil) {
        rb_gc_mark(ptr->parent);
    }
}

void
rb_FFI_MemoryPointer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_MemoryPointer_class = classMemoryPointer = rb_define_class_under(moduleFFI, "MemoryPointer", rb_FFI_AbstractMemory_class);
    rb_define_singleton_method(classMemoryPointer, "__allocate", memptr_allocate, 3);
    rb_define_method(classMemoryPointer, "inspect", memptr_inspect, 0);
    rb_define_method(classMemoryPointer, "+", memptr_plus, 1);
}
