#ifdef __x86_64__
extern void ___chkstk(void);
void __chkstk(void) { ___chkstk(); }
#endif
