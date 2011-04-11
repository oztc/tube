#ifndef _MODULE_H_
#define _MODULE_H_

#include "utils/cdecl.h"

BEGIN_DECLS

typedef struct _tube_module_t tube_module_t;

#ifdef __cplusplus
namespace tube {
typedef struct _tube_module_t Module;
}
#endif

struct _tube_module_t
{
    void*              handle;
    const char*        name;
    const char*        vendor;
    const char*        description;

    void (*on_load) (); /* for dynamic modules only */
    void (*on_initialize) ();
    void (*on_finalize) ();
};

tube_module_t* tube_module_load(const char* filename);
int            tube_module_load_dir(const char* dirname);

void           tube_module_register_module(tube_module_t* module);
void           tube_module_initialize_all();
void           tube_module_finalize_all();

#define EXPORT_MODULE_PTR __export_module_ptr
#define EXPORT_MODULE_PTR_NAME "__export_module_ptr"

#define EXPORT_MODULE(module_obj)                       \
    tube_module_t* EXPORT_MODULE_PTR = &(module_obj);   \

#define EXPORT_MODULE_STATIC(module_obj)                                \
    static void __static_module_register() __attribute__((constructor)); \
    static void __static_module_register() {                            \
        tube_module_register_module(&(module_obj));}                    \

END_DECLS

#endif /* _MODULE_H_ */
