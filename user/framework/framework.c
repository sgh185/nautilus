// pointer to the NK function table
// to be filled in by the loader
void * (**__nk_func_table)(); 

extern int main(void *, void **);

extern int _start();

int __nk_exec_entry(void *in, void **out, void * (**table)())
{
    __nk_func_table = table;
    
    return _start();
}
