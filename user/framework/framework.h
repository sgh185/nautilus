#include <stdint.h>

/*
 * Function signatures for instrumentation methods
 */ 
void nk_carat_instrument_global(void *ptr, uint64_t size, uint64_t global_ID) ;
void nk_carat_instrument_malloc(void *ptr, uint64_t size) ;
void nk_carat_instrument_calloc(void *ptr, uint64_t size_of_element, uint64_t num_elements) ;
void nk_carat_instrument_realloc(void *ptr, uint64_t size, void *old_address) ;
void nk_carat_instrument_free(void *ptr) ;
void nk_carat_instrument_escapes(void *ptr) ;
void _nk_carat_globals_compiler_target(void) ;


/*
 * HACK --- Link this function instead of the entire framework
 * such that the following happens:
 * a) All of the instrumentation methods' signatures persist in the
 *    benchmark bitcode generated
 * b) Current SVF/Noelle/PDG bugs analyzing framework code are 
 *    completely circumvented
 */ 
__attribute__((optnone, noinline, used, annotate("nocarat")))
static void _framework_persist_function_signatures(void)
{
    volatile int condition = 0;
    if (condition)
    {
        nk_carat_instrument_global(NULL, 0, 0);
        nk_carat_instrument_malloc(NULL, 0);
        nk_carat_instrument_calloc(NULL, 0, 0);
        nk_carat_instrument_realloc(NULL, 0, NULL);
        nk_carat_instrument_free(NULL);
        nk_carat_instrument_escapes(NULL);
        _nk_carat_globals_compiler_target();
    }


    return;
}
